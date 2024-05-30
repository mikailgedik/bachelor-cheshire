// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Nicole Narr <narrn@student.ethz.ch>
// Christopher Reinwardt <creinwar@student.ethz.ch>
//
// Simple payload to test bootmodes

#include "axi2hdmi_util/axi2hdmi.c"
#include "axi2hdmi_util/automatic_tester.c"

void interactive(uint32_t rtc_freq) {
    char str[] = "Hell!\r\n";
    uart_write_str(&__base_uart, str, sizeof(str));
    uart_write_flush(&__base_uart);
    
    volatile uint16_t * ptr = (volatile uint16_t *)arr;
    char c = '9';
    uint32_t cntr = 0;
    char num [] = "0000000000\r\n";
    while(c != 'q') {
        if(uart_read_ready(&__base_uart)) {
            c = uart_read(&__base_uart);
            uint32_t val;

            switch(c) {
                case 'a':
                    val = *reg32(AXI2HDMI_BASE, selected_offset);
                    val -= 1;
                    *reg32(AXI2HDMI_BASE, selected_offset) = val;
                    break;
                case 'd':
                    val = *reg32(AXI2HDMI_BASE, selected_offset);
                    val += 1;
                    *reg32(AXI2HDMI_BASE, selected_offset) = val;
                    break;
                case 'w':
                    if(selected_offset > 7) {
                        selected_offset -= 8;
                    }
                    break;
                case 's':
                    selected_offset += 8;
                    selected_offset %= 0x68;
                    break;
                case 'T':
                    //Stop hold mode
                    is_in_text_mode = !is_in_text_mode;

                    *reg32(AXI2HDMI_BASE, POWERREG) = (1 | (is_in_text_mode << 16));
                    *reg32(AXI2HDMI_BASE, POINTERQ) = ((uint32_t)(uint64_t)arr) | 0b110;
                    break;
                case 'M':
                    init_memory(arr);
                    break;
                case 'L':
                    //Bytes per pixel is 1, 2 or 3
                    bytes_per_pixel %= 3;
                    bytes_per_pixel++;
                    uint32_t pixel_format;
                    if(bytes_per_pixel == 1) {
                        pixel_format = 0x02030301;
                    } else if (bytes_per_pixel == 2) {
                        pixel_format = 0x05060502;
                    } else {
                        //3 bytes per pixel
                        pixel_format = 0x08080803;
                    }
                    *reg32(AXI2HDMI_BASE, PIXEL_FORMAT) = pixel_format;
                    break;
                case 'P':
                    //Bytes per pixel is 1, 2 or 3
                    //copy_pepe(arr);
                    break;
                case 'S':
                    automatic_tester(rtc_freq);
                    break;
                case 'O':
                    foreverypixel(arr, &colorcirc);
                    break;
                case 'U':
                    show_colors(arr);
                    break;
                case 'F':
                    uart_write_str(&__base_uart, into_str(num , arctan2(50, 70) * 1000), sizeof(num));
                    break;
                default: break;
            }

            write_params_to_screen((uint16_t*)arr);

        }
        
        sleep_intr(rtc_freq / 10);
        if(cntr++ % 2 == 0) {
            *ptr = c + (0xf000);
        } else {
            *ptr = c + (0x0f00);
        }
        
        fence();
    }
}

void simulation(volatile uint8_t* arr, uint32_t rtc_freq) {
    //One frame takes ~16.66ms
    uint32_t millis_per_frame = 17;
    sleep_intr(rtc_freq * (millis_per_frame / 2) / 1000);
    for(int i = 0; i < 4; i++) {
        wts(9, *reg32(AXI2HDMI_BASE, SYNC_FAIL_HAPPENED));
        wts(9, 0);
        *reg32(AXI2HDMI_BASE, SYNC_FAIL_HAPPENED) = 2434;
        sleep_intr(rtc_freq * millis_per_frame / 1000);
    }
}

int main(void) {
    uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);

    uart_init(&__base_uart, reset_freq, __BOOT_BAUDRATE);

    for(int i = 3; i < 16; i++)  {
        wts(i, -1);
    }
    
    for(int i = 0; i < 9; i++) {
        arr[i] = (i % 4 == 0) ? 0xff : 0;
    }
    for(int i = 0; i < 9; i++) {
        arr[3 * 480 * 640 - 10 + i] = (i % 4 == 0) ? 0xff : 0;
    }
    fence();

    uint32_t err;
    uint32_t ret = start_peripheral(&err, (uint32_t)(uint64_t)arr);
    
    wts(3, ret);
    wts(4, err);
    if(ret != 0) {
        return -1;
    }
    wts(5, *reg32(AXI2HDMI_BASE, CURRENT_PTR));

    if(is_interactive) {
        init_memory(arr);
        interactive(rtc_freq);
    } else {
        simulation(arr, rtc_freq);
    }

    char b [] = "done\r\n";
    uart_write_str(&__base_uart, b, sizeof(b));
    uart_write_flush(&__base_uart);
    return 0;
}
