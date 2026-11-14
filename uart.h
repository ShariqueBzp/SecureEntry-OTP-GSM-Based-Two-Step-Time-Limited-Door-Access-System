#ifndef UART_H
#define UART_H

#include "types.h"

/* ---- Initialization ---- */
void UART0_Init(void);
void UART0_ISR(void) __irq;

/* ---- Blocking byte-level TX ---- */
void U0_TX(u8 sByte);
void U0_TXStr(s8 *p);
void U0_TXU32(u32 num);
void U0_TXS32(s32 num);
void U0_TXF32(f32 fnum, u32 nDP);

/* ---- Blocking byte-level RX ---- */
u8   U0_RX(void);
s8*  U0_RXStr(void);

/* ---- Non-blocking line reader ---- */
u8   U0_RXLine_NonBlocking(char *out, u32 max);
u8   U0_RXAvailable(void);
void U0_Flush_Buffer(void);

/* ---- Diagnostics: UART RX integrity ---- */
extern volatile u32 uart_rx_overflow_cnt;  /* cumulative software overflows  */
extern volatile u32 uart_hw_err_cnt;       /* cumulative OE/PE/FE events     */

/* Returns 1 if any byte has been lost since the last call, 0 otherwise.
 * Uses cumulative counter diff - race-free, no sticky flag to clear. */
u8   U0_Check_Overflow(void);

u32  U0_Get_Overflow_Count(void);          /* software overflow counter */
u32  U0_Get_HW_Error_Count(void);          /* hardware error counter    */
u8 U0_RX_Raw(char *ch);
#endif /* UART_H */