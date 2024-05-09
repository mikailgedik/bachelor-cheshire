// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Nicole Narr <narrn@student.ethz.ch>
// Christopher Reinwardt <creinwar@student.ethz.ch>
//
// Simple payload to test bootmodes

#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "pepe.h"

#define AXI2HDMI_BASE     ((void*)0x03009000)                           // Paper base address
#define CMD_IF_OFFSET     0x00000008                           // Paper's command interface's register size
#define POINTERQ          ( 0 * CMD_IF_OFFSET)
#define H_VTOT            ( 1 * CMD_IF_OFFSET)
#define H_VACTIVE         ( 2 * CMD_IF_OFFSET)
#define H_VFRONT          ( 3 * CMD_IF_OFFSET)
#define H_VSYNC           ( 4 * CMD_IF_OFFSET)
#define POWERREG          ( 5 * CMD_IF_OFFSET)
#define CURRENT_PTR       ( 6 * CMD_IF_OFFSET)
#define TEXT_BUFF_PARA    ( 7 * CMD_IF_OFFSET)
#define CURSOR_FONT_PARA  ( 8 * CMD_IF_OFFSET)
#define FIFO_REFILL_THRESHOLD  ( 9 * CMD_IF_OFFSET)
#define FIFI_MAX_REFILL_AMOUNT  ( 10 * CMD_IF_OFFSET)

#define FG_COLOR_PALETTE  0x400
#define BG_COLOR_PALETTE  0x800

const uint32_t is_in_text_mode = 1;
const uint32_t make_animation = 0;
const uint32_t find_best_params = 0;
const uint32_t arr = 0x81000000;

//For 40 MHz
const uint32_t pixtot = (1056<<16) + 628;
const uint32_t pixact = (800<<16) + 600;
const uint32_t front_porch = (40<<16) + 1;
const uint32_t sync_times = ((128<<16) + 4) | (1<<31) | (1<<15);

/*
//For 50 MHz; monitor does not like this
const uint32_t pixtot = (1040<<16) + 666;
const uint32_t pixact = (800<<16) + 600;
const uint32_t front_porch = (56<<16) + 37;
const uint32_t sync_times = ((120<<16) + 6) | (1<<31) | (1<<15);
*/

/*
//For 49.5 MHz; monitor does not like
const uint32_t pixtot = (1056<<16) + 625;
const uint32_t pixact = (800<<16) + 600;
const uint32_t front_porch = (16<<16) + 1;
const uint32_t sync_times = ((80<<16) + 3) | (1<<31) | (1<<15);
*/

/*
//For 25.175 MHz; monitor does not like (test further!)
const uint32_t pixtot = ((2*800)<<16) + 2*525;
const uint32_t pixact = ((2*640)<<16) + 2*480;
const uint32_t front_porch = ((2*16)<<16) + 2*10;
const uint32_t sync_times = (((2*96)<<16) + 2*2) | (1<<31) | (1<<15);
*/
const uint16_t cols = 90, rows = 35;

void rgb888_to_rgb565(const uint8_t* src, uint8_t* dest) {
  dest[0] = 0;
  dest[1] = 0;
  
  dest[1] |= (src[2] & 0xF8); //r
  dest[1] |= ((src[1] >> 5) & 0b111); //g
  dest[0] |= ((src[1] << 3) & 0xe0); //g
  dest[0] |= ((src[0] >> 3) & 0b11111);   //b

  dest[1] &= 0xff;
  dest[0] &= 0xff;
}

void rgb565_to_rgb888(const uint8_t* src, uint8_t* dest) {
  dest[0] = 0;
  dest[1] = 0;
  dest[2] = 0;

  dest[2] = src[1] & 0xF8;
  dest[1] = ((src[1] & 0x7) << 5) | ((src[0] >> 3) & 0b11100) ;
  dest[0] = src[0] << 3;

  dest[2] &= 0xff;
  dest[1] &= 0xff;
  dest[0] &= 0xff;
}

void wts(int idx, uint32_t val) {
    *reg32(&__base_regs, CHESHIRE_SCRATCH_0_REG_OFFSET + CHESHIRE_SCRATCH_1_REG_OFFSET * idx) = val;
}

