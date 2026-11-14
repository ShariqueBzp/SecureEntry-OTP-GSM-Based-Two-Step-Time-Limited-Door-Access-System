/*****************************************************************************
 * @file      kpm.h
 * @brief     Keypad Matrix Driver Header File for LPC2148
 *****************************************************************************/

#ifndef KPM_H
#define KPM_H

#include "types.h"

void Init_KPM(void);
u32 RowCheck(void);
u32 ColCheck(void);
u8 KeyScan(void);
u32 ReadNum(void);
u32 ReadPass(void);
void ReadStringKeypad(char *buf, u8 max_len);

extern void (*g_kpm_idle_hook)(void);
extern u8 g_kpm_readpass_digit_count;

/* B03: asynchronous abort plumbing */
#define KEY_ABORT 0x1B
extern volatile u8 g_kpm_abort;

#endif /* KPM_H */