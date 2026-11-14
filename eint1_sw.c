/*****************************************************************************
 * @file      eint1_sw.c
 * @brief     External Interrupt 1 driver for admin switch (P0.3).
 * @details   Falling-edge triggered. Sets admin_flag = 1 with a short
 *            software debounce window (200 ms) to prevent rapid re-entry
 *            into the admin dashboard after the FSM clears the flag.
 *****************************************************************************/

#include <LPC21xx.h>
#include "types.h"
#include "all_peripheral_defines.h"
#include "timer.h"          /* for GetTickMs() debounce */
#include "eint1_sw.h"

#define EINT1_VIC_CHNO      15
#define EINT1_PIN_FUNC     (3 << 6)   /* 0x80 */
#define EINT1_DEBOUNCE_MS   200

volatile u8 admin_flag = 0;

/* Timestamp of the last accepted edge (ms) - file scope, ISR-only access */
static volatile u32 s_last_edge_ms = 0;

void Init_EINT1(void) {
    /* P0.3 as input */
    IODIR0 &= ~(1UL << ADMIN_EINT_PIN);

    /* P0.3 ? EINT1 via PINSEL0 bits [7:6] */
    PINSEL0 &= ~(3UL << 6);
    PINSEL0 |=  EINT1_PIN_FUNC;

    /* Edge-triggered, falling edge (configured BEFORE clearing the flag so the
     * mode change cannot leave a spurious pending edge) */
    EXTMODE  |=  (1 << 1);
    EXTPOLAR &= ~(1 << 1);

    /* Clear any pending external interrupt flag */
    EXTINT = (1 << 1);

    /* VIC slot 2 for EINT1 (channel 15) */
    VICIntSelect &= ~(1 << EINT1_VIC_CHNO);
    VICVectCntl2  = (1 << 5) | EINT1_VIC_CHNO;
    VICVectAddr2  = (u32)eint1_isr;

    /* Enable */
    VICIntEnable |= (1 << EINT1_VIC_CHNO);
}

void eint1_isr(void) __irq
{
    u32 now = GetTickMs();

    /* Debounce: accept an edge only if > 200 ms since the last accepted one */
    if ((u32)(now - s_last_edge_ms) > EINT1_DEBOUNCE_MS) {
        if (READBIT(IOPIN0, ADMIN_EINT_PIN) == 0) {
            admin_flag   = 1;
            s_last_edge_ms = now;
        }
    }

    /* Clear the interrupt flag */
    EXTINT = (1 << 1);
    VICVectAddr = 0;
}