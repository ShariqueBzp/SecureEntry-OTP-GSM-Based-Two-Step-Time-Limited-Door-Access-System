/*****************************************************************************
 * @file      userDB.h
 * @brief     3-Authorized User Database Module Layout Definitions
 *****************************************************************************/
#ifndef USERDB_H
#define USERDB_H

#include "types.h"

#define DB_PASS_OFF      0
#define DB_MOB_OFF       4
#define DB_MOB_LEN       14
#define DB_MARKER_OFF    18
#define DB_MARKER_VAL    0x55

typedef struct {
    char user_id[6];
    u32  password;
    char mobile[16];
    u8   is_blocked;
    u16  eeprom_base;
} user_profile_t;

extern user_profile_t db_users[3];

void Database_Sync_From_EEPROM(void);
s32  Database_Find_User(const char *uid);

/* B07: now returns 1 on OK, 0 on read-back failure. */
u8   Database_Update_User_Credentials(s32 idx, u32 new_pass, const char *new_mob);

void Database_Set_Lock_Status(s32 idx, u8 status);

#endif /* USERDB_H */