#ifndef ISL6295_H_
#define  ISL6295_H_

#define I2CGG_PHY_ADDR (0x26>>1)

#define I2CGG_REG_OPmode 0x07A
#define I2CGG_BANK_OPmode 0x01

#define I2CGG_BANK_GPIOctrl 0x01
#define I2CGG_REG_GPIOctrl 0x53

#define I2CGG_POR_BITMASK 0x02
#define I2CGG_SHELF_BITMASK (0x04)

#define I2CMASTER_WRITE 0
#define I2CMASTER_READ 1

#endif  // ISL6295_H_