/*******************************************************************************

				(C) COPYRIGHT Cambridge Silicon Radio

FILE
				main.c 

DESCRIPTION:
				Main program for (y)abcsp windows test program

REVISION:		$Revision: 1.1.1.1 $ by $Author: ca01 $
*******************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <conio.h>
#include "uSched.h"
#include "SerialCom.h"
#include "abcsp.h"
#include "abcsp_support_functions.h"
#include "hci.h"
#include "bccmd.h"
#include "rtos.h"

#include "lwip/pbuf.h"
#include "lwip/mem.h"
#include "lwip/sys.h"
#include "lwbt/hci.h"
#include "lwbt/rfcomm.h"
#include "bt.h"
#include "event.h"
#include "io.h"

#include "debug/trace.h"


#define SET_HOST_UART_HW_FLOW       0
#define SET_HOST_WAKE_UART_BREAK    0
#define SET_HOST_WAKE_PIO           1

#define SET_TX_POWER        0
#define TX_POWER            2

#define BT_COMMAND_QUEUE_LEN 10

//void bt_isr_wrapper();
void bt_isr(void *context);

xSemaphoreHandle bt_wakeup_semaphore;
xQueueHandle bt_wakeup_queue;

uint32_t bc_hci_event_count;
static Task bt_task_handle;
static xQueueHandle command_queue;
static Io bt_gpio;

static unsigned int bt_open_count = 0;

static unsigned long baudRate = 460800;
//static unsigned long baudRate = 115200;
//static unsigned long baudRate = 38400;

//#define USART_BAUDRATE_38400
//#define USART_BAUDRATE_115200
//#define USART_BAUDRATE_230400
//#define USART_BAUDRATE_460800
#define USART_BAUDRATE_921600

#if defined(USART_BAUDRATE_38400)
#define USART_BAUDRATE      38400
#define USART_BAUDRATE_CD   0x009d
#elif defined(USART_BAUDRATE_115200)
#define USART_BAUDRATE      115200
#define USART_BAUDRATE_CD   0x01d8
#elif defined(USART_BAUDRATE_230400)
#define USART_BAUDRATE      230400
#define USART_BAUDRATE_CD   0x03b0
#elif defined(USART_BAUDRATE_460800)
#define USART_BAUDRATE      460800
#define USART_BAUDRATE_CD    0x075f
#elif defined(USART_BAUDRATE_921600)
#define USART_BAUDRATE      921600
#define USART_BAUDRATE_CD    0x0ebf
#endif

static ps_setrq_count;
static struct bccmd_index_value {
    uint16_t index, value;
} ps_setrq[] = {
    {0, 9},
    {5, PSKEY_ANAFREQ},
    {6, 1},
    {8, 0x6590},

    {0, 9},
    {1, 9},
    {5, PSKEY_BAUDRATE},
    {6, 1},
    {8, USART_BAUDRATE_CD},
#if SET_HOST_UART_HW_FLOW
    {0, 9},
    {5, PSKEY_UART_CONFIG_BCSP},
    {6, 1},
    {8, 0x0802}, // hw flow on (default 0x0806)
#endif

#if SET_HOST_WAKE_UART_BREAK
    {0, 9},
    {5, PSKEY_UART_HOST_WAKE_SIGNAL},
    {6, 1},
    {8, 3}, /*  
        bit 0 - 3
        0 - repeated byte sequence (only for H4DS)
        1 - positive pulse on PIO
        2 - negative pulse on PIO
        3 - enable UART BREAK

        bit 4 - 7
            0 => PIO[0] 
            1 => PIO[1] 
            ...
            */
    {0, 12},
    {5, PSKEY_UART_HOST_WAKE},
    {6, 4},     
    {8, 0x0001},    // 1 enable, 4 - disable
    {9, 0x01f4},    /* Sleep_Delay = 500ms
        Sleep_Delay: Milliseconds after tx to host or rx from host,
after which host will be assumed to have gone into
deep sleep state. (Range 1 -> 65535)
When using BCSP or H5 host transports it is
recommended that this is greater than the
acknowledge delay (set by PSKEY_UART_ACK_TIMEOUT)
                    */

    {10, 0x0005},   /* Break_Length = 5ms (1 - 1000)
Duration of wake signal in milliseconds (Range 1 -> 1000)
                    */
    {11, 0x0020},   /* Pause_Length = 32ms (0 - 1000)
Pause_Length: Milliseconds between end of wake signal and sending data
to the host. (Range 0 -> 1000.)
                    */

