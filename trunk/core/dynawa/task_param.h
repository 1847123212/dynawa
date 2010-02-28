#ifndef TASK_PARAM_H
#define TASK_PARAM_H

#define TASK_STACK_SIZE(s)  ((s) / 4)

#define TASK_STARTER_PRI    7
#define TASK_STARTER_STACK  1024

#define TASK_BUTTON_PRI     3
#define TASK_BUTTON_STACK   256

#define TASK_BT_MAIN_PRI     4
#define TASK_BT_MAIN_STACK   8192

#define TASK_BT_TX_PRI     TASK_BT_MAIN_PRI
#define TASK_BT_TX_STACK   1024

#define TASK_BT_RX_PRI     TASK_BT_MAIN_PRI
#define TASK_BT_RX_STACK   1024

#define TASK_LUA_PRI     1
#define TASK_LUA_STACK   8192

#endif // TASK_PARAM_H
