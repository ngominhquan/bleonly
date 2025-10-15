/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "gatt_db.h"
#include "em_gpio.h"
#include <string.h>
#include "dbg.h"
// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static uint8_t conn_handle = 0xFF;

// Variables for delayed bonding
#if BONDING_ENABLE
static bool bonding_pending = false;
static uint32_t bonding_delay_counter = 0;
#define BONDING_DELAY_COUNT 30  // Approximately 300ms assuming app_process_action runs every ~10ms
#endif

// Company ID for Silicon Labs
#define SILABS_COMPANY_ID    0x0077

// Custom advertising data flags
#define ADV_FLAG_NEEDS_PAIRING   0x01

// Helper function to set advertising data with manufacturer specific data
static void set_adv_data_with_mfg_data(uint8_t custom_flags) {
    uint8_t adv_data[31];
    size_t adv_len = 0;

    // Add Flags (LE General Discoverable, BR/EDR disabled)
    adv_data[adv_len++] = 2;       // length
    adv_data[adv_len++] = 0x01;    // Flags AD type
    adv_data[adv_len++] = 0x06;    // Flags value (LE General, no BR/EDR)

    // Add Complete Local Name
    const char* device_name = "fanbandble";
    uint8_t name_len = strlen(device_name);
    adv_data[adv_len++] = name_len + 1;  // length (1 type + name length)
    adv_data[adv_len++] = 0x09;          // Complete Local Name AD type
    memcpy(&adv_data[adv_len], device_name, name_len);
    adv_len += name_len;

    // Add Manufacturer Specific Data
    adv_data[adv_len++] = 4;       // length (1 type + 2 company id + 1 data)
    adv_data[adv_len++] = 0xFF;    // AD type (Manufacturer Specific Data)
    adv_data[adv_len++] = (uint8_t)(SILABS_COMPANY_ID & 0xFF);        // Company ID LSB
    adv_data[adv_len++] = (uint8_t)((SILABS_COMPANY_ID >> 8) & 0xFF); // Company ID MSB
    adv_data[adv_len++] = custom_flags;  // rebond flag. 1: rebond neede; 0: normal

    // Set the advertising data
    sl_status_t sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle,
                                       0, // advertising data (not scan response)
                                       adv_len,
                                       adv_data);
    app_assert_status(sc);
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////

  GPIO_DbgSWDIOEnable(false);
  GPIO_DbgSWDClkEnable(false);

}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
  
#if BONDING_ENABLE
  // Check if bonding is pending and increment counter
  if (bonding_pending) {
    bonding_delay_counter++;
    
    // Check if delay count has been reached (approximately 300ms)
    if (bonding_delay_counter >= BONDING_DELAY_COUNT) {
      // Delay has passed, request bonding now
      app_log_mine("Bonding: requesting security increase on conn %d\r\n", conn_handle);
      sl_status_t sc = sl_bt_sm_increase_security(conn_handle);
      if (sc != SL_STATUS_OK) {
        app_log_mine("Bonding: increase_security failed, status=0x%04lx\r\n", sc);
      }
      app_assert_status(sc);
      
      // Clear the pending flag
      bonding_pending = false;
    }
  }
#endif
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.

//      sc = sl_bt_system_set_tx_power(190, 190, &min_pwr, &max_pwr);
//      app_assert_status(sc);

      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set advertising data with manufacturer specific data (no pairing needed)
      set_adv_data_with_mfg_data(0x00);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);

#if BONDING_ENABLE
      app_log_mine("Bonding: configuring security manager\r\n");
      sc = sl_bt_sm_configure(0x00, sm_io_capability_noinputnooutput);
      app_assert_status(sc);

//       sc=sl_bt_sm_delete_bondings();//test
//       app_assert_status(sc);

      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);

      // Maximum allowed bonding count: 8
      // New bonding will overwrite the bonding that was used the longest time ago
      sc = sl_bt_sm_store_bonding_configuration(1, 0x2);
      app_assert_status(sc);
      app_log_mine("Bonding: SM configured - bondable mode enabled\r\n");
#endif

      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      conn_handle = evt->data.evt_connection_opened.connection;
      app_log_mine("Connection opened: handle=%d, bonding=%d\r\n", 
                   conn_handle, evt->data.evt_connection_opened.bonding);
#if BONDING_ENABLE
      // Set a flag to delay bonding request and initialize counter
      bonding_pending = true;
      bonding_delay_counter = 0;
      app_log_mine("Bonding: delayed bonding scheduled\r\n");
#endif
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_mine("Connection closed: reason=0x%04x\r\n", 
                   evt->data.evt_connection_closed.reason);
      // Set advertising data with manufacturer specific data (no pairing needed)
      set_adv_data_with_mfg_data(0x00);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

#if BONDING_ENABLE
    case sl_bt_evt_sm_bonding_failed_id:
      app_log_mine("Bonding FAILED: conn=%d, reason=0x%04x\r\n",
                   evt->data.evt_sm_bonding_failed.connection,
                   evt->data.evt_sm_bonding_failed.reason);
      
      if (evt->data.evt_sm_bonding_failed.reason == 0x1006 ||  // PIN/Key missing
        evt->data.evt_sm_bonding_failed.reason == 0x1208 ||  // Command disallowed
        evt->data.evt_sm_bonding_failed.reason == 0x1205 ||  // Pairing not supported
        evt->data.evt_sm_bonding_failed.reason == 0x120B) {  // Authentication failed
        app_log_mine("Bonding: deleting all bondings due to error 0x%04x\r\n",
                     evt->data.evt_sm_bonding_failed.reason);
        sc = sl_bt_sm_delete_bondings();
        app_assert_status(sc);
      }    
      // Close the connection since bonding failed
      // sl_bt_connection_close(conn_handle);

      // Set advertising data with "needs pairing" flag
      set_adv_data_with_mfg_data(ADV_FLAG_NEEDS_PAIRING);
      app_log_mine("Bonding: advertising data updated with NEEDS_PAIRING flag\r\n");

      // Restart advertising
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_bonded_id:
      app_log_mine("Bonding SUCCESS: conn=%d, bonding=%d, security_mode=%d\r\n",
                   evt->data.evt_sm_bonded.connection,
                   evt->data.evt_sm_bonded.bonding,
                   evt->data.evt_sm_bonded.security_mode);
      break;

    case sl_bt_evt_sm_confirm_bonding_id:
      app_log_mine("Bonding: confirm_bonding requested on conn=%d, bonding=%d\r\n",
                   evt->data.evt_sm_confirm_bonding.connection,
                   evt->data.evt_sm_confirm_bonding.bonding_handle);
      // Confirm bonding request
      sc = sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
      app_assert_status(sc);
      app_log_mine("Bonding: confirm_bonding accepted\r\n");
      break;

    case sl_bt_evt_sm_confirm_passkey_id:
      app_log_mine("Bonding: passkey confirm requested on conn=%d, passkey=%lu\r\n",
                   evt->data.evt_sm_confirm_passkey.connection,
                   evt->data.evt_sm_confirm_passkey.passkey);
      // Auto-confirm passkey (for Just Works pairing)
      sc = sl_bt_sm_passkey_confirm(evt->data.evt_sm_confirm_passkey.connection, 1);
      app_assert_status(sc);
      app_log_mine("Bonding: passkey confirmed\r\n");
      break;
#endif

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
