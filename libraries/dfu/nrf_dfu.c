#include "nrf_dfu.h"
#include "nrf_dfu_transport.h"
#include "nrf_dfu_utils.h"
#include "nrf_bootloader_app_start.h"
#include "nrf_dfu_settings.h"
#include "nrf_gpio.h"
#include "app_scheduler.h"
#include "app_timer_appsh.h"
#include "nrf_log.h"
#include "boards.h"
#include "nrf_bootloader_info.h"
#include "nrf_dfu_req_handler.h"

#define SCHED_MAX_EVENT_DATA_SIZE       MAX(APP_TIMER_SCHED_EVT_SIZE, 0)                        /**< Maximum size of scheduler events. */

#define SCHED_QUEUE_SIZE                20                                                      /**< Maximum number of events in the scheduler queue. */

#define APP_TIMER_PRESCALER             0                                                       /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                                       /**< Size of timer operation queues. */


#define APPLICATION_START_TIMEOUT      APP_TIMER_TICKS_COMPAT(2000, APP_TIMER_PRESCALER)
#define APPLICATION_START_TIMEOUT_LONG APP_TIMER_TICKS_COMPAT(60000, APP_TIMER_PRESCALER)

APP_TIMER_DEF(application_start_timer);

// Weak function implementation

/** @brief Weak implemenation of nrf_dfu_check_enter.
 *
 * @note    This function must be overridden to enable entering DFU mode at will.
 *          Default behaviour is to enter DFU when BOOTLOADER_BUTTON is pressed.
 */
__WEAK bool nrf_dfu_enter_check(void) {
    /*
    if (nrf_gpio_pin_read(BOOTLOADER_BUTTON) == 0)
    {
        return true;
    }
    */



    if (s_dfu_settings.enter_buttonless_dfu == 1) {
        s_dfu_settings.enter_buttonless_dfu = 0;
        APP_ERROR_CHECK(nrf_dfu_settings_write(NULL));
        return true;
    }
    return false;
}

void application_start_timer_handler(void *context) {
    NRF_LOG_INFO("application start timeout\n");
    if (nrf_dfu_app_is_valid()) {
        (void)nrf_dfu_transports_close();
        NVIC_SystemReset();
    }
    else {
        NRF_LOG_ERROR("no valid app\n");
    }
}


// Internal Functions

/**@brief Function for initializing the timer handler module (app_timer).
 */
static void timers_init(void) {
    // Initialize timer module, making it use the scheduler.
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, true);

    app_timer_create(
        &application_start_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        application_start_timer_handler
    );
}


/** @brief Function for event scheduler initialization.
 */
static void scheduler_init(void) {
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


static void wait_for_event() {
    // Transport is waiting for event?
    while (true) {
        // Can't be emptied like this because of lack of static variables
        app_sched_execute();
    }
}

void nrf_dfu_wait() {
    app_sched_execute();
}


uint32_t nrf_dfu_init() {
    uint32_t ret_val = NRF_SUCCESS;
    uint32_t enter_bootloader_mode = 0;

    NRF_LOG_INFO("In real nrf_dfu_init\r\n");

    nrf_dfu_settings_init();

    // Continue ongoing DFU operations
    // Note that this part does not rely on SoftDevice interaction
    ret_val = nrf_dfu_continue(&enter_bootloader_mode);
    if (ret_val != NRF_SUCCESS) {
        NRF_LOG_INFO("Could not continue DFU operation: 0x%08x\r\n");
        enter_bootloader_mode = 1;
    }

    // Check if there is a reason to enter DFU mode
    // besides the effect of the continuation
    if (nrf_dfu_enter_check()) {
        NRF_LOG_INFO("Application sent bootloader request\n");
        enter_bootloader_mode = 1;
    }

    bool bootloader_should_stay = enter_bootloader_mode;

    bool app_is_valid = nrf_dfu_app_is_valid();


    bool first_boot = (NRF_POWER->GPREGRET++) == 0;
    enter_bootloader_mode |= first_boot;

    if (enter_bootloader_mode != 0 || !app_is_valid) {
        timers_init();
        scheduler_init();

        // Initializing transports
        ret_val = nrf_dfu_transports_init(application_start_timer);
        if (ret_val != NRF_SUCCESS) {
            NRF_LOG_INFO("Could not initalize DFU transport: 0x%08x\r\n");
            return ret_val;
        }

        (void)nrf_dfu_req_handler_init();

        if (app_is_valid) {
            if(bootloader_should_stay){
                NRF_LOG_INFO("starting long bootloader timeout\n");
                app_timer_start(
                    application_start_timer,
                    APPLICATION_START_TIMEOUT_LONG,
                    NULL
                );
            }else{
                NRF_LOG_INFO("starting short bootloader timeout\n");
                app_timer_start(
                    application_start_timer,
                    APPLICATION_START_TIMEOUT,
                    NULL
                );
            }
        }

        // This function will never return
        NRF_LOG_INFO("Waiting for events\r\n");
        wait_for_event();
        NRF_LOG_INFO("After waiting for events\r\n");
    }

    if (nrf_dfu_app_is_valid()) {
        NRF_LOG_INFO("Jumping to: 0x%08x\r\n", MAIN_APPLICATION_START_ADDR);
        nrf_bootloader_app_start(MAIN_APPLICATION_START_ADDR);
    }

    // Should not be reached!
    NRF_LOG_INFO("After real nrf_dfu_init\r\n");
    return NRF_SUCCESS;
}
