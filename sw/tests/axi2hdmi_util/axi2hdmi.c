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
#include "regs/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "pepe.h"

#define abs(x) (x > 0 ? x : -x)
#define max(x,y) (x > y ? x : y)
#define min(x,y) (x < y ? x : y)

//clint needed for the RTC
#define AXI2HDMI_BASE     ((void*)0x0300A000)                           // Paper base address
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
#define FIFO_MAX_REFILL_AMOUNT  ( 10 * CMD_IF_OFFSET)
#define PIXEL_FORMAT (11 * CMD_IF_OFFSET)
#define SYNC_FAIL_HAPPENED (12 * CMD_IF_OFFSET)
#define FG_COLOR_PALETTE  0x400
#define BG_COLOR_PALETTE  0x800

typedef struct {
    float r;       // a fraction between 0 and 1
    float g;       // a fraction between 0 and 1
    float b;       // a fraction between 0 and 1
} rgb_t;

typedef struct {
    float h;       // angle in degrees
    float s;       // a fraction between 0 and 1
    float v;       // a fraction between 0 and 1
} hsv_t;

static rgb_t   hsv2rgb(hsv_t in);

uint32_t selected_offset = FIFO_REFILL_THRESHOLD;

uint32_t bytes_per_pixel = 3;
uint32_t is_in_text_mode = 0;
const uint32_t is_interactive = 0;
volatile uint8_t * const arr = (volatile uint8_t*)(void*)0x81000000;

/*
//For 40 MHz
const uint32_t pixtot = (1056<<16) + 628;
const uint32_t pixact = (800<<16) + 600;
const uint32_t front_porch = (40<<16) + 1;
const uint32_t sync_times = ((128<<16) + 4) | (1<<31) | (1<<15);
*/
/*
//For 50 MHz; monitor does not like this
const uint32_t pixtot = (1040<<16) + 666;
const uint32_t pixact = (800<<16) + 600;
const uint32_t front_porch = (56<<16) + 37;
const uint32_t sync_times = ((120<<16) + 6) | (1<<31) | (1<<15);
*/
//For 25 MHz
const uint32_t pixtot = (800<<16) + 525;
const uint32_t pixact = (640<<16) + 480;
const uint32_t front_porch = (16<<16) + 10;
const uint32_t sync_times = ((96<<16) + 2) | (1<<31) | (1<<15);


const uint16_t cols = 96, rows = 36;

void stress_ram(uint16_t f) {
    volatile uint64_t* ptr = (volatile uint64_t*)arr;
    for(uint64_t i = 0; i < 0x00010000 * f; i++) {
        *ptr;
        ptr++;
    }
}

void rgb888_to_rgb565(const volatile uint8_t * const src_v, volatile uint8_t* const dest_v) {
  uint8_t src[] = {src_v[0], src_v[1], src_v[2]};
  uint8_t dest[2];
  dest[0] = 0;
  dest[1] = 0;
  
  dest[1] |= (src[2] & 0xF8); //r
  dest[1] |= ((src[1] >> 5) & 0b111); //g
  dest[0] |= ((src[1] << 3) & 0xe0); //g
  dest[0] |= ((src[0] >> 3) & 0b11111);   //b

  dest[1] &= 0xff;
  dest[0] &= 0xff;

  dest_v[0] = dest[0];
  dest_v[1] = dest[1];
}

void rgb888_to_rgb332(const uint8_t * const src, volatile uint8_t * const dest) {
  *dest = (src[2] & ~0b11111) |
            ((src[1] >> 3) & 0b11100) |
            ((src[0] >> 6)& 0b11);
}

void wts(int idx, uint32_t val) {
    *reg32(&__base_regs, CHESHIRE_SCRATCH_0_REG_OFFSET + CHESHIRE_SCRATCH_1_REG_OFFSET * idx) = val;
}

void write_params_to_screen(volatile uint16_t* dest);

