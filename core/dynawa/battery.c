#include "battery.h"
#include "io.h"
#include "debug/trace.h"
#include "event.h"
#include "task.h"
#include "queue.h"
#include "task_param.h"

#define PIO_USB_DETECT IO_PB22

static xTaskHandle battery_task_handle;
static gasgauge_stats _stats;
xQueueHandle battery_queue;

#define WAKEUP_EVENT_TIMED_EVENT    100
#define WAKEUP_EVENT_USB_HIGH       1000
#define WAKEUP_EVENT_USB_LOW        1001

static Io usb_io;

int battery_get_stats (gasgauge_stats *stats) {
    TRACE_INFO("battery_get_stats %x\r\n", stats);
    memcpy (stats, &_stats, sizeof(gasgauge_stats));
/*
    stats->voltage = _stats.voltage;
    stats->current = _stats.current;
    stats->state = _stats.state;
*/
    return 0;
}

#ifdef CFG_PM
void battery_timer_handler(void* context) {
    //TRACE_INFO("battery_timer_handler\r\n");

    portBASE_TYPE xHigherPriorityTaskWoken;
    uint8_t event = WAKEUP_EVENT_TIMED_EVENT;

    xQueueSendFromISR(battery_queue, &event, &xHigherPriorityTaskWoken);

    if(xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
#endif

static bool battery_usb_pin_high = false;

void battery_io_isr_handler(void* context) {
    battery_usb_pin_high = !battery_usb_pin_high;

    uint8_t ev;
    if (battery_usb_pin_high) {
        ev = WAKEUP_EVENT_USB_LOW;
    } else {
        ev = WAKEUP_EVENT_USB_HIGH;
    }
    TRACE_INFO("battery_io_isr_handler usb %d\r\n", battery_usb_pin_high);

    portBASE_TYPE xHigherPriorityTaskWoken;
    xQueueSendFromISR(battery_queue, &ev, &xHigherPriorityTaskWoken);

    if(xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void battery_task( void* p ) {
    TRACE_INFO("battery task %x\r\n", xTaskGetCurrentTaskHandle());

/*
#ifdef CFG_PM
    Timer timer;
    Timer_init(&timer, 0);
    Timer_setHandler(&timer, battery_timer_handler, NULL);
#endif
*/

    while (true) {
        gasgauge_get_stats(&_stats);
    
        if (_stats.voltage > 0) { 
            // TODO: BATTERY_STATE_CRITICAL
            // TODO: BATTERY_STATE_DISCHARGING

            if (_stats.state == GASGAUGE_STATE_NO_CHARGE) {
                if (_stats.voltage < 4050) {
                    gasgauge_charge(true);

                    event ev;
                    ev.type = EVENT_BATTERY;
                    ev.data.battery.state = BATTERY_STATE_CHARGING;
                    event_post(&ev);
                }
            } else if (_stats.state == GASGAUGE_STATE_CHARGED) {
                gasgauge_charge(false);

                event ev;
                ev.type = EVENT_BATTERY;
                ev.data.battery.state = BATTERY_STATE_CHARGED;
                event_post(&ev);
            }
        }
        Task_sleep(10000);
/*
        uint8_t battery_event;
#ifdef CFG_PM
        Timer_start(&timer, 10000, false, true);
        xQueueReceive(battery_queue, &battery_event, -1);
        Timer_stop(&timer);
#else
        xQueueReceive(battery_queue, &battery_event, 10000);
#endif
        if (battery_event == WAKEUP_EVENT_USB_LOW) {
            event ev;
            ev.type = EVENT_BATTERY;
            ev.data.battery.state = BATTERY_STATE_DICHARGING;
            event_post(&ev);
        }
*/
    }
}

int battery_init () {
    battery_queue = xQueueCreate(1, sizeof(uint8_t));

/*
    Io_init(&usb_io, PIO_USB_DETECT, IO_GPIO, INPUT);
    battery_usb_pin_high = Io_value(&usb_io);

    TRACE_INFO("battery_init() usb %d\r\n", battery_usb_pin_high);

    Io_addInterruptHandler(&usb_io, battery_io_isr_handler, NULL);
*/

    // TODO: don't start the task if no batter (_stats.voltage == 0)
    if (xTaskCreate( battery_task, "battery", TASK_STACK_SIZE(TASK_BATTERY_STACK), NULL, TASK_BATTERY_PRI, &battery_task_handle ) != 1 ) {
        return -1;
    }

    return 0;
}

