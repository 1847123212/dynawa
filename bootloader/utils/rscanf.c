/* ========================================================================== */
/*                                                                            */
/*   Filename.c                                                               */
/*   (c) 2009 reduced scanf                                                   */
/*                                                                            */
/*   Description                                                              */
/*                                                                            */
/* ========================================================================== */


#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <peripherals/serial.h>
#include "hardware_conf.h"
#include "firmware_conf.h"

#include "rscanf.h"

void scanline(char* str)
{
  char c;
  while ((c=dbg_usart_getchar())!='\r') { *str=c; str++; }
  
  *str=0;

} 

