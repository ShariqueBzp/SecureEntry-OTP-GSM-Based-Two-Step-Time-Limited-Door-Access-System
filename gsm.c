/*****************************************************************************
 * @file   gsm.c
 * @brief  Async SMS receiver (non-blocking) + blocking transmitters
 * @details B04 - at_wait reads whole lines, exact OK, fail on ERROR,
 *               15 s CMGS timeout, ESC on chevron timeout, s_tx_busy gate.
 *           B05 - deadlines every call, AT+CMGL safety-net poll, slot queue 8.
 *           B10 - zero-padded minute in OTP SMS.
 *           B1  - AT+CMGF=1 re-asserted before every send.
 *           B2  - specific error code exposed via GSM_Get_Last_Error.
 *****************************************************************************/
#include <LPC21xx.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "all_peripheral_defines.h"
#include "uart.h"
#include "lcd.h"
#include "delay.h"
#include "timer.h"
#include "userDB.h"
#include "gsm.h"

volatile s32 g_otp_invalidated_user = -1;
static volatile u8 g_gsm_ready = 0;
static volatile gsm_err_t g_gsm_last_error = GSM_ERR_NONE;

typedef enum { GSM_RX_IDLE = 0, GSM_RX_WAIT_HEADER, GSM_RX_WAIT_BODY } gsm_rx_state_t;

static gsm_rx_state_t s_rx_state    = GSM_RX_IDLE;
static u32            s_rx_deadline = 0;
static s32            s_slot        = -1;
static char           s_header_copy[128];

#define GSM_SLOT_QUEUE_SIZE 8
static s32 s_slot_queue[GSM_SLOT_QUEUE_SIZE];
static u8  s_slot_q_head  = 0;
static u8  s_slot_q_tail  = 0;
static u8  s_slot_q_count = 0;
static u32 s_poll_due     = 0;

static volatile u8 s_tx_busy = 0;

static volatile u8 s_pending_flag = 0;
static char        s_pending_mobile[16];
static char        s_pending_uid[12];
static u8          s_pending_state = 0;

static u8 enqueue_if_cmti(const char *line)
{
    char *ptr;
    if (strstr(line, "+CMTI:") == NULL) return 0;
    ptr = strrchr(line, ',');
    if (ptr != NULL) {
        s32 slot = (s32)atoi(ptr + 1);
        if (s_slot_q_count < GSM_SLOT_QUEUE_SIZE) {
            s_slot_queue[s_slot_q_tail] = slot;
            s_slot_q_tail = (u8)((s_slot_q_tail + 1) % GSM_SLOT_QUEUE_SIZE);
            s_slot_q_count++;
        }
    }
    return 1;
}

static u8 slot_queue_empty(void) { return (s_slot_q_count == 0) ? 1 : 0; }

static void slot_enqueue(s32 slot)
{
    if (s_slot_q_count >= GSM_SLOT_QUEUE_SIZE) return;
    s_slot_queue[s_slot_q_tail] = slot;
    s_slot_q_tail = (u8)((s_slot_q_tail + 1) % GSM_SLOT_QUEUE_SIZE);
    s_slot_q_count++;
}

static s32 slot_dequeue(void)
{
    s32 v;
    if (s_slot_q_count == 0) return -1;
    v = s_slot_queue[s_slot_q_head];
    s_slot_q_head = (u8)((s_slot_q_head + 1) % GSM_SLOT_QUEUE_SIZE);
    s_slot_q_count--;
    return v;
}

static u8 at_wait(char want, u32 timeout_ms)
{
    u32  t0 = GetTickMs();
    char line[128];

    while ((u32)(GetTickMs() - t0) < timeout_ms) {
        if (!U0_RXLine_NonBlocking(line, sizeof(line))) continue;
        if (enqueue_if_cmti(line)) continue;
        if (want == '>' && line[0] == '>' && line[1] == '\0') return 1;
        if (want == 'O' && strcmp(line, "OK") == 0)           return 1;
        if (strstr(line, "ERROR"))                            return 0;
    }
    return 0;
}
#define Wait_OK(ms)      at_wait('O', (ms))
#define Wait_Chevron(ms) at_wait('>', (ms))

/* let a half-finished CMGR dialogue complete before any AT command is sent */
static void gsm_wait_rx_idle(void)
{
    u32 t0 = GetTickMs();
    while (s_rx_state != GSM_RX_IDLE && (u32)(GetTickMs() - t0) < 3000)
        GSM_Process_Incoming_SMS_Traffic();
}

