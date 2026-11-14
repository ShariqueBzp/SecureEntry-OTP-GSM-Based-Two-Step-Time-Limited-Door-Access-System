/*****************************************************************************
 * @file     menu.c
 * @brief    Supervisor admin terminal (20x4 LCD)
 * @details  B07 - EEPROM FAIL shown on update failure.
 *           B08 - separate admin PIN at ADMIN_PIN_ADDR, menu 5 changes it.
 *           B15 - exactly 4 digits enforced.
 *           B16 - mobile digits-only validation.
 *****************************************************************************/
#include <LPC21xx.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "all_peripheral_defines.h"
#include "lcd.h"
#include "kpm.h"
#include "delay.h"
#include "timer.h"
#include "spi_eeprom.h"
#include "userDB.h"
#include "menu.h"
#include "gsm.h"

#define ADMIN_PIN_ADDR      0x00E0
#define ADMIN_PIN_MARKER    0x5A
#define ADMIN_MARKER_ADDR   0x00E4
#define DEFAULT_ADMIN_PIN   2580u   /* factory admin PIN - change it from the admin menu */

static u8   Admin_Authenticate(s32 *user_idx);
static void Menu_Edit_RTC_Time(void);
static void Menu_Edit_RTC_Date(void);
static void Menu_Edit_Mobile_No(s32 idx);
static void Menu_Edit_Password(s32 idx);
static void Menu_Edit_Admin_Pin(void);

static u32 read_admin_pin(void)
{
    u32 v = 0; u8 i;
    for (i = 0; i < 4; i++) v = (v << 8) | spi_eeprom_byteread(ADMIN_PIN_ADDR + i);
    return v;
}
static void write_admin_pin(u32 pin)
{
    u8 i;
    for (i = 0; i < 4; i++) spi_eeprom_bytewrite(ADMIN_PIN_ADDR + i, (u8)(pin >> ((3 - i) * 8)));
}
static void admin_pin_init(void)
{
    if (spi_eeprom_byteread(ADMIN_MARKER_ADDR) != ADMIN_PIN_MARKER) {
        write_admin_pin(DEFAULT_ADMIN_PIN);
        spi_eeprom_bytewrite(ADMIN_MARKER_ADDR, ADMIN_PIN_MARKER);
    }
}

void RunAdminDashboard(void) {
    s32 target_idx = -1;
    u8 key;

    admin_pin_init();
    if (Admin_Authenticate(&target_idx) == 0) return;

    while (1) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"1. Set Time         ");
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"2. Set Mobile       ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"3. Set PIN          ");
        CmdLCD(GOTO_LINE4_POSO); StrLCD((s8 *)"4. Exit             ");

        key = KeyScan();
        if (key == KEY_ABORT) return;

        switch (key) {
            case '1':
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"1. Edit Time        ");
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"2. Edit Date        ");
                CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"5. Change Admin PIN ");
                CmdLCD(GOTO_LINE4_POSO); StrLCD((s8 *)"Press 1, 2 or 5     ");
                key = KeyScan();
                if (key == KEY_ABORT) return;
                if (key == '1') Menu_Edit_RTC_Time();
                else if (key == '2') Menu_Edit_RTC_Date();
                else if (key == '5') Menu_Edit_Admin_Pin();
                break;
            case '2': Menu_Edit_Mobile_No(target_idx); break;
            case '3': Menu_Edit_Password(target_idx); break;
            case '4':
            case 'D':
            case 'd':
            case 'c':
            case 'C':
                CmdLCD(CLEAR_LCD); delay_ms(2);
                CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Exiting Dashboard...");
                CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"                    ");
                tdelay_ms(1000);
                return;
            default: break;
        }
    }
}

static u8 Admin_Authenticate(s32 *user_idx) {
    char uid_input[5] = {0};
    s32 idx;
    u32 entered_pin;
    u32 admin_pin;

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Confirm User ID:    ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    ReadStringKeypad(uid_input, 5);
    if (g_kpm_abort) return 0;

    idx = Database_Find_User(uid_input);
    if (idx == -1) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   ID Not Found!    ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1500);
        return 0;
    }

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Admin PIN:          ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    entered_pin = ReadPass();
    if (g_kpm_abort) return 0;

    if (g_kpm_readpass_digit_count != 4) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"PIN must be 4 digits");
        tdelay_ms(1500);
        return 0;
    }

    admin_pin = read_admin_pin();

    if (entered_pin == admin_pin) {        /* a normal user PIN no longer opens the admin menu */
        *user_idx = idx;
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)" Access Confirmed   ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1000);
        return 1;
    }

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   Auth Failure!    ");
    CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
    tdelay_ms(1500);
    return 0;
}

