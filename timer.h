/*****************************************************************************
 * @file      timer.h
 * @brief     Timer0 (polling delays) + Timer1 (1 ms tick / OTP / B06 failsafe)
 *****************************************************************************/

#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void InitTimer0(void);
void tdelay_us(u32 microseconds);
void tdelay_ms(u32 milliseconds);
void tdelay_s (u32 seconds);

void InitTimer1_Interrupt(void);
void start_otp_countdown(u32 seconds);
u32  get_otp_countdown_val(void);

u32  GetTickMs(void);
u8   DelayElapsed(u32 start_tick, u32 ms);
u8   TickExpired(u32 deadline_tick);

/* B06: hardware motor failsafe. Set to GetTickMs()+3500 to arm;
 * 0 to disarm. Timer1_ISR forces both motor pins low at the deadline. */
extern volatile u32 g_motor_off_at;

#endif /* TIMER_H */