static u8 sms_begin(const char *mobile)
{
    char cmd[48];
    u32  t0 = GetTickMs();

    while (s_rx_state != GSM_RX_IDLE && (u32)(GetTickMs() - t0) < 3000)
        GSM_Process_Incoming_SMS_Traffic();

    s_tx_busy = 1;

    sprintf(cmd, "AT+CMGS=\"%s\"\r\n", mobile);
    U0_TXStr((s8 *)cmd);

    if (!Wait_Chevron(5000)) {
        U0_TX(0x1B);
        s_tx_busy = 0;
        return 0;
    }
    return 1;
}

void GSM_Init(void)
{
    u8  t;
    u8  ok = 0;

    U0_Flush_Buffer();
    for (t = 0; t < 10 && !ok; t++) {
        U0_Flush_Buffer();
        U0_TXStr((s8 *)"AT\r\n");
        ok = Wait_OK(1000);
    }
    if (!ok) { g_gsm_ready = 0; return; }

    U0_TXStr((s8 *)"ATE0\r\n");              Wait_OK(1000);
    U0_TXStr((s8 *)"AT+CMGF=1\r\n");         Wait_OK(2000);
    U0_TXStr((s8 *)"AT+CNMI=2,1,0,0,0\r\n"); Wait_OK(2000);

    g_gsm_ready = 1;
    s_poll_due  = GetTickMs() + 30000UL;
}

u8 GSM_Send_OTP_Alert(const char *mobile, const char *uid, u32 otp,
                        s32 hr, s32 mn, s32 sc)
{
    char otp_str[7];

    if (!g_gsm_ready) {
        GSM_Init();
        if (!g_gsm_ready) {
            g_gsm_last_error = GSM_ERR_MODEM_NOT_READY;
            return 0;
        }
    }

    gsm_wait_rx_idle(); U0_TXStr((s8 *)"AT+CMGF=1\r\n");
    if (!Wait_OK(2000)) {
        g_gsm_last_error = GSM_ERR_CMGF_FAILED;
        return 0;
    }

    if (!sms_begin(mobile)) {
        g_gsm_last_error = GSM_ERR_CHEVRON_TIMEOUT;
        return 0;
    }

    otp_str[0] = (char)('0' + ((otp / 100000) % 10));
    otp_str[1] = (char)('0' + ((otp /  10000) % 10));
    otp_str[2] = (char)('0' + ((otp /   1000) % 10));
    otp_str[3] = (char)('0' + ((otp /    100) % 10));
    otp_str[4] = (char)('0' + ((otp /     10) % 10));
    otp_str[5] = (char)('0' + ( otp            % 10));
    otp_str[6] = '\0';

    U0_TXStr((s8 *)"SecureEntry: Access requested for ");
    U0_TXStr((s8 *)uid);
    U0_TXStr((s8 *)" at ");

    if (hr < 10) U0_TX('0');
    U0_TXU32((u32)hr);
    U0_TX(':');
    if (mn < 10) U0_TX('0');
    U0_TXU32((u32)mn);

    U0_TXStr((s8 *)". OTP: ");
    U0_TXStr((s8 *)otp_str);
    U0_TXStr((s8 *)". Valid 60s. If not you, reply BLOCK ");
    U0_TXStr((s8 *)uid);

    U0_TX((u8)0x1A);
    {
        u8 ok = Wait_OK(15000);
        s_tx_busy = 0;
        if (ok) { g_gsm_last_error = GSM_ERR_NONE; }
        else    { g_gsm_last_error = GSM_ERR_CMGS_FAILED; }
        return ok;
    }
}

void GSM_Send_Success_Alert(const char *mobile, const char *uid)
{
    gsm_wait_rx_idle(); U0_TXStr((s8 *)"AT+CMGF=1\r\n");
    if (!Wait_OK(2000)) return;
    if (!sms_begin(mobile)) return;
    U0_TXStr((s8 *)"SecureEntry: Access successful for ");
    U0_TXStr((s8 *)uid);
    U0_TX((u8)0x1A);
    Wait_OK(15000);
    s_tx_busy = 0;
}

