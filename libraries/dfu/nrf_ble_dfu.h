#ifndef NRF_BLE_DFU_H__
#define NRF_BLE_DFU_H__

#include <stdint.h>
#include "ble_gatts.h"
#include "ble.h"
#include "app_timer.h"


#ifdef __cplusplus
extern "C" {
#endif

    // This is a 16-bit UUID.
#define BLE_DFU_SERVICE_UUID                 0xFE59                       //!< The UUID of the DFU Service.

// These UUIDs are used with the Nordic base address to create a 128-bit UUID (0x8EC9XXXXF3154F609FB8838830DAEA50).
#define BLE_DFU_CTRL_PT_UUID                 0x0001                       //!< The UUID of the DFU Control Point.
#define BLE_DFU_PKT_CHAR_UUID                0x0002                       //!< The UUID of the DFU Packet Characteristic.


/**@brief   BLE DFU opcodes.
 *
 * @details These types of opcodes are used in control point access.
 */
    typedef enum
    {
        BLE_DFU_OP_CODE_CREATE_OBJECT = 0x01,                                 /**< Value of the opcode field for a 'Create object' request. */
        BLE_DFU_OP_CODE_SET_RECEIPT_NOTIF = 0x02,                                 /**< Value of the opcode field for a 'Set Packet Receipt Notification' request. */
        BLE_DFU_OP_CODE_CALCULATE_CRC = 0x03,                                 /**< Value of the opcode field for a 'Calculating checksum' request. */
        BLE_DFU_OP_CODE_EXECUTE_OBJECT = 0x04,                                 /**< Value of the opcode field for an 'Initialize DFU parameters' request. */
        BLE_DFU_OP_CODE_SELECT_OBJECT = 0x06,                                 /**< Value of the opcode field for a 'Select object' request. */
        BLE_DFU_OP_CODE_RESPONSE = 0x60                                  /**< Value of the opcode field for a response.*/
    } ble_dfu_op_code_t;


    /**@brief   DFU Service.
     *
     * @details This structure contains status information related to the service.
     */
    typedef struct
    {
        uint16_t                     service_handle;                        /**< Handle of the DFU Service (as provided by the SoftDevice). */
        uint8_t                      uuid_type;                             /**< UUID type assigned to the DFU Service by the SoftDevice. */
        ble_gatts_char_handles_t     dfu_pkt_handles;                       /**< Handles related to the DFU Packet Characteristic. */
        ble_gatts_char_handles_t     dfu_ctrl_pt_handles;                   /**< Handles related to the DFU Control Point Characteristic. */
    } ble_dfu_t;


    /**@brief      Function for initializing the DFU Service.
     *
     * @retval     NRF_SUCCESS If the DFU Service and its characteristics were successfully added to the
     *             SoftDevice. Otherwise, an error code is returned.
     */
    uint32_t ble_dfu_transport_init(app_timer_id_t);


    /**@brief      Function for closing down the DFU Service and disconnecting from the host.
     *
     * @retval     NRF_SUCCESS If the DFU Service was correctly closed down.
     */
    uint32_t ble_dfu_transport_close(void);

#ifdef __cplusplus
}
#endif

#endif // NRF_BLE_DFU_H__

/** @} */