uint32_t start_peripheral(uint32_t* const err, uint32_t ptr) {
    //Select PAPER_hw
    *reg32(&__base_regs, CHESHIRE_VGA_SELECT_REG_OFFSET) = 0x1;

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
    *err = *reg32(AXI2HDMI_BASE, PIXEL_FORMAT);
    if(*err != pixel_format) {
        return PIXEL_FORMAT;
    }

    //Set clock
    // = 0x0001 would tie it to internal clock with divider 1
    // = 0x0002 would tie it to internal clock with divider 2
    // = 0x0100 would tie it to external clock
    // etc.
    *reg32(&__base_regs, CHESHIRE_AXI2HDMI_CLOCK_CONFIG_REG_OFFSET) = 0x0002;

    *reg32(AXI2HDMI_BASE, H_VTOT) = pixtot;
    *err = *reg32(AXI2HDMI_BASE, H_VTOT);
    if(*err != pixtot) {
        return H_VTOT;
    }

    *reg32(AXI2HDMI_BASE, H_VACTIVE) = pixact;
    *err = *reg32(AXI2HDMI_BASE, H_VACTIVE);
    if(*err != pixact) {
        return H_VACTIVE;
    }

    *reg32(AXI2HDMI_BASE, H_VFRONT) = front_porch;
    *err = *reg32(AXI2HDMI_BASE, H_VFRONT);
    if(*err != front_porch) {
        return H_VFRONT;
    }

    *reg32(AXI2HDMI_BASE, H_VSYNC) = sync_times;
    *err = *reg32(AXI2HDMI_BASE, H_VSYNC);
    if(*err != sync_times) {
        return H_VSYNC; 
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
    *reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD) = 4;
    *reg32(AXI2HDMI_BASE, FIFO_MAX_REFILL_AMOUNT) = 4;

    //write_params_to_screen((uint16_t*)arr);

    *reg32(AXI2HDMI_BASE, POWERREG) = (1 | (is_in_text_mode << 16));
    *err = *reg32(AXI2HDMI_BASE, POWERREG);
    if(*err != (1 | (is_in_text_mode << 16))) {
        return 6;
    }
    //Clear err
    *err = 0;
    return 0;
}

rgb_t hsv2rgb(hsv_t in) {
    float      hh, p, q, t, ff;
    long        i;
    rgb_t         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}

float sqrt2(float x) {
    //taylor of sqrt(x+1/2)
    float one_divided_bz_sqrt_of_2 = 0.70710678118;
    x -= 0.5;

    float r = 1;
    r += x;
    r -= x * x / 2;
    r += x * x * x / 2;
    r -= x * x * x * x * 5 / 8;
    
    return r * one_divided_bz_sqrt_of_2;
}

float arctan2(float x, float y) {
    float a = min (abs(x), abs(y)) / max ((x), (y));
    float s = a * a;
    float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
    if (abs(y) > abs(x)) {
        r = 1.57079637 - r;
    }
    if (x < 0) r = 3.14159274 - r;
    if (y < 0) r = -r;
    if(r < 0) {
        r += 2* 3.14159274;
    }
    return r;
}

uint32_t colorcirc(int xp, int yp) {
    float x = xp, y = yp;
    float norm = 280 * 280;
    float dx = x - 400;
    float dy = y - 300;
    float r = dy * dy + dx * dx;
    r /= norm;

    rgb_t col;
    if(r >= 1) {
        col.r = 0;
        col.g = 0;
        col.b = 0;
    } else {
        r = sqrt2(r);
        hsv_t col_in;
        col_in.v = 1;
        col_in.s = r;

        if(dx == 0) {
            if(dy < 0)
                col_in.h = 180;
            else
                col_in.h = 0;
        } else if(dy == 0) {
            if(dx > 0)
                col_in.h = 270;
            else
                col_in.h = 90;
        } else {
            col_in.h = arctan2(dx, dy) * 360 / (3.14159274 * 2);
        }
        col = hsv2rgb(col_in);
    }
    col.r = max(0, min(255, col.r * 255));
    col.g = max(0, min(255, col.g * 255));
    col.b = max(0, min(255, col.b * 255));

    return (((uint8_t)col.r) << 16) | (((uint8_t)col.g) << 8) | (((uint8_t)col.b) << 0);
    
}