void GSM_Send_Block_Confirmation(const char *mobile, const char *uid, u8 block_state)
{
    gsm_wait_rx_idle(); U0_TXStr((s8 *)"AT+CMGF=1\r\n");
    if (!Wait_OK(2000)) return;
    if (!sms_begin(mobile)) return;
    U0_TXStr((s8 *)"SecureEntry: ");
    U0_TXStr((s8 *)uid);
    if (block_state) U0_TXStr((s8 *)" is BLOCKED. Pending OTP cancelled.");
    else             U0_TXStr((s8 *)" is ACTIVE. Access restored.");
    U0_TX((u8)0x1A);
    Wait_OK(15000);
    s_tx_busy = 0;
}

gsm_err_t GSM_Get_Last_Error(void) { return g_gsm_last_error; }

void GSM_Show_Last_Error(void)
{
    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8*)"  GSM Network Error ");
    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"  SMS NOT SENT!     ");

    switch (g_gsm_last_error) {
    case GSM_ERR_MODEM_NOT_READY:
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  Modem Offline     ");
        CmdLCD(GOTO_LINE4_POSO); StrLCD((s8*)"  Check Power/Cable ");
        break;
    case GSM_ERR_CMGF_FAILED:
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  Text Mode Failed  ");
        CmdLCD(GOTO_LINE4_POSO); StrLCD((s8*)"  Reset Modem       ");
        break;
    case GSM_ERR_CHEVRON_TIMEOUT:
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  No SMS Prompt     ");
        CmdLCD(GOTO_LINE4_POSO); StrLCD((s8*)"  Check SIM/Balance ");
        break;
    case GSM_ERR_CMGS_FAILED:
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  SMS Rejected      ");
        CmdLCD(GOTO_LINE4_POSO); StrLCD((s8*)"  Check Network     ");
        break;
    default:
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  Unknown Error     ");
        CmdLCD(GOTO_LINE4_POSO); StrLCD((s8*)"  Check SIM/Network ");
        break;
    }
    tdelay_ms(2500);
}

static void str_upper(char *p)
{
    while (*p) { if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32); p++; }
}

/* drop trailing punctuation so "A101." or "A101!" still matches */
static void str_trim_tail(char *p)
{
    u8 n = (u8)strlen(p);
    while (n > 0) {
        char c = p[n - 1];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) break;
        p[--n] = '\0';
    }
}

static u8 extract_last_digits(const char *src, char *dst, u8 n)
{
    char tmp[32];
    u8   len = 0;
    u8   i;
    u8   start;

    if (n == 0) return 0;
    if (n > 30) n = 30;

    for (i = 0; src[i] != '\0' && len < (sizeof(tmp) - 1); i++) {
        if (src[i] >= '0' && src[i] <= '9') tmp[len++] = src[i];
    }

    if (len == 0) { dst[0] = '\0'; return 0; }

    if (len > n) {
        start = (u8)(len - n);
        for (i = 0; i < n; i++) dst[i] = tmp[start + i];
        dst[n] = '\0';
        return n;
    } else {
        for (i = 0; i < len; i++) dst[i] = tmp[i];
        dst[len] = '\0';
        return len;
    }
}