#elif SET_HOST_WAKE_PIO
    {0, 9},
    {5, PSKEY_UART_HOST_WAKE_SIGNAL},
    {6, 1},
    {8, 0x1}, // enable PIO #0 POSITIVE EDGE (pppp0001)

    {0, 12},
    {5, PSKEY_UART_HOST_WAKE},
    {6, 4},
    {8, 0x0001},    // 1 enable, 4 - disable
    //{9, 0x01f4},    // sleep timeout = 500ms
    {9, 0x01f4},    // sleep timeout = 500ms
    {10, 0x0005},   // break len = 5ms
    {11, 0x0020},   // pause length = 32ms
#endif

#if SET_TX_POWER
    {0, 9},
    {5, PSKEY_LC_MAX_TX_POWER},
    {6, 1},
    {8, TX_POWER},

    {0, 9},
    {5, PSKEY_LC_DEFAULT_TX_POWER},
    {6, 1},
    {8, TX_POWER},

    {0, 9},
    {5, PSKEY_LC_MAX_TX_POWER_NO_RSSI},
    {6, 1},
    {8, TX_POWER},
#endif
    {0, 0}
};

/* -------------------- The task -------------------- */


typedef struct TransmitQueueEntryStructTag
{
	unsigned channel;
	unsigned reliableFlag;
	MessageBuffer * messageBuffer;
	struct TransmitQueueEntryStructTag * nextQueueEntry;
} TransmitQueueEntry;

static TransmitQueueEntry * transmitQueue;
int NumberOfHciCommands;

void queueMessage(unsigned char channel, unsigned reliableFlag, unsigned length, unsigned char * payload)
{
	MessageBuffer * messageBuffer;
// MV messageBuffer a messageBuffer->buffer freed later by abcsp_txmsg_done()
    TRACE_BT("queueMessage\r\n");
	messageBuffer = (MessageBuffer *) malloc(sizeof(MessageBuffer));
	messageBuffer->length = length;
	messageBuffer->buffer = payload;
	messageBuffer->index = 0;

	if (reliableFlag)
	{
        TRACE_BT("reliable flag on\r\n");
		if (transmitQueue)
		{
			TransmitQueueEntry * searchPtr;

            TRACE_BT("Message queued\r\n");
			for (searchPtr = transmitQueue; searchPtr->nextQueueEntry; searchPtr = searchPtr->nextQueueEntry)
			{
				;
			}
			searchPtr->nextQueueEntry = (TransmitQueueEntry *) malloc(sizeof(TransmitQueueEntry));
			searchPtr = searchPtr->nextQueueEntry;
			searchPtr->nextQueueEntry = NULL;
			searchPtr->channel = channel;
			searchPtr->reliableFlag = reliableFlag;
			searchPtr->messageBuffer = messageBuffer;
		}
		else
		{
            TRACE_BT("abcsp_sendmsg\r\n");
			if (!abcsp_sendmsg(&AbcspInstanceData, messageBuffer, channel, reliableFlag))
			{
                TRACE_BT("Message not delivered, queued\r\n");
				transmitQueue = (TransmitQueueEntry *) malloc(sizeof(TransmitQueueEntry));
				transmitQueue->nextQueueEntry = NULL;
				transmitQueue->channel = channel;
				transmitQueue->reliableFlag = reliableFlag;
				transmitQueue->messageBuffer = messageBuffer;
			}
		}
	}
	else /* unreliable - just send */
	{
		abcsp_sendmsg(&AbcspInstanceData, messageBuffer, channel, reliableFlag);
	}
}

static void pumpInternalMessage(void)
{
    TRACE_BT("pumpInternalMessage\r\n");
	while (transmitQueue)
	{
        TRACE_BT("abcsp_sendmsg\r\n");
		if (abcsp_sendmsg(&AbcspInstanceData, transmitQueue->messageBuffer, transmitQueue->channel, transmitQueue->reliableFlag))
		{
            TRACE_BT("sent. removed from queue\r\n");
			TransmitQueueEntry * tmpPtr;

			tmpPtr = transmitQueue;
			transmitQueue = tmpPtr->nextQueueEntry;

			free(tmpPtr);
		}
		else
		{
			break;
		}
	}
}

