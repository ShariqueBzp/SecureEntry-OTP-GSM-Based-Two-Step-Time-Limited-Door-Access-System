/*****************************************************************************
 * @file   gsm.h
 * @brief  GSM SMS driver - SIMCom A7662 interface
 *****************************************************************************/
#ifndef GSM_H
#define GSM_H

#include "types.h"

void GSM_Init(void);

u8 GSM_Send_OTP_Alert(const char *mobile, const char *uid, u32 otp,
                        s32 hr, s32 mn, s32 sc);
void GSM_Send_Success_Alert(const char *mobile, const char *uid);
void GSM_Send_Block_Confirmation(const char *mobile, const char *uid,
                                 u8 block_state);

void GSM_Process_Incoming_SMS_Traffic(void);
void GSM_Service_Pending(void);

extern volatile s32 g_otp_invalidated_user;

/* B2: specific error reporting */
typedef enum {
    GSM_ERR_NONE = 0,
    GSM_ERR_MODEM_NOT_READY,
    GSM_ERR_CMGF_FAILED,
    GSM_ERR_CHEVRON_TIMEOUT,
    GSM_ERR_CMGS_FAILED
} gsm_err_t;

gsm_err_t GSM_Get_Last_Error(void);
void      GSM_Show_Last_Error(void);

#endif /* GSM_H */