uint32_t test_peripheral(uint32_t* const err, uint32_t ptr) {
    //Select PAPER_hw
    *reg32(&__base_regs, CHESHIRE_SCRATCH_0_REG_OFFSET + CHESHIRE_VGA_SELECT_REG_OFFSET) = 0x1;

    *reg32(AXI2HDMI_BASE, H_VTOT) = pixtot;
    *err = *reg32(AXI2HDMI_BASE, H_VTOT);
    if(*err != pixtot) {
        return 1;
    }

    *reg32(AXI2HDMI_BASE, H_VACTIVE) = pixact;
    *err = *reg32(AXI2HDMI_BASE, H_VACTIVE);
    if(*err != pixact) {
        return 2;
    }

    *reg32(AXI2HDMI_BASE, H_VFRONT) = front_porch;
    *err = *reg32(AXI2HDMI_BASE, H_VFRONT);
    if(*err != front_porch) {
        return 4;
    }

    *reg32(AXI2HDMI_BASE, H_VSYNC) = sync_times;
    *err = *reg32(AXI2HDMI_BASE, H_VSYNC);
    if(*err != sync_times) {
        return 5; 
    }
    
    *reg32(AXI2HDMI_BASE, TEXT_BUFF_PARA) = (cols << 16) | rows;

    //no init foreground/background?
    //bg/fg colors aready good

    //Set text mode
    *reg32(AXI2HDMI_BASE, POWERREG) = (0 | (is_in_text_mode << 16));
    *err = *reg32(AXI2HDMI_BASE, POWERREG);
    if(*err != (0 | (is_in_text_mode << 16))) {
        return 12;
    }

    wts(15, 50);
    //Bitmask to hold pointer!
    *reg32(AXI2HDMI_BASE, POINTERQ) = ptr | 0b010;
    *err = *reg32(AXI2HDMI_BASE, POINTERQ);
    wts(15, *err);
    wts(15, 60);
    /*
    if(*err != 1) {
        return 3;
        //TODO why this fail? :(
    }
    */

    //Values determined by trying
    *reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD) = 25;
    *reg32(AXI2HDMI_BASE, FIFI_MAX_REFILL_AMOUNT) = 100;

    *reg32(AXI2HDMI_BASE, POWERREG) = (1 | (is_in_text_mode << 16));
    *err = *reg32(AXI2HDMI_BASE, POWERREG);
    if(*err != (1 | (is_in_text_mode << 16))) {
        return 6;
    }
    //Clear err
    *err = 0;
    return 0;
}

void pack_pixels(volatile uint32_t * const target, const uint32_t source[4]) {
    target[0] = ((source[1] & 0xff) << 24) | source[0];
    target[1] = ((source[2] & 0xffff) << 16) | ((source[1] >> 8) & 0xffff);
    target[2] = ((source[3] << 8)) & ((source[2] >> 16) & 0xff);
}

void copy_pepe(volatile uint8_t * const target) {
    volatile uint8_t * dst = target;
    volatile uint8_t * src = pepe;
    
    for(int i = 0; i < 800 * 600; i++) {
        rgb888_to_rgb565(src, dst);
        dst += 2;
        src += 3;
    }
}

void init_memory(volatile uint8_t * const target) {
    if (is_in_text_mode) {
        volatile uint16_t * ptr = target;
        for(int i = 0; i < cols * rows; i++) {
            //Print every character I guess?
            //Bg and Fg use same color palette -> characters with same bg/fg color will not work
            *ptr = (((i / 16) % 0xff)<< 8) | (i % 0xff);
            ptr++;
        }
        wts(6, ((volatile uint32_t*)target)[1]);
    } else {
        for(int y = 0; y < 600; y++) {
            for(int x = 0; x < 800; x++) {
                volatile uint8_t * ptr = target + 2 * (x + y * 800);
                if (x >= 10 && x <= 20 && y >= 10 && y <= 20) {
                    ptr[0] = 0;
                    ptr[1] = 0b11111000;
                }
                //ptr[2] = x*x*y;

            }
        }

        wts(6, ((volatile uint32_t*)target)[1]);
    }

    //Make sure that RAM is updated
    fence();
}

void setpx(volatile uint8_t * ptr, uint32_t val) {
    ptr[0] = val;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
}

float sin(float x) {
    return x - x*x*x / 6 + x*x*x*x*x / 120;
}

void make_video(volatile uint8_t * const target, int time) {
    const float PI = 3.1415;
    const int divider = 50;
    time %= (divider + 1);
    time -= (divider / 2);

    int radius = (int)(sin(2 * PI * time / divider) * 50) + 200;
    if(radius < 0) {
        radius = -radius;
    }

    int radius2 = radius * radius;
    int orad = (radius + 5) * (radius + 5);
    
    for(int y = 300 - (radius + 5 + 2); y < 300 + (radius + 5 + 3); y++) {
        for(int x = 400 - (radius + 5 + 2); x < 400 + (radius + 5 + 3); x++) {
            volatile uint8_t * ptr = target + 3 * (x + y * 800);
            int xm = x - 400;
            int ym = y - 300;
            int d = xm*xm + ym*ym;
            if(radius2 > d) {
                setpx(ptr, 0x0000ff);
            } else if (orad > d) {
                setpx(ptr, 0x00ff00);
            } else {
                setpx(ptr, 0);
            }
        }
    }
    fence();
}

