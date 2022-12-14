/**
 * Copyright (c) 2016 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include "nrf_dfu_transport.h"
#include "nrf_log.h"
#include "app_timer_appsh.h"


#define DFU_TRANS_SECTION_VARS_GET(i)       NRF_SECTION_VARS_GET((i), nrf_dfu_transport_t, dfu_trans)
#define DFU_TRANS_SECTION_VARS_COUNT        NRF_SECTION_VARS_COUNT(nrf_dfu_transport_t, dfu_trans)

 //lint -save -e19 -e526
NRF_SECTION_VARS_CREATE_SECTION(dfu_trans, const nrf_dfu_transport_t);
//lint -restore

uint32_t nrf_dfu_transports_init(const app_timer_id_t application_start_timer)
{
    uint32_t const num_transports = DFU_TRANS_SECTION_VARS_COUNT;
    uint32_t ret_val = NRF_SUCCESS;

    NRF_LOG_INFO("In nrf_dfu_transports_init\r\n");

    NRF_LOG_INFO("num transports: %d\r\n", num_transports);

    for (uint32_t i = 0; i < num_transports; i++)
    {
        nrf_dfu_transport_t * const trans = DFU_TRANS_SECTION_VARS_GET(i);
        ret_val = trans->init_func(application_start_timer);
        if (ret_val != NRF_SUCCESS)
        {
            break;
        }
    }

    NRF_LOG_INFO("After nrf_dfu_transports_init\r\n");

    return ret_val;
}


uint32_t nrf_dfu_transports_close(void)
{
    uint32_t const num_transports = DFU_TRANS_SECTION_VARS_COUNT;
    uint32_t ret_val = NRF_SUCCESS;

    NRF_LOG_INFO("In nrf_dfu_transports_close\r\n");

    NRF_LOG_INFO("num transports: %d\r\n", num_transports);

    for (uint32_t i = 0; i < num_transports; i++)
    {
        nrf_dfu_transport_t * const trans = DFU_TRANS_SECTION_VARS_GET(i);
        ret_val = trans->close_func();
        if (ret_val != NRF_SUCCESS)
        {
            break;
        }
    }

    NRF_LOG_INFO("After nrf_dfu_transports_init\r\n");

    return ret_val;
}
