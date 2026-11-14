// ==========================================
// SPI EEPROM Source Implementation (spi_eeprom.c)
// Target: AT25LC512 / SPI EEPROM
// ==========================================

#include <LPC21xx.h>
#include "spi.h"
#include "all_peripheral_defines.h"
#include "types.h"
#include "timer.h" // Include your timer header so tdelay_ms is visible

void WriteEnable(void) 
{
    IOCLR0 = 1 << CS_0_7;
    SPI0(WREN);
    IOSET0 = 1 << CS_0_7;
}

void WriteDisable(void) 
{
    IOCLR0 = 1 << CS_0_7;
    SPI0(WRDI);
    IOSET0 = 1 << CS_0_7;
}

void spi_eeprom_bytewrite(u16 wBuffAddr, u8 sDat) 
{
    WriteEnable();

    IOCLR0 = 1 << CS_0_7;
  
    SPI0(WRITE); 
    SPI0(wBuffAddr >> 8);
    SPI0(wBuffAddr);
    SPI0(sDat);

    IOSET0 = 1 << CS_0_7;

    /* Use your precise Timer0 hardware millisecond delay */
    tdelay_ms(10);

    WriteDisable();
} 

u8 spi_eeprom_byteread(u16 rBuffAddr) 
{
    u8 rDat;
    
    IOCLR0 = 1 << CS_0_7;
  
    SPI0(READ);    
    SPI0(rBuffAddr >> 8);
    SPI0(rBuffAddr);    

    rDat = SPI0(0x00);

    IOSET0 = 1 << CS_0_7;

    return rDat;    
}