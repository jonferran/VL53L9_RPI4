/**
 ******************************************************************************
 * @file    vl53l9_device.h
 * @author  IMD Software Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef VL53L9_DEVICE_H
#define VL53L9_DEVICE_H

#include "vl53l9_interface.h"

// NOTE: set the correct hardware configuration by defining one of the following symbols:
#define CONFIG_HW_STEVAL_MIPI
// #define CONFIG_HW_X_NUCLEO

#if !defined(CONFIG_HW_STEVAL_MIPI) && !defined(CONFIG_HW_X_NUCLEO)
#define CONFIG_HW_STEVAL_MIPI // fallback configuration if no symbol is defined
#endif

#if defined(CONFIG_HW_STEVAL_MIPI) && defined(CONFIG_HW_X_NUCLEO)
#define NB_DEVICES (2U)
#else
#define NB_DEVICES (1U)
#endif

extern vl53l9_device_t device[NB_DEVICES];

#endif // VL53L9_DEVICE_H