void floating_text(volatile uint16_t * target, int time) {
    char * text = "hello world";
    time /= 10;
    target += (strlen(text) * (time / 57)) % (rows * cols);
    while(*text != 0) {
        *target = ((time / 31) << 8) | *text; 
        text += 1;
        target += 1;
    }

    fence();
}

void set_text(volatile uint16_t * target, char * text) {
    while(*text != 0) {
        *target = ((0xf) << 8) | *text; 
        text += 1;
        target += 1;
    }

    fence();
}

void into_str(char* d, uint8_t s) {
    d[0] = (s / 100) % 10 + '0';
    d[1] = (s / 10) % 10 + '0';
    d[2] = (s / 1) % 10 + '0';
}

int main(void) {
    uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);

    volatile uint32_t * vga_disabler = reg32(&__base_regs, CHESHIRE_VGA_SELECT_REG_OFFSET);
    volatile uint32_t * clock_selector = reg32(&__base_regs, CHESHIRE_AXI2HDMI_CLOCK_CONFIG_REG_OFFSET);
    volatile uint32_t * refill_thrsh = reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD);
    volatile uint32_t * max_refill = reg32(AXI2HDMI_BASE, FIFI_MAX_REFILL_AMOUNT);
    *max_refill = 32;
    //Set clock divider to 2,3,ext clock, 5, 1
    *clock_selector = 0x0002;
    *clock_selector = 0x0003;
    *clock_selector = 0x0100; //Test external clock
    *clock_selector = 0x0005;
    *clock_selector = 0x0001;

    char str[] = "Hell!\r\n";
    uart_init(&__base_uart, reset_freq, __BOOT_BAUDRATE);
    /*
    uart_write_str(&__base_uart, str, sizeof(str));
    uart_write_flush(&__base_uart);
    */
    for(int i = 3; i < 16; i++)  {
        wts(i, -1);
    }
    init_memory(arr);
    //copy_pepe(arr);
    uint32_t err;
    uint32_t ret = test_peripheral(&err, (uint32_t) arr);
    
    wts(3, ret);
    wts(4, err);
    if(ret != 0) {
        return -1;
    }
    wts(5, *reg32(AXI2HDMI_BASE, CURRENT_PTR));

    if(make_animation) {
        if(is_in_text_mode == 0) {
            for(int x = 0; x < 0x80; x++) {
                make_video(arr, x);
            }
        } else {
            for(int x = 0; x < 0x10; x++) {
                floating_text(arr, x);
            }
        }
    }


    if(find_best_params) {
        uart_write_str(&__base_uart, str, sizeof(str));
        uart_write_flush(&__base_uart);
        
        volatile uint16_t * ptr = arr;
        char c = '9';
        uint32_t cntr = 0;
        while(c != 'q') {
            if(uart_read_ready(&__base_uart)) {
                char num [] = "abc\r\n";
                c = uart_read(&__base_uart);
                if(c >= '0' && c <= '9') {
                    *max_refill = 1 + (c - '0') * 2;
                } else if(c >= 'a' && c <= 'z') {
                    *refill_thrsh = 1 + (c - 'a') * 4;
                } else if(c == 'T') {
                    
                }
                char s1 [] = "Max refill: \r\n";
                char s2 [] = "Thresh: \r\n";

                uart_write_str(&__base_uart, s1, sizeof(s1));
                into_str(num , *max_refill);
                uart_write_str(&__base_uart, num, sizeof(num));
                uart_write_str(&__base_uart, s2, sizeof(s2));
                into_str(num , *refill_thrsh);
                uart_write_str(&__base_uart, num, sizeof(num));
                uart_write_flush(&__base_uart);
            }
            if((cntr++ / (10 *0xff0)) % 2 == 0) {
                *ptr = c + (0xf000);
            } else {
                *ptr = c + (0x0f00);
            }
            
            fence();
        }
    }
    

    //This is here so that the sim continues to go on a bit
    for(uint32_t i = 0; i < 0x80000; i++) {
        wts(7, i);
        //Play a bit around with maximal refill size
        //*max_refill = 5 + ((i / 0x00800) % 2) * 10;
        //Uncomment this to check whether peripheral can be replaced by axi2vga
        //*vga_disabler = (i / 0x00800) % 2;
    }

    *reg32(AXI2HDMI_BASE, POWERREG) = 0;
    char b [] = "done\r\n";
    uart_write_str(&__base_uart, b, sizeof(b));
    uart_write_flush(&__base_uart);
    return 0;
}
