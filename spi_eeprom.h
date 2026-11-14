//spi_eeprom.h
#include "types.h"
//void Cmd_25LC512(u8 cmd);
void WriteEnable(void); 
void WriteDisable(void);
void spi_eeprom_bytewrite(u16 wBuffAddr,u8 sDat);
u8   spi_eeprom_byteread(u16 rBuffAddr);




