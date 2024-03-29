/**
 * \file usb_drv.c
 * USB driver 
 * 
 * USB driver functions
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
#include <stdlib.h>
#include "hardware_conf.h"
#include "firmware_conf.h"
#include <utils/macros.h>
#include <screen/screen.h>
#include <screen/font.h>
#include <debug/trace.h>
#include <utils/delay.h>
#include <peripherals/pmc/pmc.h>
#include "usb_msd.h"
#include "usb_sbc.h"
#include "usb_lun.h"
#include "usb_bot.h"
#include "usb_dsc.h"
#include "usb_std.h"
#include "usb_drv.h"

AT91PS_AIC  pAIC = AT91C_BASE_AIC;      //!< Interrupt controller
AT91PS_UDP  pUDP = AT91C_BASE_UDP;      //!< USB controller
//! Device status, connected/disconnected etc.
volatile unsigned char usb_device_state = USB_STATE_DETACHED;

void usb_enable_device(void);           // Enable UDP peripheral
void usb_disable_device(void);          // disable UDP peripheral

//! Struct holding USB setup request parameters
s_usb_request usb_setup;
//! Pointer to setup request parameters
s_usb_request *pSetup = &usb_setup;
//! Struct for each endpoint holding endpoint parameters
s_usb_endpoint usb_epdata[NUM_OF_ENDPOINTS];
//! Pointers array to enpoint parameters
s_usb_endpoint *pEndpoint[NUM_OF_ENDPOINTS] = {&usb_epdata[0],&usb_epdata[1],&usb_epdata[2]};

volatile unsigned char usb_configuration;   //!< Current USB configuration

extern S_bot *pBot;

extern void usb_irq_service(void);

USB_PIO pUSB_PIO = USB_PIO_BASE;

unsigned char usb_power_detect(void)
{
   //$$$ this is wrong solution,  pin have to be tested several times (digital noise filering)!
   #ifdef PIN_USB_DETECT
   if (ISCLEARED(pUSB_PIO->PIO_PDSR, PIN_USB_DETECT)) return 1; else return 0;
   #else
   return 1;
   #endif
}

signed char last_set_bit(unsigned int value)
{
    signed char locindex = -1;

    if (value & 0xFFFF0000)
    {
        locindex += 16;
        value >>= 16;
    }

    if (value & 0xFF00)
    {
        locindex += 8;
        value >>= 8;
    }

    if (value & 0xF0)
    {
        locindex += 4;
        value >>= 4;
    }

    if (value & 0xC)
    {
        locindex += 2;
        value >>= 2;
    }

    if (value & 0x2)
    {
        locindex += 1;
        value >>= 1;
    }

    if (value & 0x1)
    {
        locindex++;
    }

    return locindex;
}


/**
 * USB check bus status
 * 
 * This routine enables/disables the USB module by monitoring
 * the USB power signal.
 * 
 */
unsigned char usb_check_bus_status(void)
{
    /** ******************************** \todo remove this, find hardware solution */
    //SET(pPIO->PIO_CODR, PIN_USB_PULLUP); 
    //delay(1);
    /* ********************************* necessary for hardware bug in USB detection */
     
    //internal pullup
    
    if (usb_power_detect())
    {
        
        // check if UDP is deactivated
        if (ISCLEARED(usb_device_state, USB_STATE_ATTACHED))
        {            

            usb_enable_device();    // enable USB device
            //TRACE_USB("UDP ENABLED-");   
        }
        
        //TRACE_USB("*");   
        if (ISSET(usb_device_state,USB_STATE_ATTACHED))
        {
            // clear all interrupts
            pUDP->UDP_ICR = 0;
            // enable UDP peripheral interrupts
            pUDP->UDP_IER = AT91C_UDP_ENDBUSRES | AT91C_UDP_RMWUPE | AT91C_UDP_RXSUSP;
            // Disable the interrupt on the interrupt controller
            SET(pAIC->AIC_IDCR, 1<<AT91C_ID_UDP);
            // Save the interrupt handler routine pointer
            pAIC->AIC_SVR[AT91C_ID_UDP] = (unsigned int) usb_irq_service;
            // Store the Source Mode Register
            pAIC->AIC_SMR[AT91C_ID_UDP] = AT91C_AIC_PRIOR_HIGHEST;
            // Clear the interrupt on the interrupt controller
            SET(pAIC->AIC_ICCR, 1<<AT91C_ID_UDP);
            // enable UDP interrupt on the interrupt controller        
            SET(pAIC->AIC_IECR, 1<<AT91C_ID_UDP);
            // we are in powered state now
            SET(usb_device_state,USB_STATE_POWERED);
            
            //TRACE_USB("USBSET-");
        }   
    }
    else
    {
        // check if UDP is activated
        if (ISSET(usb_device_state,USB_STATE_ATTACHED))
        {
            usb_disable_device();   // disable USB device
            //AT91C_BASE_UDP->UDP_TXVC &= ~AT91C_UDP_PUON;
        }         
    }
    
    return ISSET(usb_device_state,USB_STATE_POWERED);
}

