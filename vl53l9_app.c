/**
 ******************************************************************************
 * @file    vl53l9_app.c
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

#include <stdio.h>
#include <stdlib.h>

#include "vl53l9.h"
#include "vl53l9_device.h"
#include "vl53l9_interface.h"
#include "vl53l9_utils.h"

#include "stm32n6xx.h" // for SCB_InvalidateDCache_by_Addr

#define CONF_DEVICE_ID   (0) /**< select device entry in platform descriptor array (see vl53l9_device.c) */
#define CONF_PRINT_FRAME (0) /**< enable printing depth frames as ascii art (slows performance) */
#define CONF_USECASE     (VL53L9_USECASE_AR_PRECISION) /**< select ranging profile to be applied (see vl53l9_utils.h) */

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static void print_frame(const vl53l9_frame_t frame);
static void handle_error(void);

__attribute__((aligned(32))) volatile uint8_t g_csi_output_buffer[14900]; /* 100 * 149 (csi_width * csi_height) */

void vl53l9_app() {

    int ret;
    vl53l9_device_t *p_dev = &device[CONF_DEVICE_ID];
    vl53l9_profile_t *p_profile = &g_ranging_profiles[CONF_USECASE];
    vl53l9_hw_config_t hw_config;

    uint8_t csi_width, csi_height;
    vl53l9_utils_get_csi_resolution(p_profile->binning, &csi_width, &csi_height);

    ret = platform_power_reset(CONF_DEVICE_ID);
    if (ret) {
        handle_error();
    }

    if (p_dev->bus_type & PLATFORM_BUS_I3C) {
        if (platform_assign_dynamic_address() != 0) {
            handle_error();
        }
    }

    ret = vl53l9_init(p_dev);
    if (ret) {
        handle_error();
    }

    vl53l9_utils_set_profile(p_dev, p_profile);

    /* retrieve and override output interface parameters */
    ret = vl53l9_get_hw_config(p_dev, &hw_config);
    if (ret) {
        handle_error();
    }

    hw_config.output_interface = VL53L9_OUTPUT_CSI2;
    hw_config.signaling_mode = true;
    hw_config.csi_data_rate = 1e9;
    hw_config.csi_virtual_channel = 0;
    hw_config.csi_status_line_force_width = false;
    hw_config.csi_status_line_datatype = 0x2A;
    hw_config.csi_frame_datatype = 0x2A;
    hw_config.csi_frame_height = csi_height - 1; /* no need to consider last row containing status line */
    hw_config.csi_frame_width = csi_width;

    ret = vl53l9_set_hw_config(p_dev, hw_config);
    if (ret) {
        handle_error();
    }

    ret = platform_start_csi_pipe((uint8_t *)g_csi_output_buffer);
    if (ret) {
        handle_error();
    }

    ret = vl53l9_start(p_dev);
    if (ret) {
        handle_error();
    }

    platform_profiler_enable();
    uint32_t start_time = platform_profiler_get_timestamp();
    uint32_t stop_time;
    float frame_rate;
    uint32_t previous_frame_counter = 0;

    platform_enable_event(PLATFORM_CAM_PIPE_FRAME_EVT);

    while (1) {
        ret = platform_wait_for_event(PLATFORM_CAM_PIPE_FRAME_EVT, 1000);
        if (ret) {
            handle_error();
        }

        platform_acknowledge_event(PLATFORM_CAM_PIPE_FRAME_EVT);

        /* invalidate cache to ensure data coherency (TODO: abstract this call) */
        SCB_InvalidateDCache_by_Addr((uint32_t *)g_csi_output_buffer, sizeof(g_csi_output_buffer));

        vl53l9_frame_t frame = { 0 };
        ret = vl53l9_utils_parse_frame((uint8_t *)g_csi_output_buffer, sizeof(g_csi_output_buffer), &frame);
        if (ret) {
            handle_error();
        }

        stop_time = platform_profiler_get_timestamp();
        frame_rate = (1.0f / (float)(platform_profiler_convert_to_us(stop_time - start_time))) * 1000000;
        start_time = stop_time;

#if CONF_PRINT_FRAME
        print_frame(frame);
#endif /* CONF_PRINT_FRAME */

        printf("Processed frame n. %lu @ %u fps  (missed frames = %d) \n", frame.p_metadata->frame_counter,
               (unsigned int)frame_rate, (int)(frame.p_metadata->frame_counter - 1 - previous_frame_counter));

        previous_frame_counter = frame.p_metadata->frame_counter;
    }
}

static void print_frame(const vl53l9_frame_t frame) {

    static const char ASCII_CHARS[] = "@%#*+=-:. ";

    printf("\033[%d;%dH", 0, 0); /* set cursor to the top of the screen */

    int pixel_step = 1;
    uint16_t min = UINT16_MAX;
    uint16_t max = 0;

    for (int i = 0; i < (frame.p_metadata->frame_height * frame.p_metadata->frame_width); i++) {
        uint16_t value = frame.p_distance[i].value;
        min = MIN(value, min);
        max = MAX(value, max);
    }

    uint16_t average = (max - min) * 0.05;
    min = MAX(min - average, 0);
    max = MIN(max + average, UINT16_MAX);

    for (int y = 0; y < frame.p_metadata->frame_height; y += pixel_step) {
        for (int x = 0; x < frame.p_metadata->frame_width; x += pixel_step) {
            int pixel_index = (y * frame.p_metadata->frame_width + x);
            uint16_t value = frame.p_distance[pixel_index].value;

            int ascii_index = (value - min) * (sizeof(ASCII_CHARS) - 1) / (max - min);
            ascii_index = MAX(0, MIN(ascii_index, sizeof(ASCII_CHARS) - 1));

            printf("%c", ASCII_CHARS[ascii_index]);
        }
        printf("\n");
    }
}

static void handle_error(void) {
    while (1)
        ;
}
