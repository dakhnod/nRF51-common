#ifndef HOLYIOT_17095
#define HOLYIOT_17095

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

#define LEDS_NUMBER    0

#define LED_START      0
#define LED_STOP       0
   
#define LEDS_ACTIVE_STATE 0

#define LEDS_INV_MASK  LEDS_MASK

#define LEDS_LIST {  }

#define BUTTONS_NUMBER 0

#define BUTTON_START   0
#define BUTTON_STOP    0
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST {  }

// #define SER_CONN_CHIP_RESET_PIN     11    // Pin used to reset connectivity chip

// Low frequency clock source to be used by the SoftDevice
#define NRF_CLOCK_LFCLKSRC      {.source        = NRF_CLOCK_LF_SRC_RC,              \
                                 .rc_ctiv       = 16,                               \
                                 .rc_temp_ctiv  = 2,                                \
                                 .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_250_PPM};

#ifdef __cplusplus
}
#endif

#endif // PCA10040_H