/**
 * Enable USB device
 * 
 * This routine enables the UDP peripheral device.
 * Function should never called manually!
 * 
 */
void usb_enable_device(void)
{
    uint8_t i;
    // Set the PLL USB Divider
    AT91C_BASE_CKGR->CKGR_PLLR |= AT91C_CKGR_USBDIV_1 ;
    // Enables the 48MHz USB clock UDPCK and System Peripheral USB Clock
    pPMC->PMC_SCER |= AT91C_PMC_UDP;
    pPMC->PMC_PCER |= (1 << AT91C_ID_UDP);

    // Init data banks
    for (i=0; i < NUM_OF_ENDPOINTS; i++) {
        pEndpoint[i]->dFlag = AT91C_UDP_RX_DATA_BK0;
    }

    // we are in attached state now
    SET(usb_device_state, USB_STATE_ATTACHED);

    TRACE("UDP enabled\n");
}


/**
 * Disable USB device
 *
 * This routine disables the UDP peripheral device.
 * Function should never called manually!
 * 
 */
void usb_disable_device(void)
{
    
    #ifdef USB_PIO_PULLUP
    volatile USB_PIO pPIO = USB_PIO_BASE;   
    pPIO->PIO_SODR  = USB_PIO_PULLUP;   // enable pull-up for USBP to signal host a device disconnection
    #else
    pUDP->UDP_TXVC |= AT91C_UDP_PUON;
    #endif
    
    // Disables the 48MHz USB clock UDPCK and System Peripheral USB Clock
    pPMC->PMC_SCDR |= AT91C_PMC_UDP;
    pPMC->PMC_PCDR |= (1 << AT91C_ID_UDP);

    // Disable the interrupt on the interrupt controller
    pAIC->AIC_IDCR |= 1 << AT91C_ID_UDP;
    pUDP->UDP_IDR = 0;  // disable all UDP interrupts
    
    
    pUDP->UDP_TXVC |= AT91C_UDP_TXVDIS;  // disable UDP tranceiver

    // we are in detached state now
    usb_device_state = 0;
    SET(usb_device_state, USB_STATE_DETACHED);

    TRACE("UDP disabled\n");
}


/**
 * Disable USB device by software
 *
 * This routine disables the UDP peripheral device
 * through software.
 * 
 */
void usb_soft_disable_device(void)
{
    usb_disable_device();   
}


/**
 * USB Protocol reset handler
 *
 * A USB bus reset is received from the host.
 * 
 */
void usb_bus_reset_handler(void)
{
    TRACE_USB("Rst   ");
    // clear UDP peripheral interrupt
    pUDP->UDP_ICR |= AT91C_UDP_ENDBUSRES;
    
    // reset all endpoints
    pUDP->UDP_RSTEP  = (unsigned int)-1;
    pUDP->UDP_RSTEP  = 0;
    
    // Enable the UDP
    pUDP->UDP_FADDR = AT91C_UDP_FEN;
    
    usb_configure_endpoint(0);

    // configure UDP peripheral interrups, here only EP0 and bus reset
    pUDP->UDP_IER = AT91C_UDP_EPINT0 | AT91C_UDP_ENDBUSRES | AT91C_UDP_RXSUSP;
    
    // Flush and enable the Suspend interrupt
    pUDP->UDP_ICR |= AT91C_UDP_WAKEUP | AT91C_UDP_RXRSM | AT91C_UDP_RXSUSP;
    
    pUDP->UDP_TXVC &= ~AT91C_UDP_TXVDIS;  // enable UDP tranceiver 
    
    // The device leaves the Address & Configured states
    CLEAR(usb_device_state, USB_STATE_ADDRESS | USB_STATE_CONFIGURED);
    
    // we are in default state now
    SET(usb_device_state, USB_STATE_DEFAULT);
}


