/* ========================================================================== */
/*                                                                            */
/*   oled.c                                                                   */
/*   (c) 2009 Petr Sladek ASIX s.r.o                                          */
/*                                                                            */
/*   OLED display driver                                                      */
/*                                                                            */
/* ========================================================================== */

#include "hardware_conf.h"
#include "firmware_conf.h"
#include <peripherals/pmc/pmc.h>
#include <utils/delay.h>
#include "oled.h"
#include "types.h"

void oledWriteCommand(uint16_t cmd, uint16_t param)
{
    volatile uint16_t *pOLED;
    pOLED=OLED_CMD_BASE; //data: AD1=1 (+2)  command AD1=0 (+0)
    *pOLED = (cmd<<1); //bit align
    pOLED=OLED_PARAM_BASE;  
    *pOLED = (param<<1);
}

void oledWrite(uint16_t param)
{
    volatile uint16_t *pOLED;
    pOLED=OLED_CMD_BASE; //data: AD1=1 (+2)  command AD1=0 (+0)
    *pOLED = (OLED_DDRAM<<1); //bit align

    pOLED=OLED_PARAM_BASE;  
    *pOLED = (uint16_t)(param>>9);
    *pOLED = (uint16_t)(param);  
}

static oled_profile oled_profiles[] = {
// display off
    {
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
    },
// brightness low (TODO)
    {
        0x08, 0x08, 0x08, 
        0x60, 0x60, 0x60, 
        0x04, 0x04, 0x04, 
    },
// brightness medium
    {
        0x0f, 0x0f, 0x0f,
        0xff, 0xe6, 0xd1,
        0x09, 0x0a, 0x0a,
    },
// brightness max
    {
        0x0f, 0x0f, 0x0f,
        0xff, 0xff, 0xff,
        0xff, 0xff, 0xff,
    },
};

int oledSetProfile(uint8_t profile_index) {

    if (profile_index >= sizeof(oled_profiles) / sizeof((oled_profiles)[0]))
        return 1;

    oled_profile *profile = &oled_profiles[profile_index]; 

    oledWriteCommand(PRECHARGE_TIME_R, profile->precharge_time_r);
    oledWriteCommand(PRECHARGE_TIME_G, profile->precharge_time_g);
    oledWriteCommand(PRECHARGE_TIME_B, profile->precharge_time_b);

    oledWriteCommand(PRECHARGE_CURRENT_R, profile->precharge_current_r);
    oledWriteCommand(PRECHARGE_CURRENT_G, profile->precharge_current_g);
    oledWriteCommand(PRECHARGE_CURRENT_B, profile->precharge_current_b);

    oledWriteCommand(DRIVING_CURRENT_R, profile->driving_current_r);
    oledWriteCommand(DRIVING_CURRENT_G, profile->driving_current_g);
    oledWriteCommand(DRIVING_CURRENT_B, profile->driving_current_b);

    return 0;
}

