/*****************************************************************************
 * @file     scheduler.c
 * @brief    Finite State Machine Coordinator
 * @details  B03 - asynchronous admin switch and OTP expiry (g_kpm_abort).
 *           B06 - g_motor_off_at armed/disarmed around motor phases.
 *           B18 - re-check is_blocked before transitioning to GSM_TX.
 *           Admin override deferred while unlock_phase != 0.
 *           B2 - GSM_Show_Last_Error() on OTP send failure.
 *****************************************************************************/
#include <LPC21xx.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "all_peripheral_defines.h"
#include "timer.h"
#include "delay.h"
#include "uart.h"
#include "lcd.h"
#include "kpm.h"
#include "rtc.h"
#include "spi.h"
#include "spi_eeprom.h"
#include "eint1_sw.h"
#include "userDB.h"
#include "gsm.h"
#include "menu.h"
#include "scheduler.h"

extern volatile u8 admin_flag;
extern volatile s32 g_otp_invalidated_user;

volatile scheduler_state_t system_state = FSM_STATE_IDLE_UID;
s32 active_user_idx = -1;
static u32 generated_otp = 0;
static s32 issue_sec = 0, issue_min = 0, issue_hr = 0;

static u8 password_retry = 0;
static u8 otp_retry = 0;
#define MAX_PASSWORD_RETRY 3
#define MAX_OTP_RETRY 2

static u8   s_fail_sessions[3] = {0, 0, 0};   /* consecutive failed PIN sessions per user (brute-force back-off) */
static char uid_buffer[6] = {0};

static u8  unlock_phase    = 0;
static u32 unlock_deadline = 0;

static u8  s_otp_lcd_active = 0;
static u32 s_otp_lcd_last   = 0;

static u8  s_in_admin = 0;

static void Scheduler_KPM_Hook(void)
{
    GSM_Process_Incoming_SMS_Traffic();

    if (!s_in_admin) {
        if (admin_flag)                                         g_kpm_abort = 1;
        if (s_otp_lcd_active && get_otp_countdown_val() == 0)   g_kpm_abort = 1;
    }

    if (s_otp_lcd_active) {
        u32 now = GetTickMs();
        if ((u32)(now - s_otp_lcd_last) >= 250) {
            u32 rem = get_otp_countdown_val();

            CmdLCD(GOTO_LINE4_POSO + 11);
            if (rem > 0) {
                /* display counts 59,58,...,00 (guide requirement); the 60 s window and
                 * the expiry (rem == 0 -> EXPIRED) are unchanged */
                if ((rem - 1) < 10) { CharLCD('0'); U32LCD(rem - 1); }
                else                { U32LCD(rem - 1); }
                StrLCD((s8*)"s   ");
            } else {
                StrLCD((s8*)"EXPIRED");
            }

            CmdLCD(GOTO_LINE3_POSO);
            {
                u8 c;
                for (c = 0; c < g_kpm_readpass_digit_count; c++) CmdLCD(0x14);
            }

            s_otp_lcd_last = now;
        }
    }
}

void Scheduler_Init_Hardware(void)
{
    IODIR0 |= (1UL << MOTOR_IN1) | (1UL << MOTOR_IN2);
    IOCLR0  = (1UL << MOTOR_IN1) | (1UL << MOTOR_IN2);

    InitLCD();
    UART0_Init();
    Init_SPI0();
    RTC_Init();
    Init_KPM();
    Init_EINT1();
    InitTimer0();
    InitTimer1_Interrupt();

    /* TEMPORARY: invalidate EEPROM user markers ONCE to reload defaults.

    spi_eeprom_bytewrite(0x0000 + 18, 0x00);
    spi_eeprom_bytewrite(0x0020 + 18, 0x00);
    spi_eeprom_bytewrite(0x0040 + 18, 0x00);
    */

    Database_Sync_From_EEPROM();
    GSM_Init();

    g_kpm_idle_hook = Scheduler_KPM_Hook;
}