/**
 * End transfer
 * 
 * End transfer of given endpoint.
 * 
 * \param   endpoint    Endpoint where to end transfer
 * \param   bStatus     Status code returned by the transfer operation
 * 
 */
__inline void usb_end_of_transfer(unsigned char endpoint, char bStatus)
{
    if ((pEndpoint[endpoint]->dState == endpointStateWrite)
        || (pEndpoint[endpoint]->dState == endpointStateRead)) {

        TRACE_USB("EoT(%d)   ",pEndpoint[endpoint]->dBytesTransferred);

        // Endpoint returns in Idle state
        pEndpoint[endpoint]->dState = endpointStateIdle;
        
        // Invoke callback is present
        if (pEndpoint[endpoint]->fCallback != 0) {

            pEndpoint[endpoint]->fCallback((unsigned int) pEndpoint[endpoint]->pArgument,
                                 (unsigned int) bStatus,
                                 pEndpoint[endpoint]->dBytesTransferred,
                                 pEndpoint[endpoint]->dBytesRemaining
                                 + pEndpoint[endpoint]->dBytesBuffered);
        }
    }
}


/**
 * Configure specific endpoint
 * 
 * \param   endpoint Endpoint to configure
 * 
 */
void usb_configure_endpoint(unsigned char endpoint)
{
    // Abort the current transfer is the endpoint was configured and in
    // Write or Read state
    if ((pEndpoint[endpoint]->dState == endpointStateRead)
        || (pEndpoint[endpoint]->dState == endpointStateWrite)) {

        usb_end_of_transfer(endpoint, USB_STATUS_RESET);
    }

    // Enter IDLE state
    pEndpoint[endpoint]->dState = endpointStateIdle;
    // Reset endpoint transfer descriptor
    pEndpoint[endpoint]->pData = 0;
    pEndpoint[endpoint]->dBytesRemaining = 0;
    pEndpoint[endpoint]->dBytesTransferred = 0;
    pEndpoint[endpoint]->dBytesBuffered = 0;
    pEndpoint[endpoint]->fCallback = 0;
    pEndpoint[endpoint]->pArgument = 0;

    // Reset Endpoint Fifos
    SET(pUDP->UDP_RSTEP, 1 << endpoint);
    CLEAR(pUDP->UDP_RSTEP, 1 << endpoint);

    if (endpoint == 0)  // EP CONTROL
    {
        pEndpoint[0]->wMaxPacketSize = EP0_BUFF_SIZE;
        pEndpoint[0]->dNumFIFO = 1;
        pUDP->UDP_CSR[0] = AT91C_UDP_EPTYPE_CTRL | AT91C_UDP_EPEDS;
    }
    else
    if (endpoint == 1)  // EP OUT
    {
        pEndpoint[1]->wMaxPacketSize = EP1_BUFF_SIZE;
        pEndpoint[1]->dNumFIFO = 2;
        pUDP->UDP_CSR[1] = AT91C_UDP_EPTYPE_BULK_OUT | AT91C_UDP_EPEDS;
    }
    else
    if (endpoint == 2)  // EP IN
    {
        pEndpoint[2]->wMaxPacketSize = EP2_BUFF_SIZE;
        pEndpoint[2]->dNumFIFO = 2;
        pUDP->UDP_CSR[2] = AT91C_UDP_EPTYPE_BULK_IN | AT91C_UDP_EPEDS;         
    }

    TRACE_USB("CfgEpt%d   ", endpoint);
}


/**
 * USB Send ZLP EP0
 *
 * This routine sends a zero length packet through endpoint 0
 * 
 */
inline char usb_send_zlp0(Callback_f  fCallback, void *pArgument)
{
    return usb_write(0, 0, 0, fCallback, pArgument);
}


/**
 * Sets or unsets the device address
 *
 */ 
