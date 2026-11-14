/*****************************************************************************
 * @file      all_peripheral_defines.h
 * @brief     Consolidated Master Peripheral Definitions and Bit Macros
 *            for SecureEntry OTP GSM-Based Door Access System (LPC2148)
 * @details   Audit fixes reflected here:
 *              B11 - MOTOR_IN1 / MOTOR_IN2 moved to P0.20 / P0.21
 *                    (P0.24 / P0.25 do not exist on LPC2148 LQFP64).
 *****************************************************************************/

#ifndef ALL_PERIPHERAL_DEFINES_H
#define ALL_PERIPHERAL_DEFINES_H

#include <LPC21xx.h>
#include <stdlib.h>
#include "types.h"

#define SETBIT(WORD, BIT)               ((WORD) |=  (1UL << (BIT)))
#define CLRBIT(WORD, BIT)               ((WORD) &= ~(1UL << (BIT)))
#define CPLBIT(WORD, BIT)               ((WORD) ^=  (1UL << (BIT)))
#define WRITEBIT(WORD, BITPOS, LEVEL)   ((WORD) =   ((WORD) & ~(1UL << (BITPOS))) | ((LEVEL) << (BITPOS)))
#define READBIT(WORD, BITPOS)           (((WORD) >> (BITPOS)) & 1UL)

#define READWRITEBIT(DWORD, DBIT, SWORD, SBIT) \
    ((DWORD) = ((DWORD) & ~(1UL << (DBIT))) | ((((SWORD) >> (SBIT)) & 1UL) << (DBIT)))

#define WRITENIBBLE(WORD, POS, VAL)     ((WORD) = ((WORD) & ~(15UL << (POS))) | (((VAL) & 0x0F) << (POS)))
#define READNIBBLE(WORD, POS)           (((WORD) >> (POS)) & 0x0F)

#define WRITEBYTE(WORD, POS, BYTE)      ((WORD) = ((WORD) & ~(0xFF << (POS))) | (((BYTE) & 0xFF) << (POS)))
#define READBYTE(WORD, POS)             (((WORD) >> (POS)) & 0xFF)

#define WRITENBITS(WORD, POS, NBITS, VAL) \
    ((WORD) = ((WORD) & ~(((1UL << (NBITS)) - 1UL) << (POS))) | (((VAL) & ((1UL << (NBITS)) - 1UL)) << (POS)))

#define F_OSC           12000000UL
#define SYS_CCLK        (F_OSC * 5)
#define SYS_PCLK        (SYS_CCLK / 4)

#define PCLK1           15000000UL
#define PREINT_VALUE    ((PCLK1 / 32768) - 1)
#define PREFRAC_VALUE   (PCLK1 - ((PREINT_VALUE + 1) * 32768))

#define PIN_FUNC1       0
#define PIN_FUNC2       1
#define PIN_FUNC3       2
#define PIN_FUNC4       3

#define TXD0_PIN        0x00000001
#define RXD0_PIN        0x00000004

#define BAUD            9600
#define DIVISOR         (SYS_PCLK / (16 * BAUD))

#define _8BIT           3
#define WORD_LEN        _8BIT
#define DLAB_BIT        7
#define DR_BIT          0
#define TEMP_BIT        6

#define RBR_IE          0
#define THRE_IE         1
#define RLS_IE          2

#define FIFO_EN         0
#define RX_FIFO_RST     1
#define TX_FIFO_RST     2

#define SCK_0_4         0x00000100
#define MISO_0_5        0x00000400
#define MOSI_0_6        0x00001000
#define CS_0_7          7

#define CPHA_BIT        3
#define CPOL_BIT        4
#define MSTR_BIT        5
#define LSBF_BIT        6
#define SPIF_BIT        7

#define SPI_CLK         1000000UL
#define SPI_DIVIDER     (SYS_PCLK / SPI_CLK)

#define WRITE           0x02
#define READ            0x03
#define WRDI            0x04
#define WREN            0x06

#define LCD_DATA        10
#define LCD_RS          17
#define LCD_EN          18
#define LCD_RW          19

#define CLEAR_LCD           0x01
#define RET_CUR_HOME        0x02
#define LCD_RESET           0x03
#define SHIFT_CUR_RIGHT     0x06
#define SHIFT_CUR_LEFT      0x07
#define DSP_OFF             0x08
#define DSP_ON_CUR_OFF      0x0C
#define DSP_ON_CUR_ON       0x0E
#define DSP_ON_CUR_BLK      0x0F
#define SHIFT_DSP_LEFT      0x10
#define SHIFT_DSP_RIGHT     0x14
#define MODE_8BIT_2LINE     0x38
#define MODE_4BIT_2LINE     0x28

#define GOTO_LINE1_POSO     0x80
#define GOTO_LINE2_POSO     0xC0
#define GOTO_LINE3_POSO     0x94
#define GOTO_LINE4_POSO     0xD4
#define GOTO_CGRAM_START    0x40

#define ADMIN_EINT_PIN      3

#define ROW0     16
#define ROW1     17
#define ROW2     18
#define ROW3     19

#define COL0     20
#define COL1     21
#define COL2     22
#define COL3     23

#define TIMER0_CHNO             4
#define TCR_RESET               0x02
#define TCR_ENABLE              0x01
#define TCR_DISABLE             0x00

#define MCR_INTERRUPT_ON_MATCH  (1 << 0)
#define MCR_RESET_ON_MATCH      (1 << 1)
#define MCR_STOP_ON_MATCH       (1 << 2)

#define PRESCALE_US             (15 - 1)
#define PRESCALE_MS             (15000 - 1)
#define PRESCALE_S              (15000000 - 1)

/* B11: motor moved to P0.20 / P0.21 (P0.24 / P0.25 absent on LPC2148) */
#define MOTOR_IN1               20
#define MOTOR_IN2               21

#define EEPROM_PWD_ADDR         0x0010
#define EEPROM_MOB_ADDR         0x0020
#define OTP_TIMEOUT_SEC         60

#endif /* ALL_PERIPHERAL_DEFINES_H */