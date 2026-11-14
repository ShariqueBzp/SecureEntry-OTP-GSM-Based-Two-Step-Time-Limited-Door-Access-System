// ==========================================
// SPI Protocol Source Implementation (spi.c)
// Target: LPC21xx (ARM7TDMI-S)
// ==========================================

#include <LPC21xx.h>
#include "types.h"
#include "pin_connect_block.h"
#include "all_peripheral_defines.h"

void Init_SPI0(void) 
{ 
    // cfg P0.4 pin as SCK pin
    CfgPortPinFunc(0, 4, 1);

    // cfg P0.5 pin as MISO pin
    CfgPortPinFunc(0, 5, 1);

    // cfg P0.6 pin as MOSI pin
    CfgPortPinFunc(0, 6, 1);

    // make P0.7 chip select (CS) as high
    IOSET0 = (1 << CS_0_7);

    // cfg P0.7 as GPIO output pin
    IODIR0 |= (1 << CS_0_7);

    // cfg SPI clock speed using S0SPCCR
    if (SPI_DIVIDER >= 8)
    {
        if (SPI_DIVIDER % 2 == 0)
        {
            S0SPCCR = SPI_DIVIDER;
        }
        else
        {
            S0SPCCR = SPI_DIVIDER + 1;   // ensure even value >= 8
        }
    }
    else
    {
        S0SPCCR = 8;                     // minimum safe even divider
    }

    // cfg SPI peripheral for Master Mode, Mode3, MSB-first
    S0SPCR = (1 << MSTR_BIT) | (1 << CPHA_BIT) | (1 << CPOL_BIT);
} 

// Write and read at the same time (full-duplex)
u8 SPI0(u8 wByte)
{
    u8  rByte;
    u32 timeout;

    // write to SPI data buffer, initiates transfer
    S0SPDR = wByte;

    // wait for Data Transfer Completion Flag - with timeout guard
    timeout = 100000UL;
    while (((S0SPSR >> SPIF_BIT) & 1) == 0)
    {
        if (--timeout == 0)
        {
            // SPI peripheral stalled - return 0xFF sentinel, do not hang
            return 0xFF;
        }
    }

    // read received byte via data register
    rByte = S0SPDR;

    return rByte;
}