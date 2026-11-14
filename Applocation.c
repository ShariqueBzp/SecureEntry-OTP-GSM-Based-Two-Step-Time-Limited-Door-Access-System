/*****************************************************************************
 * @file      Application.c
 * @brief     SecureEntry OTP: GSM-Based Two-Step Time-Limited Door Access System
 * @details   Main system deployment entry point for the NXP LPC2148.
 *            Initializes all core peripheral clock channels, hardware line
 *            directions, interrupt vector spaces, and hands master processing 
 *            execution flow over to the deterministic FSM Scheduler kernel.
 *****************************************************************************/

#include <LPC21xx.h>
#include "types.h"
#include "scheduler.h"

/*****************************************************************************
 * @fn        main
 * @brief     The primary software bootstrap entry point for the processor core.
 * @details   Configures system PLL trees and sets up operational loops.
 *            Complies with strict C89 variables allocation constraints.
 * @return    int (Standard system return termination block status)
 *****************************************************************************/
int main(void)
{
    /* 1. Synchronize PLL system dividers, pin functions, and hardware VIC slots */
    Scheduler_Init_Hardware();
    
    /* 2. Launch your Finite State Machine (FSM) scheduling environment */
    Run_System_Scheduler();
    
    /* Fallback exit catch to satisfy absolute compiler specifications */
    return 0;
}
