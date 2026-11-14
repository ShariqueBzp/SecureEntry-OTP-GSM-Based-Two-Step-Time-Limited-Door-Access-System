/*****************************************************************************
 * @file      timer.c
 * @brief     Timer0 (blocking delays) + Timer1 (1 ms tick + OTP + B06)
 * @details  B06 - g_motor_off_at fires from the Timer1 ISR (ISR-safe).
 *           B28 - T0 prescaler reset on every tdelay_*; tdelay_*(0) returns.
 *****************************************************************************/

#include <LPC21xx.h>
#include "types.h"
#include "all_peripheral_defines.h"
#include "timer.h"

volatile u32 otp_seconds_remaining = 0;
volatile u32 otp_sub_ticks          = 0;
volatile u32 g_ms_ticks             = 0;
volatile u32 g_motor_off_at         = 0;

void InitTimer0(void)
{
    T0TCR = 0x02;
    T0MCR = 0x04;
}

void tdelay_us(u32 microseconds)
{
    if (microseconds == 0) return;
    T0TCR = 0x02;
    T0MR0 = microseconds;
    T0PR  = 15 - 1;
    T0TCR = 0x01;
    while (T0TC < T0MR0);
}

void tdelay_ms(u32 milliseconds)
{
    if (milliseconds == 0) return;
    T0TCR = 0x02;
    T0MR0 = milliseconds;
    T0PR  = 15000 - 1;
    T0TCR = 0x01;
    while (T0TC < T0MR0);
}

void tdelay_s(u32 seconds)
{
    if (seconds == 0) return;
    T0TCR = 0x02;
    T0MR0 = seconds;
    T0PR  = 15000000 - 1;
    T0TCR = 0x01;
    while (T0TC < T0MR0);
}

void Timer1_ISR(void) __irq
{
    T1IR = (1 << 0);
    g_ms_ticks++;

    if (g_motor_off_at != 0 &&
        (s32)(g_ms_ticks - g_motor_off_at) >= 0) {
        IOCLR0 = (1UL << MOTOR_IN1) | (1UL << MOTOR_IN2);
        g_motor_off_at = 0;
    }

    if (otp_seconds_remaining > 0) {
        if (++otp_sub_ticks >= 1000) {
            otp_sub_ticks = 0;
            otp_seconds_remaining--;
        }
    }
    VICVectAddr = 0;
}

void InitTimer1_Interrupt(void)
{
    T1TCR = 0x02;
    T1IR  = (1 << 0);
    T1PR  = 15000 - 1;
    T1MR0 = 1;
    T1MCR = (1 << 0) | (1 << 1);

    VICIntSelect &= ~(1 << 5);
    VICVectAddr1  = (u32)Timer1_ISR;
    VICVectCntl1  = (1 << 5) | 5;
    VICVectAddr   = 0;

    VICIntEnable |= (1 << 5);
    T1TCR = 0x01;
}

u32 GetTickMs(void)                          { return g_ms_ticks; }
u8  TickExpired(u32 deadline)                { return ((s32)(GetTickMs() - deadline) >= 0) ? 1 : 0; }
u8  DelayElapsed(u32 start_tick, u32 ms)     { return ((u32)(GetTickMs() - start_tick) >= ms) ? 1 : 0; }

void start_otp_countdown(u32 seconds)
{
    otp_sub_ticks         = 0;
    otp_seconds_remaining = seconds;
}

u32 get_otp_countdown_val(void)
{
    return otp_seconds_remaining;
}