void foreverypixel(volatile uint8_t * target, uint32_t (*fun)(int, int)) {
    uint32_t rgb1;
    uint8_t * rgb = (uint8_t*) &rgb1;
    for(int y = 0; y < 600; y++)
        for(int x = 0; x < 800; x++) {
            rgb1 = fun(x, y);
            if(bytes_per_pixel == 3) {
                target[2] = rgb[2];
                target[1] = rgb[1];
                target[0] = rgb[0];
            } else if(bytes_per_pixel == 2) {
                rgb888_to_rgb565(rgb, target);
            } else {

                rgb888_to_rgb332(rgb, target);
            }
            target += bytes_per_pixel;
        }
    fence();
}

void show_colors(volatile uint8_t * target) {
    uint8_t rgb[] = {0,0,0};
    for(int y = 0; y < 600; y++)
        for(int x = 0; x < 800; x++) {
            rgb[2] = rgb[1] = rgb[0] = 0;
            if(y < 200) {
                rgb[2] = 255 * x / 800;
            } else if(y < 400) {
                rgb[1] = 255 * x / 800;
            } else {
                rgb[0] = 255 * x / 800;
            }
            if(bytes_per_pixel == 3) {
                target[2] = rgb[2];
                target[1] = rgb[1];
                target[0] = rgb[0];
            } else if(bytes_per_pixel == 2) {
                rgb888_to_rgb565(rgb, target);
            } else {

                rgb888_to_rgb332(rgb, target);
            }
            target += bytes_per_pixel;
    }
    fence();
}


void copy_pepe(volatile uint8_t * const target) {
    volatile uint8_t * dst = target;
    uint8_t * src = (uint8_t *)pepe;
    
    if(bytes_per_pixel == 1) {
        for(int i = 0; i < 800 * 600; i++) {
            rgb888_to_rgb332(src, dst);
            dst += 1;
            src += 3;
        }
    } else if(bytes_per_pixel == 2) {
        for(int i = 0; i < 800 * 600; i++) {
            rgb888_to_rgb565(src, dst);
            dst += 2;
            src += 3;
        }
    } else {
        for(int i = 0; i < 800 * 600 * 3; i++) {
            *dst = *src;
            dst += 1;
            src += 1;
        }
    }
}

void init_memory(volatile uint8_t * const target) {
    if (is_in_text_mode) {
        volatile uint16_t * ptr = (volatile uint16_t *) target;
        for(int i = 0; i < cols * rows; i++) {
            //Print every character I guess?
            //Bg and Fg use same color palette -> characters with same bg/fg color will not work
            *ptr = (((i / 16) % 0xff)<< 8) | (i % 0xff);
            ptr++;
        }
        wts(6, ((volatile uint32_t*)target)[1]);
    } else {
        volatile uint8_t * dst = target;
        uint32_t rgb;
        uint8_t * src = (uint8_t*)&rgb;
        if(bytes_per_pixel == 1) {
            for(int i = 0; i < 800 * 600; i++) {
                rgb = i;
                rgb888_to_rgb332(src, dst);
                dst += 1;
            }
        } else if(bytes_per_pixel == 2) {
            for(int i = 0; i < 800 * 600; i++) {
                rgb = i;
                rgb888_to_rgb565(src, dst);
                dst += 2;
            }
        } else {
            for(int i = 0; i < 800 * 600; i++) {
                dst[2] = i % 0x100;
                dst[1] = i % 0x10000;
                dst[0] = i % 0x1000000;
                dst += 3;
            }
        }

        wts(6, ((volatile uint32_t*)target)[1]);
    }

    //Make sure that RAM is updated
    fence();
}

void set_text(volatile uint16_t * target, char * text, int row) {
    target += cols * row;
    while(*text != 0) {
        *target = ((0x0f) << 8) | *text; 
        text += 1;
        target += 1;
    }

    fence();
}

