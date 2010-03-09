#ifndef BT_H__
#define BT_H__

#include "task_param.h"
#include "lwbt/hci.h"
#include "lwbt/bd_addr.h"

#define BC_STATE_STOPPED             0
#define BC_STATE_STARTED             10
#define BC_STATE_ANAFREQ_SET         20
#define BC_STATE_BAUDRATE_SET        30
#define BC_STATE_LC_MAX_TX_POWER            50
#define BC_STATE_LC_DEFAULT_TX_POWER        51
#define BC_STATE_LC_MAX_TX_POWER_NO_RSSI    52
#define BC_STATE_RESTARTING          1000
#define BC_STATE_READY               10000

#define BT_COMMAND_STOP     1
#define BT_COMMAND_SEND     2
#define BT_COMMAND_SET_LINK_KEY 3
#define BT_COMMAND_INQUIRY 4
#define BT_COMMAND_SDP_SEARCH 5
#define BT_COMMAND_RFCOMM_CONNECT 6

#define BT_LED_LOW          0x40
#define BT_LED_HIGH         0xff

#define BT_OK                       0
#define BT_ERR_MEM                1
#define BT_ERR_ALREADY_STARTED    10

typedef struct {
    void *handle;
    struct pbuf *pbuf;
} bt_cmd_send;

typedef struct {
    uint8_t id;
    void *req;
    union {
        bt_cmd_send send;
        void *ptr;
    } data; 
} bt_command;

typedef union {
    struct {
        void *handle;
        void *pbuf;
    } data;
    void *ptr;
    struct {
        uint8_t cn;
    } sdp;
} bt_param;

typedef struct {
    void *req;
    bt_param param;
} bt_event;

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
