Remove R25 from the STEVAL VL53L9 board to enable onboard clock

Build with the following command

gcc test_i2c_ranging.c vl53l9_platform.c vl53l9.c vl53l9_utils.c -o test_i2c_ranging -I. -lm -fshort-enums