char* into_str(char* d, uint32_t s) {
    int i = 0;
    d[i++] = (s / 1000000000) % 10 + '0';
    d[i++] = (s / 100000000) % 10 + '0';
    d[i++] = (s / 10000000) % 10 + '0';
    d[i++] = (s / 1000000) % 10 + '0';
    d[i++] = (s / 100000) % 10 + '0';
    d[i++] = (s / 10000) % 10 + '0';
    d[i++] = (s / 1000) % 10 + '0';
    d[i++] = (s / 100) % 10 + '0';
    d[i++] = (s / 10) % 10 + '0';
    d[i++] = (s / 1) % 10 + '0';
    return d;
}

char* into_str16(char* d, uint32_t s) {
    int i = 0;
    uint8_t ch[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    d[i++] = ch[(s / 0x10000000) % 0x10];
    d[i++] = ch[(s / 0x1000000) % 0x10];
    d[i++] = ch[(s / 0x100000) % 0x10];
    d[i++] = ch[(s / 0x10000) % 0x10];
    d[i++] = ch[(s / 0x1000) % 0x10];
    d[i++] = ch[(s / 0x100) % 0x10];
    d[i++] = ch[(s / 0x10) % 0x10];
    d[i++] = ch[(s / 0x1) % 0x10];

    return d;
}

void write_params_to_screen(volatile uint16_t* const dest) {
    char buff[]   = "0000000000";
    char buff16[] = "00112233";
    int i = 1;
    set_text(dest, "ptrq: ", i); set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, POINTERQ)), i++);
    set_text(dest, "ptot: ", i); set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, H_VTOT)), i++);
    set_text(dest, "pact: ", i); set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, H_VACTIVE)), i++);
    set_text(dest, "fron: ", i); set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, H_VFRONT)), i++);
    set_text(dest, "sync: ", i); set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, H_VSYNC)), i++);

    set_text(dest, "pwrr: ", i);set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, POWERREG)), i++);
    set_text(dest, "cptr: ", i);set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, CURRENT_PTR)), i++);
    set_text(dest, "txtb: ", i);set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, TEXT_BUFF_PARA)), i++);
    set_text(dest, "curs: ", i);set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, CURSOR_FONT_PARA)), i++);
    set_text(dest, "thrs: ", i);set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD)), i++);

    set_text(dest, "mlen: ", i);set_text(dest + 10, into_str(buff, *reg32(AXI2HDMI_BASE, FIFO_MAX_REFILL_AMOUNT)), i++);
    set_text(dest, "pfor: ", i);set_text(dest + 10, into_str16(buff16, *reg32(AXI2HDMI_BASE, PIXEL_FORMAT)), i++);

    dest[(selected_offset / 8 + 1) * cols] = 0x5f00 | '>';
}

void sleep_intr(uint64_t ticks) {
    //clint_sleep_ticks(0, ticks);
    
    clint_set_mtimecmpx(0, clint_get_mtime() + ticks);
    fence();
    set_mtie(1);
    set_mie(1);
    
    wfi();
    
}

void trap_vector(void) {
    //Disaple interrupts in the IRQ; or else this will be re-enabled immediatly, causing a loop
    set_mtie(0);
    set_mie(0);
}

void automatic_tester(uint32_t rtc_freq) {
    char num [] = "0000000000\r\n";

    for(int thrs = 1; thrs < 32; thrs++) {
        *reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD) = thrs;
        char s [] = "Threshold = ";
        char s2[] = "; Time (ms) = ";
        uart_write_str(&__base_uart, s, sizeof(s));
        //skip newline
        uart_write_str(&__base_uart, into_str(num, thrs), sizeof(num) - 3);

        uint64_t time = clint_get_mtime();
        stress_ram(0x80);
        time = clint_get_mtime() - time;
        uart_write_str(&__base_uart, s2, sizeof(s2));
        uart_write_str(&__base_uart, into_str(num , time * 1000 / rtc_freq), sizeof(num));
    }
    
    uart_write_flush(&__base_uart);
}