void phybusif_output(struct pbuf *p, u16_t len)
{
    /* Send pbuf on UART */
    //LWIP_DEBUGF(PHYBUSIF_DEBUG, ("phybusif_output: Send pbuf on UART\n"));
    
    unsigned char *t = p->payload;
    TRACE_BT("phybusif_output %d %d %d\r\n", len, t[0], t[1]);

    int channel;
    switch (t[0]) {
    case HCI_COMMAND_DATA_PACKET:
        channel = HCI_COMMAND_CHANNEL;
        break;
    case HCI_ACL_DATA_PACKET:
        channel = HCI_ACL_CHANNEL;
        break;
    default:
        TRACE_ERROR("Unknown packet type\r\n");
    }

    len--;
    u8_t *msg = malloc(len);
    if (msg == NULL) {
        TRACE_ERROR("NOMEM\r\n");
        panic();
        return;
    }
    // TODO: pbuf2buf()
    int remain = len;
    struct pbuf *q = p;
    u8_t *b = msg;
    int count = 0;
    while (remain) {
        if (q == NULL) {
            TRACE_ERROR("PBUF=NULL\r\n");
            panic();
            return;
        }
        int offset = count ? 0 : 1; // to ignore payload[0] = packet type

        int chunk_len = q->len - offset;
        TRACE_BT("pbuf len %d\r\n", chunk_len);
        int n = remain > chunk_len ? chunk_len : remain;
        memcpy(b, q->payload + offset, n);
        b += n;
        remain -= n;
        q = q->next;
        count++;
    }
{
/*
    int cmd = ((((uint16) msg[HCI_CSE_COMMAND_OPCODE_HIGH_BYTE]) << 8)
                     | ((uint16) msg[HCI_CSE_COMMAND_OPCODE_LOW_BYTE]));
*/
    int i;
    for(i = 0; i < len; i++) {
        TRACE_BT("%d=%x\r\n", i, msg[i]);
    }
}
    queueMessage(channel, 1, len, msg);
}

void BgIntPump(void)
{
    TRACE_BT("BgIntPump\r\n");

    int more_todo;
// loop by MV
    do {
TRACE_BT("abcsp_pumptxmsgs\r\n");
	    more_todo = abcsp_pumptxmsgs(&AbcspInstanceData);

	    pumpInternalMessage();
    } while (more_todo);
    // MV } while (0);
}

static unsigned char cmdIssueCount;

static void u_init_bt_task(void)
{
    TRACE_BT("u_init_bt_task\r\n");
	NumberOfHciCommands = 0;
	cmdIssueCount = 0;
}

static void pumpHandler(void);
static void restartHandler(void);
#define PUMP_INTERVAL	        1000000
#define BT_INTERVAL	            1000000

#if !defined(TCP_TMR_INTERVAL)
#define TCP_TMR_INTERVAL        250
#endif
#define TCP_INTERVAL	        (TCP_TMR_INTERVAL * 1000)

uint16 bc_state = BC_STATE_STOPPED;

