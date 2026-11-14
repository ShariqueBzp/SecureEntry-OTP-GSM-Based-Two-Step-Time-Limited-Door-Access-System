/*****************************************************************************
 * @file   scheduler.h
 * @brief  System Finite State Machine & Scheduler declarations
 *****************************************************************************/
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

/* FSM state type. Values are internal to scheduler.c; other files should
 * only use the exported helper functions below. */
typedef enum {
    FSM_STATE_IDLE_UID = 0,
    FSM_STATE_VERIFY_UID,
    FSM_STATE_PROMPT_PASS,
    FSM_STATE_GSM_TX,
    FSM_STATE_WAIT_OTP,
    FSM_STATE_UNLOCK,
    FSM_STATE_LOCKOUT
} scheduler_state_t;

extern volatile scheduler_state_t system_state;
extern s32 active_user_idx;

void Scheduler_Init_Hardware(void);
void Run_System_Scheduler(void);

/* OTP countdown getters - these are defined in timer.c, not scheduler.c,
 * but were historically declared here for convenience. New code should
 * include timer.h instead. */
void start_otp_countdown(u32 seconds);   /* lowercase, from timer.c */
u32  get_otp_countdown_val(void);        /* lowercase, from timer.c */

#endif /* SCHEDULER_H */