void usb_set_address(void)
{
    unsigned int address = pSetup->wValue;

    TRACE_USB("SetAddr(%d) ", address);

    // Set address
    SET(pUDP->UDP_FADDR, AT91C_UDP_FEN | address);

    if (address == 0) {

        SET(pUDP->UDP_GLBSTATE, 0);

        // Device enters the Default state
        CLEAR(usb_device_state, USB_STATE_ADDRESS);
    }
    else {

        SET(pUDP->UDP_GLBSTATE, AT91C_UDP_FADDEN);

        // The device enters the Address state
        SET(usb_device_state, USB_STATE_ADDRESS);
    }
}


/**
 * Clear transmission status flag
 * 
 * This routine clears the transmission status flag and
 * swaps banks for dual FIFO endpoints.
 * 
 * \param   endpoint    Endpoint where to clear flag
 * 
 */
static void usb_clear_rx_flag(unsigned char endpoint)
{
    // Clear flag
    usb_ep_clr_flag(pUDP, endpoint, pEndpoint[endpoint]->dFlag);

    // Swap banks
    if (pEndpoint[endpoint]->dFlag == AT91C_UDP_RX_DATA_BK0) {

        if (pEndpoint[endpoint]->dNumFIFO > 1) {
            // Swap bank if in dual-fifo mode
            pEndpoint[endpoint]->dFlag = AT91C_UDP_RX_DATA_BK1;
        }
    }
    else {
        pEndpoint[endpoint]->dFlag = AT91C_UDP_RX_DATA_BK0;
    }    
}


/**
 * Writes data to UDP FIFO
 * 
 * This routine writes data from specific buffer to given endpoint FIFO.
 * 
 * \param   endpoint    Endpoint to write data
 * \return  Number of bytes write
 * 
 */
static unsigned int usb_writepayload( unsigned char endpoint)
{
    unsigned int bytes; //???
    uint16_t ctr;

    // Get the number of bytes to send
    bytes = MIN(pEndpoint[endpoint]->wMaxPacketSize, pEndpoint[endpoint]->dBytesRemaining);

    // Transfer one packet in the FIFO buffer
    for (ctr = 0; ctr < bytes; ctr++)
    {
        pUDP->UDP_FDR[endpoint] = *(pEndpoint[endpoint]->pData);
        pEndpoint[endpoint]->pData++;
    }
    // track status of byte transmission
    pEndpoint[endpoint]->dBytesBuffered += bytes;
    pEndpoint[endpoint]->dBytesRemaining -= bytes;

    return bytes;
}


/**
 * Send data packet via given endpoint
 * 
 * Send a data packet with specific size via given endpoint.
 * 
 * \param   endpoint    Endpoint to send data through
 * \param   pData       Pointer to data buffer
 * \param   len         Packet size
 * \param   fCallback   Optional callback to invoke when the read finishes
 * \param   pArgument   Optional callback argument 
 * \return  Operation result code
 * 
 */
char usb_write(unsigned char endpoint,
               const void *pData,
               unsigned int len,
               Callback_f    fCallback,
               void          *pArgument)
{
    // Check that the endpoint is in Idle state
    if (pEndpoint[endpoint]->dState != endpointStateIdle) return USB_STATUS_LOCKED;

    TRACE_USB("Write%d(%d)   ", endpoint, len);

    // Setup the transfer descriptor
    pEndpoint[endpoint]->pData = (char *) pData;
    pEndpoint[endpoint]->dBytesRemaining = len;
    pEndpoint[endpoint]->dBytesBuffered = 0;
    pEndpoint[endpoint]->dBytesTransferred = 0;
    pEndpoint[endpoint]->fCallback = fCallback;
    pEndpoint[endpoint]->pArgument = pArgument;

    // Send one packet
    pEndpoint[endpoint]->dState = endpointStateWrite;
    usb_writepayload(endpoint);
    usb_ep_set_flag(pUDP, endpoint, AT91C_UDP_TXPKTRDY);

    // If double buffering is enabled and there is data remaining,
    // prepare another packet
    if ((pEndpoint[endpoint]->dNumFIFO > 1) && (pEndpoint[endpoint]->dBytesRemaining > 0))
        usb_writepayload(endpoint);

    // Enable interrupt on endpoint
    SET(pUDP->UDP_IER, 1 << endpoint);
    
    return USB_STATUS_SUCCESS;
}


/**
 * Read data from UDP FIFO
 * 
 * This routine reads data from given endpoint FIFO with given size to specific buffer.
 * 
 * \param   endpoint    Endpoint to read data from
 * \param   packetsize  Maximum size of packet to receive
 * \return  Number of bytes read.
 * 
 */
