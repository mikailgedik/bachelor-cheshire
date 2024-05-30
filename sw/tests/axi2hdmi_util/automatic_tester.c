#define MIN_QUEUE_LEN 1
#define MAX_QUEUE_LEN 32

void automatic_tester(uint32_t rtc_freq) {
    char num [] = "0000000000\r\n";
    
    uint32_t data[MAX_QUEUE_LEN][MAX_QUEUE_LEN];
    for(int i = 0; i < MAX_QUEUE_LEN; i++) {
        for(int ii = 0; ii < MAX_QUEUE_LEN; ii++) {
            data[i][ii] = 0xffffffff;
        }
    }

    for(int queue_len = 1; queue_len < MAX_QUEUE_LEN; queue_len++) {
        for(int threshold = 1; threshold < queue_len; threshold++) {
            //Write this first, so that the threshold will never
            //accidentally be higher than the queue size
            *reg32(AXI2HDMI_BASE, FIFO_MAX_REFILL_AMOUNT) = 64;
            
            *reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD) = threshold;
            *reg32(AXI2HDMI_BASE, FIFO_MAX_REFILL_AMOUNT) = queue_len - threshold;
        }
    }

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