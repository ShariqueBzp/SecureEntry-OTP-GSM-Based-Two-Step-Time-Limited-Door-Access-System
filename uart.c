/*****************************************************************************
 * @file     uart.c
 * @brief    Interrupt-Driven UART0 - Ring Buffer + Non-Blocking Reader
 * @details  B17 - preserve byte on overrun; both counters in overflow check;
 *                 flush clears partial-line buffer; ring 512 bytes.
 *           B04 - '>' prompt returned as one-character line so at_wait
 *                 in gsm.c can see it. Leading space dropped.
 *****************************************************************************/
#include <LPC21xx.h>
#include "types.h"
#include "all_peripheral_defines.h"
#include "uart.h"

#define UART_RX_BUF_SIZE    512
#define LINE_MAX            128

static volatile u8  rx_buffer[UART_RX_BUF_SIZE];
static volatile u32 rx_head = 0;
static volatile u32 rx_tail = 0;

static char partial[LINE_MAX];
static u32  partial_len = 0;

volatile u32 uart_rx_overflow_cnt  = 0;
volatile u32 uart_hw_err_cnt       = 0;

void UART0_ISR(void) __irq
{
    u8  data;
    u32 next_head;

    if (U0LSR & 0x0E) uart_hw_err_cnt++;

    while ((U0LSR & (1 << 0)) != 0) {
        data = U0RBR;
        next_head = (rx_head + 1) % UART_RX_BUF_SIZE;
        if (next_head != rx_tail) {
            rx_buffer[rx_head] = data;
            rx_head = next_head;
        } else {
            uart_rx_overflow_cnt++;
        }
    }
    VICVectAddr = 0;
}

void UART0_Init(void)
{
    PINSEL0 &= (u32)~15 << 0;
    PINSEL0 |= TXD0_PIN | RXD0_PIN;

    U0LCR = (1 << DLAB_BIT) | WORD_LEN;
    U0DLL = DIVISOR;
    U0DLM = DIVISOR >> 8;
    U0LCR &= ~(1 << DLAB_BIT);

    U0FCR = (1 << FIFO_EN) | (1 << RX_FIFO_RST) | (1 << TX_FIFO_RST);
    U0IER = (1 << RBR_IE);

    uart_rx_overflow_cnt = 0;
    uart_hw_err_cnt      = 0;

    VICIntSelect &= ~(1 << 6);
    VICVectAddr0  = (u32)UART0_ISR;
    VICVectCntl0  = (1 << 5) | 6;
    VICIntEnable |= (1 << 6);
}

void U0_TX(u8 sByte)
{
    U0THR = sByte;
    while (((U0LSR >> TEMP_BIT) & 1) == 0);
}

u8 U0_RX(void)
{
    u8 data;
    while (rx_head == rx_tail);
    data = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUF_SIZE;
    return data;
}

void U0_TXStr(s8 *p) { while (*p) U0_TX((u8)*p++); }

void U0_TXU32(u32 num)
{
    u8  a[12];
    s32 i = 0;
    if (num == 0) { U0_TX('0'); return; }
    while (num > 0) { a[i++] = (num % 10) + 48; num /= 10; }
    for (--i; i >= 0; i--) U0_TX(a[i]);
}

void U0_TXS32(s32 num)
{
    if (num < 0) { U0_TX('-'); num = -num; }
    U0_TXU32((u32)num);
}

void U0_TXF32(f32 fnum, u32 nDP)
{
    u32 num;
    s32 i;
    if (fnum < 0.0) { U0_TX('-'); fnum = -fnum; }
    num = (u32)fnum;
    U0_TXU32(num);
    U0_TX('.');
    for (i = 0; i < (s32)nDP; i++) {
        fnum = (fnum - (f32)num) * 10.0f;
        num = (u32)fnum;
        U0_TX((u8)num + 48);
    }
}

s8* U0_RXStr(void)
{
    static s8  line_buf[128];
    static s32 i = 0;
    s8 ch;

    if (rx_head == rx_tail) return NULL;

    while (rx_head != rx_tail) {
        ch = (s8)U0_RX();
        if (ch == '\n' || ch == '\r') {
            if (i > 0) {
                line_buf[i] = '\0';
                i = 0;
                return line_buf;
            }
        } else if (i < 127) {
            line_buf[i++] = ch;
        } else {
            i = 0;
            return NULL;
        }
    }
    return NULL;
}

u8 U0_RXLine_NonBlocking(char *out, u32 max)
{
    u8 ch;

    if (max == 0) return 0;

    while (rx_head != rx_tail) {
        ch = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) % UART_RX_BUF_SIZE;

        /* B04: SIMCom '>' SMS prompt has no CR/LF. */
        if (ch == '>' && partial_len == 0) {
            out[0] = '>';
            out[1] = '\0';
            return 1;
        }

        /* B17: drop one leading space so URCs are not missed. */
        if (ch == ' ' && partial_len == 0) continue;

        if (ch == '\r' || ch == '\n') {
            if (partial_len == 0) continue;
            if (partial_len >= max) partial_len = max - 1;
            partial[partial_len] = '\0';
            {
                u32 k;
                for (k = 0; k < partial_len; k++) out[k] = partial[k];
                out[partial_len] = '\0';
            }
            partial_len = 0;
            return 1;
        }

        if (partial_len < (sizeof(partial) - 1)) {
            partial[partial_len++] = (char)ch;
        } else {
            partial_len = 0;
        }
    }
    return 0;
}

u8 U0_RXAvailable(void)
{
    return (rx_head != rx_tail) ? 1 : 0;
}

void U0_Flush_Buffer(void)
{
    rx_tail = rx_head;
    partial_len = 0;
    partial[0] = '\0';
}

u8 U0_Check_Overflow(void)
{
    static u32 last_sw = 0;
    static u32 last_hw = 0;
    u32 now_sw = uart_rx_overflow_cnt;
    u32 now_hw = uart_hw_err_cnt;

    if (now_sw != last_sw || now_hw != last_hw) {
        last_sw = now_sw;
        last_hw = now_hw;
        return 1;
    }
    return 0;
}

u32 U0_Get_Overflow_Count(void)
{
    return uart_rx_overflow_cnt;
}

u32 U0_Get_HW_Error_Count(void)
{
    return uart_hw_err_cnt;
}