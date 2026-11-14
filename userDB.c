/*****************************************************************************
 * @file     userDB.c
 * @brief    Persistent user database - double-buffered (B07).
 * @details  B07 - two slots per user with CRC-8 and sequence number.
 *           Update invalidates target slot, writes payload, commits seq LAST.
 *           Power loss leaves old slot intact. is_blocked byte lives outside.
 *****************************************************************************/

#include <string.h>
#include "types.h"
#include "all_peripheral_defines.h"
#include "spi_eeprom.h"
#include "userDB.h"

#define SLOT_A          0x00
#define SLOT_B          0x14
#define SLOT_LEN        20
#define DB_BLOCK_OFF    0x30

user_profile_t db_users[3] = {
    {"A101", 1234, "+91XXXXXXXXXX", 0, 0x0000},
    {"B102", 5678, "+91XXXXXXXXXX", 0, 0x0040},
    {"C103", 9012, "+91XXXXXXXXXX", 0, 0x0080}
};

static u8 crc8(const u8 *p, u8 n)
{
    u8 c = 0xA5, b;
    while (n--) {
        c ^= *p++;
        for (b = 0; b < 8; b++)
            c = (c & 0x80) ? (u8)((c << 1) ^ 0x07) : (u8)(c << 1);
    }
    return c;
}

static u8 slot_read(u16 addr, u8 *r)
{
    u8 i;
    for (i = 0; i < SLOT_LEN; i++) r[i] = spi_eeprom_byteread(addr + i);
    return (r[19] != 0xFF) && (crc8(r, 18) == r[18]);
}

static void slot_write(u16 addr, const u8 *r)
{
    u8 i;
    spi_eeprom_bytewrite(addr + 19, 0xFF);
    for (i = 0; i < 19; i++) spi_eeprom_bytewrite(addr + i, r[i]);
    spi_eeprom_bytewrite(addr + 19, r[19]);
}

static s32 pick_valid_slot(u8 idx, u8 *out_slot)
{
    u16 base = db_users[idx].eeprom_base;
    u8  rA[SLOT_LEN], rB[SLOT_LEN];
    u8  vA, vB, sA, sB;

    vA = slot_read(base + SLOT_A, rA);
    vB = slot_read(base + SLOT_B, rB);

    if (vA && vB) {
        sA = rA[19]; sB = rB[19];
        /* the slot with the NEWER sequence number is the live one */
        if ((u8)(sB - sA) < 128) { *out_slot = SLOT_B; return SLOT_B; }
        else                     { *out_slot = SLOT_A; return SLOT_A; }
    }
    if (vA) { *out_slot = SLOT_A; return SLOT_A; }
    if (vB) { *out_slot = SLOT_B; return SLOT_B; }
    return -1;
}

static void load_from_slot(u8 idx, u16 slot_addr)
{
    u16 base = db_users[idx].eeprom_base;
    u8  r[SLOT_LEN];
    u8  i;
    u8  b;

    slot_read(base + slot_addr, r);

    db_users[idx].password =
        ((u32)r[0] << 24) | ((u32)r[1] << 16) |
        ((u32)r[2] << 8)  | (u32)r[3];

    for (i = 0; i < DB_MOB_LEN; i++)
        db_users[idx].mobile[i] = (char)r[4 + i];
    db_users[idx].mobile[DB_MOB_LEN] = '\0';

    b = spi_eeprom_byteread(base + DB_BLOCK_OFF);
    db_users[idx].is_blocked = (b == 1) ? 1 : 0;
}

void Database_Sync_From_EEPROM(void)
{
    u8 u;
    for (u = 0; u < 3; u++) {
        u16 base = db_users[u].eeprom_base;
        u8  slot;
        s32 chosen = pick_valid_slot(u, &slot);

        if (chosen == -1) {
            u8 r[SLOT_LEN];
            u8 i;
            u32 p = db_users[u].password;

            memset(r, 0, sizeof(r));
            r[0] = (u8)(p >> 24); r[1] = (u8)(p >> 16);
            r[2] = (u8)(p >> 8);  r[3] = (u8)p;
            for (i = 0; i < DB_MOB_LEN; i++)
                r[4 + i] = (u8)db_users[u].mobile[i];
            r[18] = crc8(r, 18);
            r[19] = 0;

            slot_write(base + SLOT_A, r);

            if (spi_eeprom_byteread(base + DB_BLOCK_OFF) == 0xFF)
                spi_eeprom_bytewrite(base + DB_BLOCK_OFF, 0);
            db_users[u].is_blocked = 0;
        } else {
            load_from_slot(u, (u16)chosen);
        }
    }
}

s32 Database_Find_User(const char *uid)
{
    s32 i;
    for (i = 0; i < 3; i++)
        if (strcmp(db_users[i].user_id, uid) == 0) return i;
    return -1;
}

u8 Database_Update_User_Credentials(s32 idx, u32 new_pass, const char *new_mob)
{
    u8 r[SLOT_LEN], chk[SLOT_LEN];
    u8 i, tgt_slot, old_slot, seq;
    u16 base, addr;

    if (idx < 0 || idx > 2) return 0;

    base = db_users[idx].eeprom_base;

    if (pick_valid_slot((u8)idx, &old_slot) == -1)
        old_slot = SLOT_A;
    tgt_slot = (old_slot == SLOT_A) ? SLOT_B : SLOT_A;

    memset(r, 0, sizeof(r));
    r[0] = (u8)(new_pass >> 24);
    r[1] = (u8)(new_pass >> 16);
    r[2] = (u8)(new_pass >> 8);
    r[3] = (u8)new_pass;

    {
        u8 n = (u8)strlen(new_mob);
        if (n > DB_MOB_LEN) n = DB_MOB_LEN;
        for (i = 0; i < n; i++) r[4 + i] = (u8)new_mob[i];
        for (; i < DB_MOB_LEN; i++) r[4 + i] = 0;
    }

    {
        u8 rA[SLOT_LEN];
        u8 prev_seq = 0;
        if (slot_read(base + old_slot, rA)) prev_seq = rA[19];
        seq = (u8)(prev_seq + 1);
        if (seq == 0xFF) seq = 0;
    }
    r[18] = crc8(r, 18);
    r[19] = seq;

    addr = base + tgt_slot;
    slot_write(addr, r);

    if (!slot_read(addr, chk) || memcmp(chk, r, SLOT_LEN) != 0) return 0;

    db_users[idx].password = new_pass;
    strncpy(db_users[idx].mobile, new_mob, sizeof(db_users[idx].mobile) - 1);
    db_users[idx].mobile[sizeof(db_users[idx].mobile) - 1] = '\0';

    return 1;
}

void Database_Set_Lock_Status(s32 idx, u8 status)
{
    u16 base;
    if (idx < 0 || idx > 2) return;
    if (db_users[idx].is_blocked == status) return;
    base = db_users[idx].eeprom_base;
    db_users[idx].is_blocked = status;
    spi_eeprom_bytewrite(base + DB_BLOCK_OFF, status);
}