static unsigned int usb_getpayload(unsigned char endpoint, unsigned int packetsize)
{
    unsigned int bytes;
    unsigned int ctr; 

    // Get number of bytes to retrieve
    bytes = MIN(pEndpoint[endpoint]->dBytesRemaining, packetsize);

    // Retrieve packet
    for (ctr= 0; ctr < bytes; ctr++) {

        *pEndpoint[endpoint]->pData = (char) pUDP->UDP_FDR[endpoint];
        pEndpoint[endpoint]->pData++;
    }
    // track bytes transmission status
    pEndpoint[endpoint]->dBytesRemaining -= bytes;
    pEndpoint[endpoint]->dBytesTransferred += bytes;
    pEndpoint[endpoint]->dBytesBuffered += packetsize - bytes;

    return bytes;
}


/**
 * Read data packet from given endpoint
 * 
 * Read a data packet with specific size from given endpoint.
 * 
 * \param   endpoint    Endpoint to send data through
 * \param   pData       Pointer to data buffer
 * \param   len         Packet size
 * \param   fCallback   Optional callback to invoke when the read finishes
 * \param   pArgument   Optional callback argument 
 * \return  Operation result code
 * 
 */
char usb_read(unsigned char endpoint,
              void *pData,
              unsigned int len,
              Callback_f    fCallback,
              void          *pArgument)
{
    // Check that the endpoint is in Idle state
    if (pEndpoint[endpoint]->dState != endpointStateIdle) return USB_STATUS_LOCKED;

    TRACE_USB("Read%d(%d)   ", endpoint, len);

    // Endpoint enters Read state
    pEndpoint[endpoint]->dState = endpointStateRead;

    // Set the transfer descriptor
    pEndpoint[endpoint]->pData = (char *) pData;
    pEndpoint[endpoint]->dBytesRemaining = len;
    pEndpoint[endpoint]->dBytesBuffered = 0;
    pEndpoint[endpoint]->dBytesTransferred = 0;
    pEndpoint[endpoint]->fCallback = fCallback;
    pEndpoint[endpoint]->pArgument = pArgument;
    
    // Enable interrupt on endpoint
    SET(pUDP->UDP_IER, 1 << endpoint);
    
    return USB_STATUS_SUCCESS;
}


/**
 * USB endpoint handler
 *
 * Service routine for all endpoint communication
 *  
 * \param   endpoint    Endpoint for which to handle communication
 * 
 * Called from UDP interrupt service routine in case of
 * endpoint interrupt.
 * 
 */
