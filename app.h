/***************************************************************************//**
 * @file
 * @brief Application interface provided to main().
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

#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include "nvm3_default.h"
#include "sl_bluetooth.h"

// Feature enable/disable flags
#define BONDING_ENABLE 1

#if BONDING_ENABLE
// NVM3 key for bonding validation
#define NVM3_KEY_BONDING_VALID    0x1000
#endif

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void);

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void);

#if BONDING_ENABLE
/**************************************************************************//**
 * Check if bonding data is valid.
 * Returns true if valid, false if corrupted or missing.
 *****************************************************************************/
bool app_check_bonding_valid(void);

/**************************************************************************//**
 * Mark bonding data as valid.
 * Called after successful bonding.
 *****************************************************************************/
void app_mark_bonding_valid(void);

/**************************************************************************//**
 * Handle bonding data corruption.
 * Notifies user and takes recovery action.
 *****************************************************************************/
void app_handle_bonding_corrupted(void);
#endif

#endif // APP_H