static void Menu_Edit_RTC_Time(void) {
    s32 val;

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Hour (0-23):        ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    val = (s32)ReadNum();
    if (val >= 0 && val <= 23) { HOUR = (u8)val; SEC = 0; }
    else {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   Out of Range!    ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1000);
        return;
    }

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Minute (0-59):      ");
    CmdLCD(GOTO_LINE2_POSO); //StrLCD((s8 *)"                    ");
    val = (s32)ReadNum();
    if (val >= 0 && val <= 59) {
        MIN = (u8)val; SEC = 0;
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)" Time Synchronized  ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1000);
    } else {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   Out of Range!    ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1000);
    }
}

static void Menu_Edit_RTC_Date(void) {
    s32 val;

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Date (1-31):        ");
    CmdLCD(GOTO_LINE2_POSO); //StrLCD((s8 *)"                    ");
    val = (s32)ReadNum();
    if (val >= 1 && val <= 31) { DOM = (u8)val; }
    else {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   Out of Range!    ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1000);
        return;
    }

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Month (1-12):       ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    val = (s32)ReadNum();
    if (val >= 1 && val <= 12) {
        MONTH = (u8)val;
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)" Date Synchronized  ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1000);
    } else {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   Out of Range!    ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1000);
    }
}

static void Menu_Edit_Mobile_No(s32 idx) {
    char new_mob[15] = {0};
    u8  i, n, ok;

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"New Phone No:       ");
    CmdLCD(GOTO_LINE2_POSO); //StrLCD((s8 *)"                    ");
    ReadStringKeypad(new_mob, 14);
    if (g_kpm_abort) return;

    n  = (u8)strlen(new_mob);
    ok = (n >= 10) ? 1 : 0;
    for (i = 0; i < n; i++) {
        if (new_mob[i] < '0' || new_mob[i] > '9') { ok = 0; break; }
    }

    if (ok) {
        if (Database_Update_User_Credentials(idx, db_users[idx].password, new_mob)) {
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   Mobile Updated   ");
            CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"   EEPROM Sync OK   ");
            tdelay_ms(1500);
        } else {
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"   EEPROM FAIL!     ");
            CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"   Not Saved        ");
            tdelay_ms(2000);
        }
    } else {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"  Invalid Number!   ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"                    ");
        tdelay_ms(1500);
    }
}

static void Menu_Edit_Password(s32 idx) {
    u32 pin_first;
    u32 pin_confirm;

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Enter New PIN:      ");
    CmdLCD(GOTO_LINE2_POSO); //StrLCD((s8 *)"                    ");
    pin_first = ReadPass();
    if (g_kpm_abort) return;
    if (g_kpm_readpass_digit_count != 4) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"PIN must be 4 digits");
        tdelay_ms(1500);
        return;
    }

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Confirm New PIN:    ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    pin_confirm = ReadPass();
    if (g_kpm_abort) return;
    if (g_kpm_readpass_digit_count != 4) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"PIN must be 4 digits");
        tdelay_ms(1500);
        return;
    }

    if (pin_first == pin_confirm) {
        if (Database_Update_User_Credentials(idx, pin_first, db_users[idx].mobile)) {
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"  PIN Changed OK!   ");
            CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"   EEPROM Sync OK   ");
            tdelay_ms(1500);
        } else {
            CmdLCD(CLEAR_LCD); delay_ms(2);
            CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"  EEPROM FAIL!      ");
            CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"  PIN not saved     ");
            tdelay_ms(2000);
        }
    } else {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"  PINs Mismatched   ");
        CmdLCD(GOTO_LINE3_POSO); StrLCD((s8 *)"  Update Aborted!   ");
        tdelay_ms(1500);
    }
}

static void Menu_Edit_Admin_Pin(void) {
    u32 pin_first;
    u32 pin_confirm;
    u32 old_pin;

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Old Admin PIN:      ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    old_pin = ReadPass();
    if (g_kpm_abort) return;
    if (g_kpm_readpass_digit_count != 4 || old_pin != read_admin_pin()) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"  Wrong Admin PIN   ");
        tdelay_ms(1500);
        return;
    }

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"New Admin PIN:      ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    pin_first = ReadPass();
    if (g_kpm_abort) return;
    if (g_kpm_readpass_digit_count != 4) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"PIN must be 4 digits");
        tdelay_ms(1500);
        return;
    }

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE1_POSO); StrLCD((s8 *)"Confirm New PIN:    ");
    CmdLCD(GOTO_LINE2_POSO);// StrLCD((s8 *)"                    ");
    pin_confirm = ReadPass();
    if (g_kpm_abort) return;
    if (g_kpm_readpass_digit_count != 4) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"PIN must be 4 digits");
        tdelay_ms(1500);
        return;
    }

    if (pin_first != pin_confirm) {
        CmdLCD(CLEAR_LCD); delay_ms(2);
        CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"  PINs Mismatched   ");
        tdelay_ms(1500);
        return;
    }

    write_admin_pin(pin_first);

    CmdLCD(CLEAR_LCD); delay_ms(2);
    CmdLCD(GOTO_LINE2_POSO); StrLCD((s8 *)"Admin PIN Updated   ");
    tdelay_ms(1500);
}