void usb_endpoint_handler(unsigned char endpoint)
{
    unsigned int status = pUDP->UDP_CSR[endpoint];
    
    TRACE_USB("EP%d   ",endpoint);

    // Handle interrupts
    // IN packet sent
    if (ISSET(status, AT91C_UDP_TXCOMP))
    {
        TRACE_USB("Wr ");

        // Check that endpoint was in Write state
        if (pEndpoint[endpoint]->dState == endpointStateWrite)
        {

            // End of transfer ?
            if ((pEndpoint[endpoint]->dBytesBuffered < pEndpoint[endpoint]->wMaxPacketSize)
                ||
                (!ISCLEARED(status, AT91C_UDP_EPTYPE)
                 && (pEndpoint[endpoint]->dBytesRemaining == 0)
                 && (pEndpoint[endpoint]->dBytesBuffered == pEndpoint[endpoint]->wMaxPacketSize)))
            {
                TRACE_USB("%d   ", pEndpoint[endpoint]->dBytesBuffered);

                pEndpoint[endpoint]->dBytesTransferred += pEndpoint[endpoint]->dBytesBuffered;
                pEndpoint[endpoint]->dBytesBuffered = 0;

                // Disable interrupt if this is not a control endpoint
                if (!ISCLEARED(status, AT91C_UDP_EPTYPE))
                    SET(pUDP->UDP_IDR, 1 << endpoint);

                usb_end_of_transfer(endpoint, USB_STATUS_SUCCESS);
            }
            else
            {

                // Transfer remaining data
                TRACE_USB("+%d   ", pEndpoint[endpoint]->dBytesBuffered);

                pEndpoint[endpoint]->dBytesTransferred += pEndpoint[endpoint]->wMaxPacketSize;
                pEndpoint[endpoint]->dBytesBuffered -= pEndpoint[endpoint]->wMaxPacketSize;

                // Send next packet
                if (pEndpoint[endpoint]->dNumFIFO == 1)
                {
                    // No double buffering
                    usb_writepayload(endpoint);
                    usb_ep_set_flag(pUDP, endpoint, AT91C_UDP_TXPKTRDY);
                }
                else
                {
                    // Double buffering
                    usb_ep_set_flag(pUDP, endpoint, AT91C_UDP_TXPKTRDY);
                    usb_writepayload(endpoint);
                }
            }
        }
                
        // acknowledge interrupt
        usb_ep_clr_flag(pUDP, endpoint, AT91C_UDP_TXCOMP);
    }
    // OUT packet received
    if (ISSET(status, AT91C_UDP_RX_DATA_BK0) || ISSET(status, AT91C_UDP_RX_DATA_BK1))
    {
        TRACE_USB("Rd   ");
       
        // Check that the endpoint is in Read state
        if (pEndpoint[endpoint]->dState != endpointStateRead)
        {
            // Endpoint is NOT in Read state
            if (ISCLEARED(status, AT91C_UDP_EPTYPE)
                && ISCLEARED(status, 0xFFFF0000)) {

                // Control endpoint, 0 bytes received
                // Acknowledge the data and finish the current transfer
                TRACE_USB("Ack   ");
                usb_clear_rx_flag(endpoint);

                usb_end_of_transfer(endpoint, USB_STATUS_SUCCESS);
            }
            else if (ISSET(status, AT91C_UDP_FORCESTALL))
            {
                // Non-control endpoint
                // Discard stalled data
                TRACE_USB("Disc   ");
                usb_clear_rx_flag(endpoint);
            }
            else
            {
                // Non-control endpoint
                // Nak data
                TRACE_USB("Nak   ");
                SET(pUDP->UDP_IDR, 1 << endpoint);
            }
        }
        else
        {
            // Endpoint is in Read state
            // Retrieve data and store it into the current transfer buffer
            unsigned short packetsize = (unsigned short) (status >> 16);

            TRACE_USB("%d   ", packetsize);

            usb_getpayload(endpoint, packetsize);

            usb_clear_rx_flag(endpoint);

            if ((pEndpoint[endpoint]->dBytesRemaining == 0)
                || (packetsize < pEndpoint[endpoint]->wMaxPacketSize))
            {
                // Disable interrupt if this is not a control endpoint
                if (!ISCLEARED(status, AT91C_UDP_EPTYPE))
                    SET(pUDP->UDP_IDR, 1 << endpoint);

                usb_end_of_transfer(endpoint, USB_STATUS_SUCCESS);
            }
        }
    }
    // SETUP packet received
    if (ISSET(status, AT91C_UDP_RXSETUP))
    {
        TRACE_USB("Stp   ");

        // If a transfer was pending, complete it
        // Handle the case where during the status phase of a control write
        // transfer, the host receives the device ZLP and ack it, but the ack
        // is not received by the device
        if ((pEndpoint[endpoint]->dState == endpointStateWrite)
            || (pEndpoint[endpoint]->dState == endpointStateRead))
        {
            usb_end_of_transfer(endpoint, USB_STATUS_SUCCESS);
        }

        // Get request parameters
        pSetup->bmRequestType = pUDP->UDP_FDR[0];
        pSetup->bRequest      = pUDP->UDP_FDR[0];
        pSetup->wValue        = (pUDP->UDP_FDR[0] & 0xFF);
        pSetup->wValue       |= (pUDP->UDP_FDR[0] << 8);
        pSetup->wIndex        = (pUDP->UDP_FDR[0] & 0xFF);
        pSetup->wIndex       |= (pUDP->UDP_FDR[0] << 8);
        pSetup->wLength       = (pUDP->UDP_FDR[0] & 0xFF);
        pSetup->wLength      |= (pUDP->UDP_FDR[0] << 8);

        // Set the DIR bit before clearing RXSETUP in Control IN sequence
        if (pSetup->bmRequestType & 0x80)
            usb_ep_set_flag(pUDP, endpoint, AT91C_UDP_DIR);
                
        // acknowledge interrupt
        usb_ep_clr_flag(pUDP, endpoint, AT91C_UDP_RXSETUP);

        /*
         * request are forwarded first to BOT driver
         * but passed back to standard handler if no BOT request
         */
        bot_request_handler();
    }
    // STALL sent
    if (ISSET(status, AT91C_UDP_STALLSENT))
    {
        TRACE_USB("Sta   ");
        
        // acknowledge interrupt
        usb_ep_clr_flag(pUDP, endpoint, AT91C_UDP_STALLSENT);
        
        // If the endpoint is not halted, clear the stall condition
        if (pEndpoint[endpoint]->dState != endpointStateHalted)
            usb_ep_clr_flag(pUDP, endpoint, AT91C_UDP_FORCESTALL);
    }

}