static void u_bt_task(bt_command *cmd)
{
	unsigned char * readBdAddr;

    TRACE_BT("u_bt_task begin\r\n");

    if (cmd) {
        switch(cmd->id) {
        case BT_COMMAND_STOP:
            // TODO: stop all BT connections
            TerminateMicroSched();
            break;
        case BT_COMMAND_SEND:
            {
                TRACE_INFO("BT_COMMAND_SEND\r\n");
                bt_socket *sock = cmd->sock;
                struct pbuf *p = cmd->param.ptr;

                _bt_rfcomm_send(sock, p);

                pbuf_free(p);
            }
            break;
        case BT_COMMAND_SET_LINK_KEY:
            {
                TRACE_INFO("BT_COMMAND_SET_LINK_KEY\r\n");
                struct bt_bdaddr_link_key *bdaddr_link_key = cmd->param.ptr;

                hci_write_stored_link_key(&bdaddr_link_key->bdaddr, &bdaddr_link_key->link_key);

                free(bdaddr_link_key);
            }
            break;
        case BT_COMMAND_LINK_KEY_REQ_REPLY:
            {
                TRACE_INFO("BT_COMMAND_LINK_KEY_REQ_REPLY\r\n");
                struct bt_bdaddr_link_key *bdaddr_link_key = cmd->param.ptr;

                hci_link_key_request_reply(&bdaddr_link_key->bdaddr, &bdaddr_link_key->link_key);

                free(bdaddr_link_key);
            }
            break;
        case BT_COMMAND_LINK_KEY_REQ_NEG_REPLY:
            {
                TRACE_INFO("BT_COMMAND_LINK_KEY_REQ_NEG_REPLY\r\n");
                struct bd_addr *bdaddr = cmd->param.ptr;

                hci_link_key_request_neg_reply(bdaddr);

                free(bdaddr);
            }
            break;
        case BT_COMMAND_RFCOMM_LISTEN:
            {
                TRACE_INFO("BT_COMMAND_RFCOMM_LISTEN\r\n");
                bt_socket *sock = cmd->sock;

                _bt_rfcomm_listen(sock, cmd->param.cn);
            }
            break;
        case BT_COMMAND_RFCOMM_CONNECT:
            {
                TRACE_INFO("BT_COMMAND_RFCOMM_CONNECT\r\n");
                bt_socket *sock = cmd->sock;
                struct bt_bdaddr_cn *bdaddr_cn = cmd->param.ptr;

                _bt_rfcomm_connect(sock, &bdaddr_cn->bdaddr, bdaddr_cn->cn);

                free(bdaddr_cn);
            }
            break;
        case BT_COMMAND_FIND_SERVICE:
            {
                TRACE_INFO("BT_COMMAND_FIND_SERVICE\r\n");
                bt_socket *sock = cmd->sock;
                struct bd_addr *bdaddr = cmd->param.ptr;

                sock->current_cmd = cmd->id;
                _bt_find_service(sock, bdaddr);

                free(bdaddr);
            }
            break;
        case BT_COMMAND_INQUIRY:
            {
                TRACE_INFO("BT_COMMAND_INQUIRY\r\n");
                _bt_inquiry();
            }
            break;
        }
    }

	cmdIssueCount++;
	if (cmdIssueCount >= 100)
	{
		//printf(".");
		TRACE_INFO(".");
		cmdIssueCount = 0;
	}

	if (NumberOfHciCommands > 0)
	{
        uint16_t *bccmd;

		NumberOfHciCommands--;

        switch(bc_state) {
            case BC_STATE_READY:
                readBdAddr = malloc(3);
                readBdAddr[0] = (unsigned char) ((HCI_COMMAND_READ_BD_ADDR) & 0x00FF);
                readBdAddr[1] = (unsigned char) (((HCI_COMMAND_READ_BD_ADDR) >> 8) & 0x00FF);
                readBdAddr[2] = 0;
                queueMessage(HCI_COMMAND_CHANNEL, 1, 3, readBdAddr);
                break;
            case BC_STATE_STARTED:
                {
                    uint16 *bccmd = NULL;
                    uint16_t len, size;
                    while(1) {
                        struct bccmd_index_value *ps_setrq_value = &ps_setrq[ps_setrq_count];        
                        if (ps_setrq_value->index) {
                            bccmd[ps_setrq_value->index] = ps_setrq_value->value;
                            ps_setrq_count++;
                        } else if (bccmd) {
                            queueMessage(BCCMD_CHANNEL, 1, size, bccmd);
                            break;
                        } else { 
                            len = ps_setrq_value->value;
                            size = sizeof(uint16_t) * len;
                            bccmd = malloc(size);
                            if (bccmd == NULL) {
                                panic();
                            }
                            memset(bccmd, 0, size);

                            bccmd[0] = BCCMDPDU_SETREQ;
                            bccmd[1] = len;         // number of uint16s in PDU
                            bccmd[2] = ps_setrq_count;    // value choosen by host
                            bccmd[3] = BCCMDVARID_PS;
                            bccmd[4] = BCCMDPDU_STAT_OK;

                            ps_setrq_count++;
                        }
                    }
                }
                break;
            case BC_STATE_PS_SET:
                bccmd = malloc(sizeof(uint16) * 9);
                //bccmd[0] = 0;         // BCCMDPDU_GETREQ
                bccmd[0] = BCCMDPDU_SETREQ;
                bccmd[1] = 9;         // number of uint16s in PDU
                bccmd[2] = 3;    // value choosen by host
                //bccmd[3] = BCCMDVARID_CHIPVER;
                bccmd[3] = BCCMDVARID_WARM_RESET;
                bccmd[4] = BCCMDPDU_STAT_OK;
                bccmd[5] = 0;         // emty
                // bccmd[6-8]         // ignored, zero padding
                bc_state = BC_STATE_RESTARTING;
                TRACE_INFO("RESTARTING BC\r\n");
                queueMessage(BCCMD_CHANNEL, 1, sizeof(uint16) * 9, bccmd);
                StartTimer(250000, restartHandler);
                break;
            default:
                panic();
        }
	}
    TRACE_BT("u_bt_task end\r\n");
}

