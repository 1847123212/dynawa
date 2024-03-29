/**
 * \file usb_irq.c
 * Interrupt service routines for UDP
 * 
 * This files holds the interrupt service routines for UDP peripheral
 * 
 * AT91SAM7S-128 USB Mass Storage Device with SD Card by Michael Wolf\n
 * Copyright (C) 2008 Michael Wolf\n\n
 * 
 * This program is free software: you can redistribute it and/or modify\n
 * it under the terms of the GNU General Public License as published by\n
 * the Free Software Foundation, either version 3 of the License, or\n
 * any later version.\n\n
 * 
 * This program is distributed in the hope that it will be useful,\n
 * but WITHOUT ANY WARRANTY; without even the implied warranty of\n
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n
 * GNU General Public License for more details.\n\n
 * 
 * You should have received a copy of the GNU General Public License\n
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.\n
 *
 */
 
 //tmp
#include "hardware_conf.h"
#include "firmware_conf.h"
 
#include <utils/interrupt_utils.h>
//#include "AT91SAM7SE512.h"

#include <screen/screen.h>
#include <screen/font.h>
#include <debug/trace.h>
#include <utils/macros.h>
#include "usb_drv.h"

extern AT91PS_UDP  pUDP;    // USB controller

/**
 * UDP interrupt service routine
 * 
 * Handels all UDP peripheral interrupts
 * 
 */
//void NAKEDFUNC usb_irq_service(void)
void usb_irq_service(void)
{
    //ISR_ENTRY();
    // End interrupt if we are not attached to USB bus
    //*AT91C_PIOA_SODR = PIN_LED;
    //TRACE_ALL("@");    
    
    if (ISCLEARED(usb_device_state,USB_STATE_ATTACHED)) goto end_of_irq;

    unsigned char endpoint;
    unsigned int status = pUDP->UDP_ISR & pUDP->UDP_IMR & ISR_MASK;

    while( status != 0)
    {
        // Start Of Frame (SOF)
        if (ISSET(status, AT91C_UDP_SOFINT))
        {
            TRACE_USB("SOF   ");
            // Acknowledge interrupt
            SET(pUDP->UDP_ICR, AT91C_UDP_SOFINT);
            CLEAR(status, AT91C_UDP_SOFINT);
        }        
        // Suspend
        if (ISSET(status,AT91C_UDP_RXSUSP))
        {
            TRACE_USB("Susp   ");
           
            if (ISCLEARED(usb_device_state,USB_STATE_SUSPENDED))
            {
                // The device enters the Suspended state
                //      MCK + UDPCK must be off
                //      Pull-Up must be connected
                //      Transceiver must be disabled

                // Enable wakeup
                SET(pUDP->UDP_IER, AT91C_UDP_WAKEUP | AT91C_UDP_RXRSM);

                // Acknowledge interrupt
                SET(pUDP->UDP_ICR, AT91C_UDP_RXSUSP);
                // Set suspended state
                SET(usb_device_state,USB_STATE_SUSPENDED);
                // Disable transceiver
                SET(pUDP->UDP_TXVC, AT91C_UDP_TXVDIS);
                // Disable master clock
                AT91C_BASE_PMC->PMC_PCDR |= (1 << AT91C_ID_UDP);
                // Disable peripheral clock for USB
                AT91C_BASE_PMC->PMC_SCDR |= AT91C_PMC_UDP;
            }            
        }
        // Resume
        else
        if (ISSET(status, AT91C_UDP_WAKEUP) || ISSET(status, AT91C_UDP_RXRSM))
        {
            TRACE_USB("Resm   ");

            // The device enters Configured state
            //      MCK + UDPCK must be on
            //      Pull-Up must be connected
            //      Transceiver must be enabled
            // Powered state
            // Enable master clock
            AT91C_BASE_PMC->PMC_PCER |= (1 << AT91C_ID_UDP);
            // Enable peripheral clock for USB
            AT91C_BASE_PMC->PMC_SCER |= AT91C_PMC_UDP;

            // Default state
            if (ISSET(usb_device_state,USB_STATE_DEFAULT))
            {
                // Enable transceiver
                CLEAR(pUDP->UDP_TXVC, AT91C_UDP_TXVDIS);
            }

            CLEAR(usb_device_state, USB_STATE_SUSPENDED);

            SET(pUDP->UDP_ICR, AT91C_UDP_WAKEUP | AT91C_UDP_RXRSM | AT91C_UDP_RXSUSP);
            SET(pUDP->UDP_IDR, AT91C_UDP_WAKEUP | AT91C_UDP_RXRSM);
        }        
        // End of bus reset
        else
        if (ISSET(status, AT91C_UDP_ENDBUSRES))
        {
            TRACE_USB("\n\n\nEoBres   ");

            // Initialize UDP peripheral device
            usb_bus_reset_handler();

            // Flush and enable the Suspend interrupt
            SET(pUDP->UDP_ICR, AT91C_UDP_WAKEUP | AT91C_UDP_RXRSM | AT91C_UDP_RXSUSP);

            // Acknowledge end of bus reset interrupt
            SET(pUDP->UDP_ICR, AT91C_UDP_ENDBUSRES);
        }
        // Endpoint interrupts
        else {
            while (status != 0) {

                // Get endpoint index
                endpoint = last_set_bit(status);
                usb_endpoint_handler(endpoint);
                
                CLEAR(pUDP->UDP_ICR, (1 << endpoint));
                
                CLEAR(status, 1 << endpoint);
            }
        }          

        status = pUDP->UDP_ISR & pUDP->UDP_IMR & ISR_MASK;
       
        // Mask unneeded interrupts
        if (ISCLEARED(usb_device_state,DEFAULT_STATE))
        {
            status &= AT91C_UDP_ENDBUSRES | AT91C_UDP_SOFINT;
        }
        
    }   // end while status != 0

end_of_irq:
    *AT91C_AIC_EOICR = 1;    // ACK interrupt end
    *AT91C_AIC_ICCR = 1 << AT91C_ID_UDP;    // clear interrupt on the interrupt controller    

    //ISR_EXIT();   
}