void GSM_Process_Incoming_SMS_Traffic(void)
{
    char line[128];
    u32  now = GetTickMs();

    if (U0_Check_Overflow()) {
        s_rx_state = GSM_RX_IDLE;
        s_slot_q_head = 0; s_slot_q_tail = 0; s_slot_q_count = 0;
        s_poll_due = now;
    }

    if (s_rx_state == GSM_RX_WAIT_HEADER && (s32)(now - s_rx_deadline) >= 0)
        s_rx_state = GSM_RX_IDLE;

    if (s_rx_state == GSM_RX_WAIT_BODY && (s32)(now - s_rx_deadline) >= 0) {
        char cmd[32];
        sprintf(cmd, "AT+CMGD=%d\r\n", (int)s_slot);
        U0_TXStr((s8 *)cmd);
        s_rx_state = GSM_RX_IDLE;
    }

    if (s_rx_state == GSM_RX_IDLE && !s_tx_busy && slot_queue_empty()
        && (s32)(now - s_poll_due) >= 0) {
        u8 i;
        for (i = 1; i <= 4; i++) slot_enqueue(i);
        s_poll_due = now + 30000UL;
    }

    if (s_rx_state == GSM_RX_IDLE && !s_tx_busy && !slot_queue_empty()) {
        char cmd[32];
        s_slot = slot_dequeue();
        sprintf(cmd, "AT+CMGR=%d\r\n", (int)s_slot);
        U0_TXStr((s8 *)cmd);
        s_rx_deadline = GetTickMs() + 3000;
        s_rx_state    = GSM_RX_WAIT_HEADER;
    }

    if (!U0_RXLine_NonBlocking(line, sizeof(line))) return;
    if (enqueue_if_cmti(line)) return;

    switch (s_rx_state) {
    case GSM_RX_IDLE:
        return;

    case GSM_RX_WAIT_HEADER:
        if (strcmp(line, "OK") == 0 || strstr(line, "ERROR")) {
            s_rx_state = GSM_RX_IDLE;
            return;
        }
        if (strstr(line, "+CMGR:")) {
            strncpy(s_header_copy, line, sizeof(s_header_copy) - 1);
            s_header_copy[sizeof(s_header_copy) - 1] = '\0';
            s_rx_deadline = GetTickMs() + 2000;
            s_rx_state    = GSM_RX_WAIT_BODY;
        }
        return;

    case GSM_RX_WAIT_BODY:
        if (strlen(line) <= 2) return;
        {
            char action[12]     = {0};
            char target_uid[12] = {0};
            s32  u_idx;

            sscanf(line, "%11s %11s", action, target_uid);
            str_upper(action);                      /* "Block a101." from a phone keyboard */
            str_upper(target_uid);
            str_trim_tail(target_uid);
            u_idx = Database_Find_User(target_uid);

            if (u_idx != -1) {
                {
                    char raw_sender[20]  = {0};
                    char norm_sender[16] = {0};
                    char norm_db[16]     = {0};

                    /* sender = 2nd CSV field of  +CMGR: "REC UNREAD","<sender>",...  (any format) */
                    sscanf(s_header_copy, "%*[^,],\"%19[^\"]\"", raw_sender);
                    extract_last_digits(raw_sender, norm_sender, 10);
                    extract_last_digits(db_users[u_idx].mobile, norm_db, 10);

                    if (norm_sender[0] != '\0' &&
                        strcmp(norm_sender, norm_db) == 0) {

                        if (strcmp(action, "BLOCK") == 0) {
                            Database_Set_Lock_Status(u_idx, 1);
                            g_otp_invalidated_user = u_idx;
                            strncpy(s_pending_mobile, db_users[u_idx].mobile, sizeof(s_pending_mobile) - 1);
                            s_pending_mobile[sizeof(s_pending_mobile) - 1] = '\0';
                            strncpy(s_pending_uid, target_uid, sizeof(s_pending_uid) - 1);
                            s_pending_uid[sizeof(s_pending_uid) - 1] = '\0';
                            s_pending_state = 1;
                            s_pending_flag  = 1;
                        } else if (strcmp(action, "UNBLOCK") == 0) {
                            Database_Set_Lock_Status(u_idx, 0);
                            strncpy(s_pending_mobile, db_users[u_idx].mobile, sizeof(s_pending_mobile) - 1);
                            s_pending_mobile[sizeof(s_pending_mobile) - 1] = '\0';
                            strncpy(s_pending_uid, target_uid, sizeof(s_pending_uid) - 1);
                            s_pending_uid[sizeof(s_pending_uid) - 1] = '\0';
                            s_pending_state = 0;
                            s_pending_flag  = 1;
                        }
                    }
                }
            }
            {
                char cmd[32];
                sprintf(cmd, "AT+CMGD=%d\r\n", (int)s_slot);
                U0_TXStr((s8 *)cmd);
            }
            s_rx_state = GSM_RX_IDLE;
        }
        return;

    default:
        s_rx_state = GSM_RX_IDLE;
        return;
    }
}

void GSM_Service_Pending(void)
{
    u8   state;
    char mobile[16];
    char uid[12];

    if (!s_pending_flag) return;
    if (s_tx_busy)       return;

    state = s_pending_state;
    strncpy(mobile, s_pending_mobile, sizeof(mobile) - 1);
    mobile[sizeof(mobile) - 1] = '\0';
    strncpy(uid, s_pending_uid, sizeof(uid) - 1);
    uid[sizeof(uid) - 1] = '\0';

    s_pending_flag = 0;
    GSM_Send_Block_Confirmation(mobile, uid, state);
}