static void pumpHandler(void)
{
    TRACE_INFO("pumpHandler\r\n");
    BgIntPump();
	StartTimer(PUMP_INTERVAL, pumpHandler);
}

static int btiptmr = 0;

static void btHandler(void) {
    l2cap_tmr();
    rfcomm_tmr();
    bt_spp_tmr();

    //ppp_tmr();
    //nat_tmr();

    if(++btiptmr == 5/*sec*/) {
        //  bt_ip_tmr();
        btiptmr = 0;
    }
	StartTimer(BT_INTERVAL, btHandler);
}

static void tcpHandler(void) {
    tcp_tmr();
	StartTimer(TCP_INTERVAL, tcpHandler);
}

static void restartHandler()
{
    TRACE_INFO("BC RESTARTED\r\n");
    bc_state = BC_STATE_READY;
    bc_hci_event_count = 0;
    Serial_setBaud(0, USART_BAUDRATE); 
#if SET_HOST_UART_HW_FLOW
    Serial_setHandshaking(0, true);
#endif
    abcsp_init(&AbcspInstanceData);
#if defined(TCPIP)
    //echo_init();
    httpd_init();
#endif
    bt_spp_start();
    TRACE_INFO("Applications started.\r\n");

/*
    event ev;
    ev.type = EVENT_BT;
    ev.data.bt.type = EVENT_BT_STARTED;
    event_post(&ev);
*/

#if defined(TCPIP)
    StartTimer(TCP_INTERVAL, tcpHandler);
#endif
    StartTimer(BT_INTERVAL, btHandler);
}


/* -------------------- MAIN -------------------- */

#include <board.h>
volatile AT91PS_PIO  pPIOB = AT91C_BASE_PIOB;
volatile AT91PS_PIO  pPIOA = AT91C_BASE_PIOA;

