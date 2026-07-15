#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "vl53l9.h"
#include "vl53l9_interface.h"
#include "vl53l9_platform.h"
#include "vl53l9_utils.h"

int main() {
    int status = 0;
    vl53l9_device_t dev;
    memset(&dev, 0, sizeof(vl53l9_device_t));

    printf("=========================================================\n");
    printf(" STMicroelectronics VL53L9CX Direct I2C Console Ranging  \n");
    printf("=========================================================\n\n");

    // Locked to the ribbon cable interface bus
    printf("[STEP 1/5] Opening camera I2C control bus (/dev/i2c-10)...\n");
    int fd = open("/dev/i2c-10", O_RDWR);
    if (fd < 0) {
        perror("[-] CRITICAL ERROR: Unable to open /dev/i2c-10.");
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, 0x29) < 0) {
        perror("[-] Slave address configuration failed");
        close(fd);
        return 1;
    }

    // Populate ST hardware configurations
    dev.bus = (void*)(intptr_t)fd;
    dev.bus_type = PLATFORM_BUS_I2C;
    dev.address = 0x52;          
    dev.ext_clock = 12000000;    
    dev.vdda = VDDA_2V8;         
    dev.vddio = VDDIO_1V8;       

    vl53l9_set_com_config(&dev, dev.address, 0);
    printf("[+] Bus open. Struct initialized.\n");

    uint8_t current_fsm = 0;
    vl53l9_read8(&dev, 0x008C, &current_fsm);

    int skip_init = 0;
    int skip_start = 0;

    if (current_fsm == 0x03) {
        printf("[+] SENSOR STATE: Already actively STREAMING data (0x03)!\n");
        skip_init = 1;
        skip_start = 1;
    } else if (current_fsm == 0x02) {
        printf("[+] SENSOR STATE: Already initialized and in STANDBY mode (0x02).\n");
        skip_init = 1;
    }

    if (!skip_init) {
        printf("\n[STEP 2/5] Initializing sensor (loading firmware & booting)...\n");
        status = vl53l9_init(&dev);
        if (status != 0) {
            printf("[-] CRITICAL ERROR: vl53l9_init() failed with code %d.\n", status);
            close(fd);
            return 1;
        }
        printf("[+] Sensor initialized and firmware loaded successfully.\n");
    }

    if (!skip_start) {
        printf("\n[STEP 3/5] Configuring tracking profiles...\n");
        vl53l9_set_binning(&dev, 0, 4); 
        vl53l9_set_frame_period(&dev, 66666); 
    }

    uint16_t buffer_size = 0;
    vl53l9_get_raw_buffer_size(4, &buffer_size);
    uint8_t *frame_buffer = (uint8_t *)malloc(buffer_size);

    if (!skip_start) {
        printf("\n[STEP 4/5] Sending system start streaming command...\n");
        status = vl53l9_start(&dev);
        if (status != 0) {
            printf("[-] CRITICAL ERROR: Could not start streaming.\n");
            free(frame_buffer);
            close(fd);
            return 1;
        }
    }

    printf("\n=========================================================\n");
    printf("[+] RANGING LOOP ACTIVE: Preparing terminal rendering...\n");
    printf("=========================================================\n\n");
    sleep(1);

    while (1) {
        uint8_t is_ready = 0;
        vl53l9_poll_frame(&dev, &is_ready);

        if (is_ready) {
            status = vl53l9_get_frame(&dev, frame_buffer, buffer_size);
            if (status == 0) {
                vl53l9_frame_t frame = { 0 };
                
                // Parse the raw byte buffer using ST's utility engine
                if (vl53l9_utils_parse_frame(frame_buffer, buffer_size, &frame) == 0) {
                    
                    // ANSI escape sequence to clear screen and home cursor for a steady live feed
                    printf("\033[H\033[J");
                    printf("===========================================================\n");
                    printf("   LIVE VL53L9CX ToF DEPTH MATRIX (Distances in mm)        \n");
                    printf("===========================================================\n\n");

                    int width = frame.p_metadata->frame_width;
                    int height = frame.p_metadata->frame_height;

                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            int idx = y * width + x;
                            uint16_t distance = frame.p_distance[idx].value;
                            uint16_t error_flag = frame.p_distance[idx].flag;

                            if (error_flag) {
                                printf(" --- "); // Display blank dashes for invalid/noise-filtered pixels
                            } else {
                                printf("%4d ", distance); // Render millimeter distance readout
                            }
                        }
                        printf("\n");
                    }

                    // Calculate and print the direct center focus distance
                    int center_idx = (height / 2) * width + (width / 2);
                    printf("\n-----------------------------------------------------------\n");
                    if (frame.p_distance[center_idx].flag) {
                        printf(" Focus Target: LOST/NOISE\n");
                    } else {
                        printf(" Focus Target Distance: %d mm\n", frame.p_distance[center_idx].value);
                    }
                    printf("===========================================================\n");
                }
            }
        }
        usleep(10000); 
    }

    vl53l9_stop(&dev);
    free(frame_buffer);
    close(fd);
    return 0;
}