int oledInitHw(void)
{
    volatile AT91PS_PMC	pPMC = AT91C_BASE_PMC;
    volatile AT91PS_SMC2	pSMC = AT91C_BASE_SMC;
    volatile OLED_PIO_NENVOL	pPIONENVOL = OLED_PIO_NENVOL_BASE;
    volatile OLED_PIO_NORES pPIONORES = OLED_PIO_NORES_BASE;
    volatile oled_access_fast *pFastOLED;
    volatile oled_access_cmd *pCMDOLED;
    uint32_t i;


/* MV DANGER!!!
    //configure the PMC CLK and SMC for OLED (channel CS1)
    pPMC->PMC_PCER = AT91C_ID_PIOC;
    pSMC->SMC2_CSR[1] = 0x10003082;


    pPIONENVOL->PIO_PER = OLED_PIN_NENVOL;
    pPIONENVOL->PIO_SODR = OLED_PIN_NENVOL; //set to log1 (power off)  
    pPIONENVOL->PIO_OER = OLED_PIN_NENVOL;

    pPIONORES->PIO_PER = OLED_PIN_NORES;
    pPIONORES->PIO_SODR = OLED_PIN_NORES;
    pPIONORES->PIO_OER = OLED_PIN_NORES;


    delayms(10);

    pPIONENVOL->PIO_CODR = OLED_PIN_NENVOL; //set to log0 (power on)   
    //powerup:
    delayms(80);
    pPIONORES->PIO_CODR = OLED_PIN_NORES; //reset
    delayms(5);   
    pPIONORES->PIO_SODR = OLED_PIN_NORES;    
    delayms(1);
*/

/*
    oledWriteCommand(DISP_ON_OFF, 0x00);  //disp off
    oledWriteCommand(REDUCE_CURRENT, 0x01);


    oledWriteCommand(SOFT_RST, 0x01);



    oledWriteCommand(SOFT_RST, 0x00);
    oledWriteCommand(REDUCE_CURRENT, 0x00);

    oledWriteCommand(OSC_CTL, 0x01);
    oledWriteCommand(CLOCK_DIV, 0x30);

    delayms(1);
*/

#if 1
    oledSetProfile(2);
#elif 0
    oledWriteCommand(PRECHARGE_TIME_R, 0x0F);  //ghost fix orig: 3
    oledWriteCommand(PRECHARGE_TIME_G, 0x0F);  //ghost fix, orig:5
    oledWriteCommand(PRECHARGE_TIME_B, 0x0F);  //ghost fix orig: 5

    oledWriteCommand(PRECHARGE_CURRENT_R, 0xff); //ghost fix orig:56
    oledWriteCommand(PRECHARGE_CURRENT_G, 0xe6); //ghost fix orig:4D
    oledWriteCommand(PRECHARGE_CURRENT_B, 0xd1); //ghost fix orig:46

    //
    oledWriteCommand(DRIVING_CURRENT_R, 13); //9
    oledWriteCommand(DRIVING_CURRENT_G, 15);  //A
    oledWriteCommand(DRIVING_CURRENT_B, 15);  //A
#elif 0
    oledWriteCommand(PRECHARGE_TIME_R, 0x0F);  //ghost fix orig: 3
    oledWriteCommand(PRECHARGE_TIME_G, 0x0F);  //ghost fix, orig:5
    oledWriteCommand(PRECHARGE_TIME_B, 0x0F);  //ghost fix orig: 5

    oledWriteCommand(PRECHARGE_CURRENT_R, 0xff); //ghost fix orig:56
    oledWriteCommand(PRECHARGE_CURRENT_G, 0xff); //ghost fix orig:4D
    oledWriteCommand(PRECHARGE_CURRENT_B, 0xff); //ghost fix orig:46

    //
    oledWriteCommand(DRIVING_CURRENT_R, 0xff); //9
    oledWriteCommand(DRIVING_CURRENT_G, 0xff);  //A
    oledWriteCommand(DRIVING_CURRENT_B, 0xff);  //A

#else
    oledWriteCommand(PRECHARGE_TIME_R, 0x3);
    oledWriteCommand(PRECHARGE_TIME_G, 0x5);
    oledWriteCommand(PRECHARGE_TIME_B, 0x5);

    oledWriteCommand(PRECHARGE_CURRENT_R, 0x56);
    oledWriteCommand(PRECHARGE_CURRENT_G, 0x4d);
    oledWriteCommand(PRECHARGE_CURRENT_B, 0x46);

    //
    oledWriteCommand(DRIVING_CURRENT_R, 9);
    oledWriteCommand(DRIVING_CURRENT_G, 0xa);
    oledWriteCommand(DRIVING_CURRENT_B, 0xa);
#endif


/*
    oledWriteCommand(DISPLAY_MODE_SET, 0x00);
    //

    oledWriteCommand(RGB_IF, 0x01);
    oledWriteCommand(RGB_POL, 0x08);
    oledWriteCommand(MEMORY_WRITE_MODE, 0x46); //9bit auto inc transfer
    //
    oledWriteCommand(OLED_DUTY,0x7F);
    oledWriteCommand(OLED_DSL,0x00);
    oledWriteCommand(OLED_IREF,0x00);

    //clear screen (blank, black)
    oledWriteCommand(MX1_ADDR, 0);
    oledWriteCommand(MY1_ADDR, 0);
    oledWriteCommand(MX2_ADDR, OLED_RESOLUTION_X-1);
    oledWriteCommand(MY2_ADDR, OLED_RESOLUTION_Y-1);
    oledWriteCommand(MEMORY_ACCESSP_X, 0);
    oledWriteCommand(MEMORY_ACCESSP_Y, 0);  
    pCMDOLED=OLED_CMD_BASE;  
    *pCMDOLED = (OLED_DDRAM<<1); //bit align          
    pFastOLED = OLED_PARAM_BASE;          
    for (i=0;i<(((OLED_RESOLUTION_X+1)*(OLED_RESOLUTION_Y+1))/8);(i++))
    {    
        *pFastOLED = 0;
        *pFastOLED = 0;
        *pFastOLED = 0;
        *pFastOLED = 0;            
    }

    oledWriteCommand(DISP_ON_OFF, 0x01);  

    delayms(1);   
*/
    return 0;
}

void oledScreen(int16_t scrscrX1, int16_t scrscrY1, int16_t scrscrX2, int16_t scrscrY2)
{


}

void oled_screen_state(bool on) {
    oledWriteCommand(DISP_ON_OFF, on ? 0x01 : 0x00);  
}