void bt_task(void *p)
//int bcsp_main()
{
    TRACE_BT("bt_task %x\r\n", xTaskGetCurrentTaskHandle());

    bt_wakeup_queue = xQueueCreate(1, 2);
    vSemaphoreCreateBinary(bt_wakeup_semaphore);
    xSemaphoreTake(bt_wakeup_semaphore, -1);

    ledrgb_open();
    ledrgb_set(0x4, 0, 0, BT_LED_HIGH);

    // TODO sys_init();
#ifdef PERF
    perf_init("/tmp/minimal.perf");
#endif /* PERF */
#ifdef STATS
    stats_init();
#endif /* STATS */
    mem_init();
    memp_init();
    pbuf_init();
    TRACE_INFO("mem mgmt initialized\r\n");


#if defined(TCPIP)
    netif_init();
    ip_init();
    //udp_init();
    tcp_init();
    TRACE_INFO("TCP/IP initialized.\r\n");
#endif
    lwbt_memp_init();
    //phybusif_init(argv[1]);
    if(hci_init() != ERR_OK) {
        TRACE_ERROR("HCI initialization failed!\r\n");
        return -1;
    }
    l2cap_init();
    sdp_init();
    rfcomm_init();
#if defined(TCPIP)
    ppp_init();
#endif
    TRACE_INFO("Bluetooth initialized.\r\n");

	InitMicroSched(u_init_bt_task, u_bt_task);

	UartDrv_RegisterHandlers();
	UartDrv_Configure(baudRate);

/* BC4
-BCRES - PB30
BCBOOT0 - PB23 -> BC4 PIO0
BCBOOT1 - PB25 -> BC4 PIO1
BCBOOT2 - PB29 -> BC4 PIO4

PIO[0]
PIO[1]
PIO[4]
Host Transport
Auto System Clock Adaptation
Auto Baud Rate Adaptation
0
0
0
BCSP (default) (a)
Available (b)
Available (c)
0
0
1
BCSP with UART configured to use 2 stop bits and no parity
Available (b)
Available (c)
0
1
0
USB, 16 MHz crystal (d)
Not available
Not appropriate
0
1
1
USB, 26 MHz crystal (d)
Not available
Not appropriate
1
0
0
Three-wire UART
Available (b)
Available (c)
1
0
1
H4DS
Available (b)
Available (c)
1
1
0
UART (H4)
Available (b)
Available (c)
1
1
1
Undefined
-
-

Petr: takze nejprve drzet v resetu a potom nastavit piny BCBOOT0:2 na jaky protokol ma BC naject, pak -BCRES do 1

#define BCBOOT0_MASK (1 << 23)  // BC4 PIO0
#define BCBOOT1_MASK (1 << 25)  // BC4 PIO1
#define BCBOOT2_MASK (1 << 29)  // BC4 PIO4
#define BCNRES_MASK (1 << 30)

#define BC_WAKEUP_MASK (1 << 23)  // BC4 PIO0

//MV CTS/RTS
    pPIOA->PIO_PDR = (1 << 7);
    pPIOA->PIO_PDR = (1 << 8);
*/

    pPIOB->PIO_PER = BCBOOT0_MASK;
    pPIOB->PIO_OER = BCBOOT0_MASK;
    pPIOB->PIO_CODR = BCBOOT0_MASK; //set to log0

    pPIOB->PIO_PER = BCBOOT1_MASK;
    pPIOB->PIO_OER = BCBOOT1_MASK;                                         
    pPIOB->PIO_CODR = BCBOOT1_MASK; //set to log0

    pPIOB->PIO_PER = BCBOOT2_MASK;
    pPIOB->PIO_OER = BCBOOT2_MASK;
    pPIOB->PIO_CODR = BCBOOT2_MASK; //set to log0

    //TRACE_INFO("B_PIO_ODSR %x\r\n", pPIOB->PIO_ODSR);

    pPIOB->PIO_PER = BCNRES_MASK;
    pPIOB->PIO_OER = BCNRES_MASK;
    pPIOB->PIO_CODR = BCNRES_MASK; //set to log0

    //TRACE_INFO("B_PIO_ODSR %x\r\n", pPIOB->PIO_ODSR);
    //TRACE_INFO("B_PIO_PSR %x\r\n", pPIOB->PIO_PSR);

    Task_sleep(20);
/*
    pPIOB->PIO_SODR = BCNRES_MASK;
    Task_sleep(20);
    pPIOB->PIO_CODR = BCNRES_MASK; //set to log0
    Task_sleep(20);
*/
    pPIOB->PIO_SODR = BCNRES_MASK; // Run BC, run!
    //TRACE_INFO("B_PIO_ODSR %x\r\n", pPIOB->PIO_ODSR);

    Task_sleep(10);

    pPIOB->PIO_PDR = BCBOOT0_MASK | BCBOOT1_MASK | BCBOOT2_MASK;
#if 1
    Io_init(&bt_gpio, IO_PB23, IO_GPIO, INPUT);
    Io_addInterruptHandler(&bt_gpio, bt_isr, NULL);
    //AT91C_BASE_PIOB->PIO_IDR = BC_WAKEUP_MASK;
#else

    pPIOB->PIO_PER = BC_WAKEUP_MASK;
    pPIOB->PIO_ODR = BC_WAKEUP_MASK;
    pPIOB->PIO_IDR = BC_WAKEUP_MASK;

    //AIC_ConfigureIT(AT91C_ID_PIOB, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL | 3, bt_isr_wrapper);
    AIC_ConfigureIT(AT91C_ID_PIOB, AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE | 3, bt_isr_wrapper);
    pPIOB->PIO_IER = BC_WAKEUP_MASK;
    AT91C_BASE_AIC->AIC_IECR = 1 << AT91C_ID_PIOB;
#endif

    ps_setrq_count = 0;

    bc_state = BC_STATE_STARTED;

    TRACE_BT("BC restarted\r\n");
 
    if (!UartDrv_Start())
    {
        TerminateMicroSched();
    } else {
        //TRACE_INFO("A_PIO_OSR %x\r\n", pPIOA->PIO_OSR);
        //StartTimer(KEYBOARD_SCAN_INTERVAL, keyboardHandler);
	    abcsp_init(&AbcspInstanceData);
        //StartTimer(PUMP_INTERVAL, pumpHandler);
        MicroSched();
        UartDrv_Stop();
        CloseMicroSched();
    }

#if 0
    Io_removeInterruptHandler(&bt_gpio);
#else
    pPIOB->PIO_PDR = BC_WAKEUP_MASK;
#endif
    vQueueDelete(bt_wakeup_semaphore);
    vQueueDelete(bt_wakeup_queue);

    pPIOB->PIO_CODR = BCNRES_MASK; // BC Stop
    bc_state = BC_STATE_STOPPED;

    event ev;
    ev.type = EVENT_BT;
    ev.data.bt.type = EVENT_BT_STOPPED;
    event_post(&ev);

    ledrgb_set(0x4, 0, 0, 0x0);
    ledrgb_close();
    vTaskDelete(NULL);
}

