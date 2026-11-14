/*****************************************************************************
 * @file     kpm.c
 * @brief    Keypad Matrix Driver for LPC2148
 * @details  B02 - settle, debounce, OOB guard, row-nibble mask.
 *           B03 - KEY_ABORT propagation via g_kpm_abort.
 *****************************************************************************/

#include <lpc21xx.h>
#include "types.h"
#include "all_peripheral_defines.h"
#include "lcd.h"
#include "delay.h"
#include "kpm.h"

void (*g_kpm_idle_hook)(void) = 0;
u8 g_kpm_readpass_digit_count = 0;
volatile u8 g_kpm_abort = 0;

#ifdef KPM_LAYOUT_HARDWARE
u8 KPMLUT[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
#endif

#ifdef KPM_LAYOUT_PROTEUS
u8 KPMLUT[4][4] = {
    {'7', '8', '9', 'A'},
    {'4', '5', '6', 'B'},
    {'1', '2', '3', 'C'},
    {'*', '0', '#', 'D'}
};
#endif

#if !defined(KPM_LAYOUT_HARDWARE) && !defined(KPM_LAYOUT_PROTEUS)
u8 KPMLUT[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
#endif

static u32 ColScan(void) {
    return (READNIBBLE(IOPIN1, COL0) < 15) ? 0 : 1;
}

void Init_KPM(void) {
    WRITENIBBLE(IODIR1, ROW0, 15);
}

u32 RowCheck(void) {
    u32 rno;
    for (rno = 0; rno < 4; rno++) {
        WRITENIBBLE(IOPIN1, ROW0, ((~(1UL << rno)) & 0x0F));
        delay_us(10);
        if (ColScan() == 0) break;
    }
    IOCLR1 = (15UL << ROW0);
    return rno;
}

u32 ColCheck(void) {
    u32 cno;
    for (cno = 0; cno < 4; cno++) {
        if (READBIT(IOPIN1, (cno + COL0)) == 0) break;
    }
    return cno;
}

u8 KeyScan(void) {
    u32 rno, cno;
    u8  key;

    for (;;) {
        while (ColScan()) {
            if (g_kpm_idle_hook) g_kpm_idle_hook();
            if (g_kpm_abort) return KEY_ABORT;
        }
        delay_ms(20);
        if (ColScan()) continue;
        rno = RowCheck();
        cno = ColCheck();
        if (rno < 4 && cno < 4) break;
    }
    key = KPMLUT[rno][cno];
    while (!ColScan()) {
        if (g_kpm_idle_hook) g_kpm_idle_hook();
        if (g_kpm_abort) return KEY_ABORT;
    }
    delay_ms(20);
    return key;
}

u32 ReadNum(void) {
    u8 key;
    u32 num = 0;
    u8 cnt = 0;
input_restart:
    num = 0; cnt = 0;
    while (1) {
        key = KeyScan();
        if (key == KEY_ABORT) { g_kpm_readpass_digit_count = 0; return 0; }
        if (key >= '0' && key <= '9') {
            if (cnt < 4) { num = (num * 10) + (key - '0'); CharLCD(key); cnt++; }
        } else if (key == 'C') {
            if (cnt > 0) { num /= 10; cnt--; CmdLCD(0x10); CharLCD(' '); CmdLCD(0x10); }
            if (cnt == 0) goto input_restart;
        } else if (key == 'D') { goto clear_and_restart; }
        else { if (cnt == 0) goto input_restart; break; }
    }
    return num;
clear_and_restart:
    while (cnt > 0) { CmdLCD(0x10); CharLCD(' '); CmdLCD(0x10); cnt--; }
    goto input_restart;
}

u32 ReadPass(void) {
    u8 key;
    u32 num = 0;
    u8 cnt = 0;
input_restart:
    num = 0; cnt = 0; g_kpm_readpass_digit_count = 0;
    while (1) {
        key = KeyScan();
        if (key == KEY_ABORT) { g_kpm_readpass_digit_count = 0; return 0; }
        if (key >= '0' && key <= '9') {
            if (cnt < 6) { num = (num * 10) + (key - '0'); CharLCD('*'); cnt++;
                           g_kpm_readpass_digit_count = cnt; }
        } else if (key == 'C') {
            if (cnt > 0) { num /= 10; cnt--;
                           CmdLCD(0x10); CharLCD(' '); CmdLCD(0x10);
                           g_kpm_readpass_digit_count = cnt; }
            if (cnt == 0) goto input_restart;
        } else if (key == 'D') { goto clear_and_restart; }
        else { if (cnt == 0) goto input_restart; break; }
    }
    return num;
clear_and_restart:
    while (cnt > 0) { CmdLCD(0x10); CharLCD(' '); CmdLCD(0x10); cnt--; }
    g_kpm_readpass_digit_count = 0;
    goto input_restart;
}

void ReadStringKeypad(char *buf, u8 max_len) {
    u8 key;
    u8 idx = 0;
    while (idx < (max_len - 1)) {
        key = KeyScan();
        if (key == KEY_ABORT) { buf[0] = '\0'; return; }
        if (key == '#' || key == 'D') { if (idx > 0) break; }
        else if (key == 'C' && idx > 0) {          /* backspace (a leading 'C' is a letter: ID C103) */
            idx--; buf[idx] = '\0'; CmdLCD(0x10); CharLCD(' '); CmdLCD(0x10);
        } else { buf[idx++] = (char)key; CharLCD(key); }
    }
    buf[idx] = '\0';
}