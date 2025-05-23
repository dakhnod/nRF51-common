#include "ble_dfu.h"
#include "nrf_log.h"
#include <string.h>
#include "ble_hci.h"
#include "sdk_macros.h"
#include "ble_srv_common.h"
#include "nrf_dfu_settings.h"
#include "bootloader_secret.h"
#include "app_timer.h"
#include "sensor_timer.h"

#if FAMILY == 52
#include "nrf_fstorage.h"
#endif

#define MAX_CTRL_POINT_RESP_PARAM_LEN 3

#define REBOOT_TIMEOUT APP_TIMER_TICKS_COMPAT(500, APP_TIMER_PRESCALER)

ble_dfu_t *p_m_dfu;

uint8_t bootloader_secret[] = BOOTLOADER_SECRET;
APP_TIMER_DEF(reboot_timer);

#if FAMILY == 51
void flash_callback(fs_evt_t const *const evt, fs_ret_t result) {
    if (result == FS_SUCCESS) {
        NRF_LOG_INFO("Obtained settings, enter dfu is %d\n", s_dfu_settings.enter_buttonless_dfu);

        (void)sd_ble_gap_disconnect(p_m_dfu->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

        p_m_dfu->is_waiting_for_disconnection = true;
    }
}
#else
void flash_callback(void * buf) {
    NRF_LOG_INFO("Obtained settings, enter dfu is %d\n", s_dfu_settings.enter_buttonless_dfu);

    (void)sd_ble_gap_disconnect(p_m_dfu->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

    p_m_dfu->is_waiting_for_disconnection = true;
}
#endif

static void enter_bootloader(ble_dfu_t *p_dfu) {
    if (p_dfu->evt_handler != NULL) {
        ble_dfu_evt_t evt;

        evt.type = BLE_DFU_EVT_ENTERING_BOOTLOADER;

        p_dfu->evt_handler(p_dfu, &evt);
    }

    s_dfu_settings.enter_buttonless_dfu = true;

    (void)nrf_dfu_settings_write(flash_callback);

    NRF_LOG_DEBUG("reboot requested, rebooting...\n");
    ret_code_t err_code = app_timer_start(reboot_timer, REBOOT_TIMEOUT, NULL);
    APP_ERROR_CHECK(err_code);

    /*
    TODO:
     - Save bond data
    */
}


/**@brief Function for adding RX characteristic.
 *
 * @param[in] p_nus       Nordic UART Service structure.
 * @param[in] p_nus_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t rx_char_add(ble_dfu_t *p_dfu, const ble_dfu_init_t *p_dfu_init) {
    /**@snippet [Adding proprietary characteristic to S110 SoftDevice] */
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.notify = 1;
    char_md.char_props.write = 1;
    char_md.p_char_user_desc = NULL;
    char_md.p_char_pf = NULL;
    char_md.p_user_desc_md = NULL;
    char_md.p_cccd_md = &cccd_md;
    char_md.p_sccd_md = NULL;

    ble_uuid.type = p_dfu->uuid_type;
    ble_uuid.uuid = 0x0001;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 1;
    attr_md.vlen = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = 0;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = 23;

    return sd_ble_gatts_characteristic_add(p_dfu->service_handle,
        &char_md,
        &attr_char_value,
        &p_dfu->control_point_char);
    /**@snippet [Adding proprietary characteristic to S110 SoftDevice] */
}



uint32_t ble_dfu_init(ble_dfu_t *p_dfu, const ble_dfu_init_t *p_dfu_init) {
    uint32_t      err_code;
    ble_uuid_t    ble_uuid;
    ble_uuid128_t nus_base_uuid = BLE_DFU_BASE_UUID;

    VERIFY_PARAM_NOT_NULL(p_dfu);
    VERIFY_PARAM_NOT_NULL(p_dfu_init);

    p_m_dfu = p_dfu; // TODO: find a nicer solution to this

    // Initialize the service structure.
    p_dfu->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_dfu->evt_handler = p_dfu_init->evt_handler;
    p_dfu->is_waiting_for_disconnection = false;
    p_dfu->is_ctrlpt_notification_enabled = false;

    /**@snippet [Adding proprietary Service to S110 SoftDevice] */
    // Add a custom base UUID.
    err_code = sd_ble_uuid_vs_add(&nus_base_uuid, &p_dfu->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_dfu->uuid_type;
    ble_uuid.uuid = BLE_UUID_DFU_SERVICE;

    // Add the service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
        &ble_uuid,
        &p_dfu->service_handle);
    /**@snippet [Adding proprietary Service to S110 SoftDevice] */
    VERIFY_SUCCESS(err_code);

    // Add the RX Characteristic.
    err_code = rx_char_add(p_dfu, p_dfu_init);
    VERIFY_SUCCESS(err_code);

    #if FAMILY == 51
    err_code = nrf_dfu_flash_init(true);
    VERIFY_SUCCESS(err_code);
    
    nrf_dfu_settings_init();
    #else
    nrf_dfu_settings_init(true);
    #endif

    err_code = app_timer_create(
        &reboot_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        (app_timer_timeout_handler_t)NVIC_SystemReset
    );
    APP_ERROR_CHECK(err_code);

    return NRF_SUCCESS;
}

static void resp_send(ble_dfu_t *p_dfu, ble_dfu_buttonless_op_code_t op_code, ble_dfu_rsp_code_t rsp_code) {
    // Send notification
    uint16_t               hvx_len;
    uint8_t                hvx_data[MAX_CTRL_POINT_RESP_PARAM_LEN];
    ble_gatts_hvx_params_t hvx_params;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_len = 3;
    hvx_data[0] = DFU_OP_RESPONSE_CODE;
    hvx_data[1] = (uint8_t)op_code;
    hvx_data[2] = (uint8_t)rsp_code;

    hvx_params.handle = p_dfu->control_point_char.value_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len = &hvx_len;
    hvx_params.p_data = hvx_data;

    (void)sd_ble_gatts_hvx(p_dfu->conn_handle, &hvx_params);
}



/**@brief Handle write events to the Location and Navigation Service Control Point characteristic.
 *
 * @param[in]   p_dfu         DFU Service structure.
 * @param[in]   p_evt_write   Write event received from the BLE stack.
 */
static void on_ctrlpt_write(ble_dfu_t *p_dfu, ble_gatts_evt_write_t const *p_evt_write) {
    uint32_t      err_code;
    ble_dfu_rsp_code_t rsp_code = DFU_RSP_OPERATION_FAILED;

    ble_gatts_rw_authorize_reply_params_t write_authorize_reply;
    memset(&write_authorize_reply, 0, sizeof(write_authorize_reply));

    write_authorize_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;

    if (p_dfu->is_ctrlpt_notification_enabled) {
        write_authorize_reply.params.write.update = 1;
        write_authorize_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
    }
    else {
        write_authorize_reply.params.write.gatt_status = DFU_RSP_CCCD_CONFIG_IMPROPER;
    }

    // reply to the write authorization
    do {
        err_code = sd_ble_gatts_rw_authorize_reply(p_dfu->conn_handle, &write_authorize_reply);
    } while (err_code == NRF_ERROR_BUSY);


    if (write_authorize_reply.params.write.gatt_status != BLE_GATT_STATUS_SUCCESS) {
        return;
    }

    // Start executing the control point write action

    rsp_code = DFU_RSP_OP_CODE_NOT_SUPPORTED;
    uint8_t first = p_evt_write->data[0];

    if (p_evt_write->len == 16) {
        if (memcmp(p_evt_write->data, bootloader_secret, 16) == 0) {
            rsp_code = DFU_RSP_SUCCESS;
            first = BLE_DFU_ENTER_BOOTLOADER;
        }
    }

    /*
    switch (p_evt_write->data[0])
    {
        case BLE_DFU_ENTER_BOOTLOADER:
            rsp_code = DFU_RSP_SUCCESS;
            break;

         // Unrecognized Op Code
        default:
            rsp_code = DFU_RSP_OP_CODE_NOT_SUPPORTED;
            break;
    }
    */

    resp_send(p_dfu, (ble_dfu_buttonless_op_code_t)p_evt_write->data[0], rsp_code);

    if (rsp_code == BLE_DFU_ENTER_BOOTLOADER
        && first == BLE_DFU_ENTER_BOOTLOADER) {
        enter_bootloader(p_dfu);
    }
}


/**@brief Write authorization request event handler.
 *
 * @details The write authorization request event handler is called when writing to the control point.
 *
 * @param[in]   p_dfu     DFU structure.
 * @param[in]   p_ble_evt Event received from the BLE stack.
 */
static void on_rw_authorize_req(ble_dfu_t *p_dfu, ble_evt_t const *p_ble_evt) {
    if (p_ble_evt->evt.gatts_evt.conn_handle != p_dfu->conn_handle) {
        return;
    }

    const ble_gatts_evt_rw_authorize_request_t *p_auth_req =
        &p_ble_evt->evt.gatts_evt.params.authorize_request;

    if (
        (p_auth_req->type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
        &&
        (p_auth_req->request.write.handle == p_dfu->control_point_char.value_handle)
        &&
        (p_auth_req->request.write.op != BLE_GATTS_OP_PREP_WRITE_REQ)
        &&
        (p_auth_req->request.write.op != BLE_GATTS_OP_EXEC_WRITE_REQ_NOW)
        &&
        (p_auth_req->request.write.op != BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL)
        ) {
        on_ctrlpt_write(p_dfu, &p_auth_req->request.write);
    }

}

/**@brief Connect event handler.
 *
 * @param[in]   p_dfu       DFU Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_dfu_t *p_dfu, ble_evt_t const *p_ble_evt) {
    p_dfu->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Disconnect event handler.
 *
 * @param[in]   p_dfu      DFU Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_dfu_t *p_dfu, ble_evt_t const *p_ble_evt) {
    if (p_dfu->conn_handle != p_ble_evt->evt.gatts_evt.conn_handle) {
        return;
    }

    p_dfu->conn_handle = BLE_CONN_HANDLE_INVALID;

    if (p_dfu->is_waiting_for_disconnection) {
        NVIC_SystemReset();
    }
}

/**@brief Write event handler.
 *
 * @param[in]   p_dfu     DFU Service structure.
 * @param[in]   p_ble_evt Event received from the BLE stack.rtt
 */
static void on_write(ble_dfu_t *p_dfu, ble_evt_t const *p_ble_evt) {
    const ble_gatts_evt_write_t *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle != p_dfu->control_point_char.cccd_handle) {
        return;
    }

    if (p_evt_write->len == BLE_CCCD_VALUE_LEN) {
        // CCCD written, update indications state
        p_dfu->is_ctrlpt_notification_enabled = ble_srv_is_notification_enabled(p_evt_write->data);

        if (p_dfu->evt_handler != NULL) {
            ble_dfu_evt_t evt;

            if (p_dfu->is_ctrlpt_notification_enabled) {
                evt.type = BLE_DFU_EVT_INDICATION_ENABLED;;
            }
            else {
                evt.type = BLE_DFU_EVT_INDICATION_DISABLED;;
            }

            p_dfu->evt_handler(p_dfu, &evt);
        }
    }
}


void ble_dfu_on_ble_evt(ble_dfu_t *p_dfu, const ble_evt_t *p_ble_evt) {
    VERIFY_PARAM_NOT_NULL_VOID(p_dfu);
    VERIFY_PARAM_NOT_NULL_VOID(p_ble_evt);

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_dfu, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_dfu, p_ble_evt);
            break;

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            on_rw_authorize_req(p_dfu, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_dfu, p_ble_evt);
            break;

        default:
            // no implementation
            break;
    }

}