bool bt_get_command(bt_command *cmd) {
    return xQueueReceive(command_queue, cmd, 0);
}


void bt_stop_callback() {
    vQueueDelete(command_queue);
}

uint16_t bt_buf_len(struct pbuf *p) {
    return p->len;
}

void *bt_buf_payload(struct pbuf *p) {
    return p->payload;
}

void bt_buf_free(struct pbuf *p) {
    pbuf_free(p);
}

void trace_bytes(char *text, uint8_t *bytes, int len) {
    int i;
    for (i = 0; i < len; i++) {
        TRACE_INFO("%s[%d] = %02x\r\n", text, i, bytes[i]);
    }
}

bool bt_is_ps_set(void) {
    struct bccmd_index_value *ps_setrq_value = &ps_setrq[ps_setrq_count];
    return !ps_setrq_value->index && !ps_setrq_value->value;
}

int bt_wait_for_data(void) {
    //TRACE_INFO("bt_wait_for_data\r\n");

    uint16_t event;
    if (bc_state == BC_STATE_READY) {
        xQueueReceive(bt_wakeup_queue, &event, portMAX_DELAY);
    }
/*
    if (bc_state == BC_STATE_READY) {
        AT91C_BASE_PIOB->PIO_IER = BC_WAKEUP_MASK;
        if ( xSemaphoreTake(bt_wakeup_semaphore, -1) != pdTRUE) {
            TRACE_BT("xSemaphoreTake err\r\n");
            return 0;
        }
    }
*/
    return 1;
}

// commands 

int bt_init() {
    bc_state = BC_STATE_STOPPED;
    return 0;
}

int bt_open() {

    if(!bt_open_count++) {
        bc_state = BC_STATE_STARTING;
        command_queue = xQueueCreate(BT_COMMAND_QUEUE_LEN, sizeof(bt_command));
        //bt_task_handle = Task_create( bt_task, "bt_main", TASK_BT_MAIN_STACK, TASK_BT_MAIN_PRI, NULL );
        xTaskCreate(bt_task, "bt_main", TASK_STACK_SIZE(TASK_BT_MAIN_STACK), NULL, TASK_BT_MAIN_PRI, &bt_task_handle);
        return BT_OK;
    }
    return BT_ERR_ALREADY_STARTED;
}

int bt_close() {

    if(bt_open_count && --bt_open_count == 0) {
        bt_command cmd;

        bc_state = BC_STATE_STOPPING;
        if (bt_task_handle == NULL) {
            return BT_OK;
        }
        cmd.id = BT_COMMAND_STOP;

        xQueueSend(command_queue, &cmd, portMAX_DELAY);
        scheduler_wakeup();
    }
    return BT_OK;
}

void bt_set_link_key(uint8_t *bdaddr, uint8_t *link_key) {
    bt_command cmd;

    TRACE_INFO("bt_set_link_key\r\n");

    struct bt_bdaddr_link_key *bdaddr_link_key = malloc(sizeof(struct bt_bdaddr_link_key)); 
    if (bdaddr_link_key == NULL) {
        return BT_ERR_MEM;
    }
    memcpy(&bdaddr_link_key->bdaddr, bdaddr, BT_BDADDR_LEN);
    memcpy(&bdaddr_link_key->link_key, link_key, BT_LINK_KEY_LEN);

    trace_bytes("bdaddr", bdaddr, BT_BDADDR_LEN);
    trace_bytes("linkkey", link_key, BT_LINK_KEY_LEN);

    cmd.id = BT_COMMAND_SET_LINK_KEY;
    cmd.param.ptr = bdaddr_link_key;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}

