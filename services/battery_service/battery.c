#include "battery.h"
#include "nrf_delay.h"


#define INVALID_BATTERY_LEVEL 255


#define BATTERY_VOLTAGE_MIN 1800.0
#define BATTERY_VOLTAGE_MAX 3000.0

#define ADC_DELAY_US 20

uint16_t ble_bas_service_handle;
uint16_t ble_bas_level_handle;
uint16_t ble_bas_voltage_handle;
uint16_t ble_bas_connection_handle = BLE_CONN_HANDLE_INVALID;

static void on_connect(ble_evt_t * p_ble_evt)
{
    ble_bas_connection_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


static void on_disconnect(ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    ble_bas_connection_handle = BLE_CONN_HANDLE_INVALID;
}




uint16_t battery_voltage_get (void)
{
#ifdef NRF52
  static uint16_t result;
  NRF_SAADC->CH[0].CONFIG |= SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos;
  NRF_SAADC->CH[0].CONFIG |= SAADC_CH_CONFIG_MODE_SE         << SAADC_CH_CONFIG_MODE_Pos;
  NRF_SAADC->CH[0].CONFIG |= SAADC_CH_CONFIG_GAIN_Gain1_6    << SAADC_CH_CONFIG_GAIN_Pos;
  NRF_SAADC->CH[0].CONFIG |= SAADC_CH_CONFIG_BURST_Disabled  << SAADC_CH_CONFIG_BURST_Pos;
  NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELP_PSELP_VDD;
  NRF_SAADC->RESULT.PTR = (uint32_t) &result;
  NRF_SAADC->RESULT.MAXCNT = 1;
  NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;

  NRF_SAADC->TASKS_CALIBRATEOFFSET = 0;
  NRF_SAADC->TASKS_START = 0;
  NRF_SAADC->TASKS_SAMPLE = 0;
  NRF_SAADC->TASKS_STOP = 0;

  nrf_delay_us(ADC_DELAY_US); // the ADC seems to need these short breaks

  NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
  NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
  while(!NRF_SAADC->EVENTS_CALIBRATEDONE);

  nrf_delay_us(ADC_DELAY_US);

  NRF_SAADC->EVENTS_STARTED = 0;
  NRF_SAADC->TASKS_START = 1;
  while (!NRF_SAADC->EVENTS_STARTED);

  NRF_SAADC->EVENTS_END = 0;
  NRF_SAADC->TASKS_SAMPLE = 1;
  while (!NRF_SAADC->EVENTS_END);

  NRF_SAADC->EVENTS_STOPPED = 0;
  NRF_SAADC->TASKS_STOP = 1;
  while (!NRF_SAADC->EVENTS_STOPPED);

  return (uint16_t)(result * 0.6 /* REFERENCE */ * 6 /* GAIN */);
#else
  // Configure ADC
  NRF_ADC->CONFIG = (ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos) |
    (ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos) |
    (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos) |
    (ADC_CONFIG_PSEL_Disabled << ADC_CONFIG_PSEL_Pos) |
    (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
  NRF_ADC->EVENTS_END = 0;
  NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;

  NRF_ADC->EVENTS_END = 0;	// Stop any running conversions.
  NRF_ADC->TASKS_START = 1;

  while (!NRF_ADC->EVENTS_END);

  uint16_t vbg_in_mv = 1200;
  uint8_t adc_max = 255;
  uint16_t vbat_current_in_mv = (NRF_ADC->RESULT * 3 * vbg_in_mv) / adc_max;

  NRF_ADC->EVENTS_END = 0;
  NRF_ADC->TASKS_STOP = 1;
    
  return vbat_current_in_mv;
#endif
}

uint8_t level_get(uint16_t voltage){
  return (uint8_t) ((voltage -
		     BATTERY_VOLTAGE_MIN) / (BATTERY_VOLTAGE_MAX -
					     BATTERY_VOLTAGE_MIN) * 100.0);
}

uint8_t battery_level_get(void){
  return level_get(battery_voltage_get());
}

void on_authorize(ble_evt_t * p_ble_evt) {
    uint8_t * data = NULL;
    uint8_t len = 0;
    uint8_t level;
    
    ble_gatts_evt_read_t evt = p_ble_evt->evt.gatts_evt.params.authorize_request.request.read;
    
    uint16_t handle = evt.handle;

    if((handle != ble_bas_level_handle) && (handle != ble_bas_voltage_handle)){
        return;
    }
    
    uint16_t voltage = battery_voltage_get();
    
    if(handle == ble_bas_level_handle){
        len = 1;
        level = MIN(100, level_get(voltage));
        data = &level;
    }else if(handle == ble_bas_voltage_handle){
        len = 2;
        data = (uint8_t*) &voltage;
    }
    
    
    ble_gatts_rw_authorize_reply_params_t reply;
    
    reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
    reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
    reply.params.read.len = len;
    reply.params.read.offset = 0;
    reply.params.read.update = 1;
    reply.params.read.p_data = data;
    
    sd_ble_gatts_rw_authorize_reply(ble_bas_connection_handle, &reply);
}


void ble_bas_on_ble_evt(ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_ble_evt);
            break;
        
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            on_authorize(p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


static uint32_t battery_level_char_add()
{
    return ble_bas_battery_char_add(
        BLE_UUID_BATTERY_LEVEL_CHAR, 
        sizeof(uint8_t),
        &ble_bas_level_handle
    );
}

static uint32_t battery_voltage_char_add()
{
    return ble_bas_battery_char_add(
        0x3A19, 
        sizeof(uint16_t),
        &ble_bas_voltage_handle
    );
}

ret_code_t ble_bas_battery_char_add(uint16_t uuid, uint32_t size, uint16_t *handle){
    uint32_t            err_code;
    ble_uuid_t          ble_uuid;
    
    ble_gatts_char_md_t char_md = {
        .char_props.read   = 1,
        .char_props.notify = 0,
        .char_props.write  = 0
    };

    BLE_UUID_BLE_ASSIGN(ble_uuid, uuid);
    
    ble_gatts_attr_md_t attr_md = {
        .vloc       = BLE_GATTS_VLOC_STACK,
        .rd_auth    = 1,
        .wr_auth    = 0,
        .vlen       = 0
    };
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
    
    ble_gatts_attr_t attr_char_value = {
        .p_uuid    = &ble_uuid,
        .p_attr_md = &attr_md,
        .init_len  = size,
        .init_offs = 0,
        .max_len   = size,
    };
                                               
    ble_gatts_char_handles_t p_handles;

    err_code = sd_ble_gatts_characteristic_add(ble_bas_service_handle, 
                                               &char_md,
                                               &attr_char_value,
                                               &p_handles
    );
    
    *handle = p_handles.value_handle;
    return err_code;
}

uint32_t ble_bas_init()
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_BATTERY_SERVICE);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &ble_bas_service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add battery level characteristic
    battery_level_char_add();
    battery_voltage_char_add();
    
    return NRF_SUCCESS;
}