/**
 * Send stall condition
 * 
 * Send stall condition to given endpoint
 * 
 * \param   endpoint    Endpoint where to send stall condition
 * 
 */
void usb_stall(unsigned char endpoint)
{
    // Check that endpoint is in Idle state
    if (pEndpoint[endpoint]->dState != endpointStateIdle) return;

    TRACE_USB("Stall%d   ", endpoint);

    usb_ep_set_flag(pUDP, endpoint, AT91C_UDP_FORCESTALL);
}


/**
 * Handle halt feature request
 * 
 * Set or clear a halt feature request for given endpoint.
 * 
 * \param   endpoint    Endpoint to handle
 * \param   request     Set/Clear feature flag
 * \return  Current endpoint halt status
 * 
 */
bool usb_halt(unsigned char endpoint, unsigned char request)
{
    // Mask endpoint number, direction bit is not used
    // see USB v2.0 chapter 9.3.4
    endpoint &= 0x0F;    
    
    // Clear the Halt feature of the endpoint if it is enabled
    if (request == USB_CLEAR_FEATURE) {

        TRACE_USB("Unhalt%d   ", endpoint);

        // Return endpoint to Idle state
        pEndpoint[endpoint]->dState = endpointStateIdle;

        // Clear FORCESTALL flag
        usb_ep_clr_flag(pUDP, endpoint, AT91C_UDP_FORCESTALL);

        // Reset Endpoint Fifos, beware this is a 2 steps operation
        SET(pUDP->UDP_RSTEP, 1 << endpoint);
        CLEAR(pUDP->UDP_RSTEP, 1 << endpoint);
    }
    // Set the Halt feature on the endpoint if it is not already enabled
    // and the endpoint is not disabled
    else
    if ((request == USB_SET_FEATURE)
             && (pEndpoint[endpoint]->dState != endpointStateHalted)
             && (pEndpoint[endpoint]->dState != endpointStateDisabled))
    {

        TRACE_USB("Halt%d   ", endpoint);

        // Abort the current transfer if necessary
        usb_end_of_transfer(endpoint, USB_STATUS_ABORTED);

        // Put endpoint into Halt state
        usb_ep_set_flag(pUDP, endpoint, AT91C_UDP_FORCESTALL);
        pEndpoint[endpoint]->dState = endpointStateHalted;

        // Enable the endpoint interrupt
        SET(pUDP->UDP_IER, 1 << endpoint);
    }
    
    // Return the endpoint halt status
    if (pEndpoint[endpoint]->dState == endpointStateHalted)
        return true;
    else
        return false;

}


/**
 * Changes the device state from Address to Configured, or from
 * Configured to Address.
 * 
 * This method directly access the last received SETUP packet to
 * decide on what to do.
 * 
 */
void usb_set_configuration(void)
{
    unsigned int wValue = pSetup->wValue;
    uint8_t ep;

    TRACE_USB("SetCfg(%d)   ",wValue);

    // Check the request
    if (wValue != 0)
    {
        // Enter Configured state
        SET(usb_device_state, USB_STATE_CONFIGURED);
        SET(pUDP->UDP_GLBSTATE, AT91C_UDP_CONFG);
    }
    else
    {
        // Go back to Address state
        CLEAR(usb_device_state, USB_STATE_CONFIGURED);
        SET(pUDP->UDP_GLBSTATE, AT91C_UDP_FADDEN);
        // For each endpoint, if it is enabled, disable it
        // Control endpoint 0 is not disabled
        for (ep = 1; ep < NUM_OF_ENDPOINTS; ep++) {
            usb_end_of_transfer(ep, USB_STATUS_RESET);
            pEndpoint[ep]->dState = endpointStateDisabled;
        }
    }
}