void bt_link_key_req_reply(uint8_t *bdaddr, uint8_t *link_key) {
    bt_command cmd;

    TRACE_INFO("bt_link_key_req_reply\r\n");

    struct bt_bdaddr_link_key *bdaddr_link_key = malloc(sizeof(struct bt_bdaddr_link_key)); 
    if (bdaddr_link_key == NULL) {
        return BT_ERR_MEM;
    }
    memcpy(&bdaddr_link_key->bdaddr, bdaddr, BT_BDADDR_LEN);
    memcpy(&bdaddr_link_key->link_key, link_key, BT_LINK_KEY_LEN);

    trace_bytes("bdaddr", bdaddr, BT_BDADDR_LEN);
    trace_bytes("linkkey", link_key, BT_LINK_KEY_LEN);

    cmd.id = BT_COMMAND_LINK_KEY_REQ_REPLY;
    cmd.param.ptr = bdaddr_link_key;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}

void bt_link_key_req_neg_reply(uint8_t *bdaddr) {
    bt_command cmd;

    TRACE_INFO("bt_link_key_req_neg_reply\r\n");

    struct bd_addr *cmd_bdaddr = malloc(sizeof(struct bd_addr)); 
    if (cmd_bdaddr == NULL) {
        return BT_ERR_MEM;
    }
    memcpy(cmd_bdaddr, bdaddr, BT_BDADDR_LEN);

    trace_bytes("bdaddr", bdaddr, BT_BDADDR_LEN);

    cmd.id = BT_COMMAND_LINK_KEY_REQ_NEG_REPLY;
    cmd.param.ptr = cmd_bdaddr;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}

void bt_inquiry() {
    bt_command cmd;

    TRACE_INFO("bt_inquiry\r\n");

    cmd.id = BT_COMMAND_INQUIRY;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}

// socket commands

void bt_rfcomm_listen(bt_socket *sock, uint8_t channel) {
    bt_command cmd;

    TRACE_INFO("bt_rfcomm_listen %x %d\r\n", sock, channel);

    cmd.id = BT_COMMAND_RFCOMM_LISTEN;
    cmd.sock = sock;
    cmd.param.cn = channel;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}

void bt_rfcomm_connect(bt_socket *sock, uint8_t *bdaddr, uint8_t channel) {
    bt_command cmd;

    TRACE_INFO("bt_rfcomm_connect %x %d\r\n", sock, channel);

    struct bt_bdaddr_cn *bdaddr_cn = malloc(sizeof(struct bt_bdaddr_cn)); 
    if (bdaddr_cn == NULL) {
        return BT_ERR_MEM;
    }
    memcpy(&bdaddr_cn->bdaddr, bdaddr, BT_BDADDR_LEN);
    bdaddr_cn->cn = channel;

    trace_bytes("bdaddr", bdaddr, BT_BDADDR_LEN);

    cmd.id = BT_COMMAND_RFCOMM_CONNECT;
    cmd.sock = sock;
    cmd.param.ptr = bdaddr_cn;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}

void bt_find_service(bt_socket *sock, uint8_t *bdaddr) {
    bt_command cmd;

    TRACE_INFO("bt_find_service %x\r\n", sock);

    struct bd_addr *cmd_bdaddr = malloc(sizeof(struct bd_addr)); 
    if (cmd_bdaddr == NULL) {
        return BT_ERR_MEM;
    }
    memcpy(cmd_bdaddr, bdaddr, BT_BDADDR_LEN);

    trace_bytes("bdaddr", bdaddr, BT_BDADDR_LEN);

    cmd.id = BT_COMMAND_FIND_SERVICE;
    cmd.sock = sock;
    cmd.param.ptr = cmd_bdaddr;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}

int bt_rfcomm_send(bt_socket *sock, const char *data, size_t len) {
    bt_command cmd;
    struct pbuf *p;

    //uint16_t len = strlen(data) + 1;
    
    TRACE_INFO("bt_rfcomm_send %s %d\r\n", data, len);
    p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
    if (p == NULL) {
        return BT_ERR_MEM;
    }
    //strcpy(p->payload, data);
    memcpy(p->payload, data, len);

    cmd.id = BT_COMMAND_SEND;
    cmd.sock = sock;
    cmd.param.ptr = p;

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    scheduler_wakeup();
    return BT_OK;
}