void Run_System_Scheduler(void)
{
    u32 raw_input;
    s32 cur_sec, cur_min, cur_hr;

    while (1) {
        g_kpm_abort = 0;

        if (system_state != FSM_STATE_UNLOCK) {
            g_motor_off_at = 0;
        }

        if (admin_flag == 1 && unlock_phase == 0) {
            s_in_admin = 1;
            IO0CLR = (1UL << MOTOR_IN1);
            IO0CLR = (1UL << MOTOR_IN2);
            g_motor_off_at = 0;
            RunAdminDashboard();
            Database_Sync_From_EEPROM();
            s_in_admin = 0;
            g_kpm_abort = 0;
            admin_flag = 0;
            system_state = FSM_STATE_IDLE_UID;
        }

        GSM_Process_Incoming_SMS_Traffic();

        if (system_state != FSM_STATE_UNLOCK) {
            GSM_Service_Pending();
        }

        if (g_otp_invalidated_user >= 0) {
            if (g_otp_invalidated_user == active_user_idx) {
                generated_otp = 0;
                otp_retry     = 0;
            }
            g_otp_invalidated_user = -1;
        }

        switch (system_state) {

        case FSM_STATE_IDLE_UID:
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE1_POSO); StrLCD((s8*)"SecureEntry System  ");
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"                    ");
            CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"Enter User ID:      ");
            CmdLCD(GOTO_LINE4_POSO);// StrLCD((s8*)"                    ");

            ReadStringKeypad(uid_buffer, 5);
            if (!admin_flag) system_state = FSM_STATE_VERIFY_UID;
            break;

        case FSM_STATE_VERIFY_UID:
            active_user_idx = Database_Find_User(uid_buffer);
            if (active_user_idx == -1) {
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"   UNKNOWN USER ID! ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"                    ");
                tdelay_ms(1500);
                system_state = FSM_STATE_IDLE_UID;
            } else if (db_users[active_user_idx].is_blocked) {
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"    ID IS BLOCKED!  ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"    ACCESS DENIED   ");
                tdelay_ms(2000);
                system_state = FSM_STATE_IDLE_UID;
            } else {
                password_retry = 0;
                system_state = FSM_STATE_PROMPT_PASS;
            }
            break;

        case FSM_STATE_PROMPT_PASS:
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE1_POSO); StrLCD((s8*)"User Verified       ");
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"Enter Your Password:");
            CmdLCD(GOTO_LINE3_POSO); //StrLCD((s8*)"                    ");

            raw_input = ReadPass();

            if (admin_flag || g_kpm_abort) {
                g_kpm_abort = 0;
                system_state = FSM_STATE_IDLE_UID;
                break;
            }

            if (g_kpm_readpass_digit_count != 4) {
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)" Password must be   ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  exactly 4 digits  ");
                tdelay_ms(1500);
                break;
            }

            generated_otp = 0;

            if (raw_input == db_users[active_user_idx].password) {

                if (db_users[active_user_idx].is_blocked) {
                    CmdLCD(CLEAR_LCD); delay_ms(2);
                    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"    ID IS BLOCKED!  ");
                    CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"    ACCESS DENIED   ");
                    tdelay_ms(2000);
                    system_state = FSM_STATE_IDLE_UID;
                    break;
                }

                otp_retry = 0;
                s_fail_sessions[active_user_idx] = 0;
                system_state = FSM_STATE_GSM_TX;
            } else {
                password_retry++;
                if (password_retry >= MAX_PASSWORD_RETRY) {
                    CmdLCD(CLEAR_LCD); delay_ms(2);
                    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"  Too many errors!  ");
                    CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)" Returning to ID... ");
                    tdelay_ms(1500);
                    /* brute-force back-off: none for the first failed session, then
                     * +5 s per further consecutive failed session (max 30 s) */
                    if (s_fail_sessions[active_user_idx] < 250) s_fail_sessions[active_user_idx]++;
                    if (s_fail_sessions[active_user_idx] > 1) {
                        u32 w = 5UL * (s_fail_sessions[active_user_idx] - 1);
                        if (w > 30) w = 30;
                        while (w--) tdelay_ms(1000);
                    }
                    system_state = FSM_STATE_IDLE_UID;
                } else {
                    CmdLCD(CLEAR_LCD); delay_ms(2);
                    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"   WRONG PASSWORD!  ");
                    CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"   Try again...     ");
                    tdelay_ms(1000);
                }
            }
            break;

        case FSM_STATE_GSM_TX:
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE1_POSO); StrLCD((s8*)" Sending SMS OTP... ");
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)" Please wait...     ");

            GetRTCTimeInfo(&cur_hr, &cur_min, &cur_sec);
            issue_hr = cur_hr; issue_min = cur_min; issue_sec = cur_sec;
            /* seed with uptime in ms (depends on human typing speed) mixed with the RTC,
             * so the OTP is not a function of the wall-clock time alone */
            srand((unsigned int)(GetTickMs() ^ (GetTickMs() << 11) ^ (u32)(cur_sec * cur_min + cur_hr)));
            generated_otp = (rand() % 999999) + 1;

            if (GSM_Send_OTP_Alert(db_users[active_user_idx].mobile,
                                   db_users[active_user_idx].user_id,
                                   generated_otp, cur_hr, cur_min, cur_sec) == 1)
            {
                start_otp_countdown(60);
                otp_retry = 0;
                system_state = FSM_STATE_WAIT_OTP;
            }
            else
            {
                generated_otp = 0;
                GSM_Show_Last_Error();
                system_state = FSM_STATE_IDLE_UID;
            }
            break;

        case FSM_STATE_WAIT_OTP:
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE1_POSO); StrLCD((s8*)"OTP Verification    ");
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"Enter SMS OTP:      ");
            CmdLCD(GOTO_LINE3_POSO);// StrLCD((s8*)"                    ");
            CmdLCD(GOTO_LINE4_POSO); StrLCD((s8*)"Time Left: 59s      ");

            s_otp_lcd_active = 1;
            s_otp_lcd_last   = GetTickMs();
            CmdLCD(GOTO_LINE3_POSO + 0);

            raw_input = ReadPass();
            s_otp_lcd_active = 0;

            if (admin_flag) {
                g_kpm_abort = 0;
                system_state = FSM_STATE_IDLE_UID;
                break;
            }
            g_kpm_abort = 0;   /* an expiry abort falls through to the "OTP EXPIRED" check below */

            if (db_users[active_user_idx].is_blocked) {
                generated_otp = 0;
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"  OTP CANCELLED!!   ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)" Remote Block Exec  ");
                tdelay_ms(2000);
                system_state = FSM_STATE_IDLE_UID;
                break;
            }

            if (get_otp_countdown_val() == 0) {
                generated_otp = 0;
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"    OTP EXPIRED!    ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)" Re-enter password  ");
                tdelay_ms(1500);
                system_state = FSM_STATE_PROMPT_PASS;
                break;
            }

            if (g_kpm_readpass_digit_count != 6) {
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"  OTP must be       ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  exactly 6 digits  ");
                tdelay_ms(1500);
                system_state = FSM_STATE_WAIT_OTP;
                break;
            }

            if (raw_input == generated_otp && generated_otp != 0) {
                generated_otp = 0;
                system_state = FSM_STATE_UNLOCK;
            } else {
                otp_retry++;
                if (otp_retry >= MAX_OTP_RETRY) {
                    generated_otp = 0;
                    CmdLCD(CLEAR_LCD); delay_ms(2);
                    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)" Invalid OTP twice  ");
                    CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)" Re-enter password  ");
                    tdelay_ms(1500);
                    system_state = FSM_STATE_PROMPT_PASS;
                } else {
                    CmdLCD(CLEAR_LCD); delay_ms(2);
                    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"   INVALID OTP!     ");
                    CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"   Try again...     ");
                    tdelay_ms(1000);
                    system_state = FSM_STATE_WAIT_OTP;
                }
            }
            break;

        case FSM_STATE_UNLOCK:
            if (unlock_phase == 0) {
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8*)"  ACCESS GRANTED!   ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8*)"  Door Opening...   ");
                IO0SET = (1UL << MOTOR_IN1);
                IO0CLR = (1UL << MOTOR_IN2);
                g_motor_off_at = GetTickMs() + 3500;
                if (g_motor_off_at == 0) g_motor_off_at = 1;
                unlock_deadline = GetTickMs() + 3000;
                unlock_phase = 1;
            } else if (unlock_phase == 1 && TickExpired(unlock_deadline)) {
                IO0CLR = (1UL << MOTOR_IN1) | (1UL << MOTOR_IN2);   /* dead time: never reverse instantly */
                g_motor_off_at = 0;
                unlock_deadline = GetTickMs() + 100;
                unlock_phase = 4;
            } else if (unlock_phase == 4 && TickExpired(unlock_deadline)) {
                CmdLCD(GOTO_LINE3_POSO);
                StrLCD((s8*)"  Securing Lock..   ");
                IO0CLR = (1UL << MOTOR_IN1);
                IO0SET = (1UL << MOTOR_IN2);
                g_motor_off_at = GetTickMs() + 3500;
                if (g_motor_off_at == 0) g_motor_off_at = 1;
                unlock_deadline = GetTickMs() + 2000;
                unlock_phase = 2;
            } else if (unlock_phase == 2 && TickExpired(unlock_deadline)) {
                IO0CLR = (1UL << MOTOR_IN1) | (1UL << MOTOR_IN2);
                g_motor_off_at = 0;
                unlock_deadline = GetTickMs() + 500;
                unlock_phase = 3;
            } else if (unlock_phase == 3 && TickExpired(unlock_deadline)) {
                GSM_Send_Success_Alert(db_users[active_user_idx].mobile,
                                       db_users[active_user_idx].user_id);
                unlock_phase = 0;
                system_state = FSM_STATE_IDLE_UID;
            }
            break;
        }
    }
}