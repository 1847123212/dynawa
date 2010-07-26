#ifndef BT_H__
#define BT_H__

#include "task_param.h"
#include "lwbt/hci.h"
#include "lwbt/bd_addr.h"

#include "bt/bt_socket.h"

#define BC_STATE_STOPPED             0
#define BC_STATE_STOPPING            1
#define BC_STATE_STARTED             10
#define BC_STATE_STARTING            11
#define BC_STATE_ANAFREQ_SET         20
#define BC_STATE_BAUDRATE_SET        30
#define BC_STATE_UART_HOST_WAKE_SIGNAL      40
#define BC_STATE_UART_HOST_WAKE             41
#define BC_STATE_LC_MAX_TX_POWER            50
#define BC_STATE_LC_DEFAULT_TX_POWER        51
#define BC_STATE_LC_MAX_TX_POWER_NO_RSSI    52
#define BC_STATE_PS_SET          500
#define BC_STATE_RESTARTING          1000
#define BC_STATE_READY               10000

#define BT_COMMAND_STOP     1
#define BT_COMMAND_SEND     2
#define BT_COMMAND_SET_LINK_KEY 3
#define BT_COMMAND_INQUIRY 4
#define BT_COMMAND_FIND_SERVICE 5
#define BT_COMMAND_RFCOMM_CONNECT 6
#define BT_COMMAND_RFCOMM_LISTEN 7
#define BT_COMMAND_LINK_KEY_REQ_REPLY 8
#define BT_COMMAND_LINK_KEY_REQ_NEG_REPLY 9

#define BT_LED_LOW          0x40
#define BT_LED_HIGH         0xff

#define BT_OK                       0
#define BT_ERR_MEM                1
#define BT_ERR_ALREADY_STARTED    10

typedef struct {
    uint8_t id;
    bt_socket *sock;
    union {
        void *ptr;
        uint8_t cn;
    } param; 
} bt_command;

#define BT_LINK_KEY_LEN     16
#define BT_BDADDR_LEN      6

/*
struct bt_bd_addr {
    uint8_t addr[BT_BDADDR_LEN];
};
*/

struct bt_link_key {
    uint8_t key[BT_LINK_KEY_LEN];
};

struct bt_bdaddr_link_key {
    struct bd_addr bdaddr;
    struct bt_link_key link_key;
};

struct bt_bdaddr_cn {
    struct bd_addr bdaddr;
    uint8_t cn;
};

#endif /* BT_H__ */
