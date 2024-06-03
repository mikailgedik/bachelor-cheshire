#define MIN_THRESHOLD 2
#define MIN_QUEUE_LEN 2
#define MAX_QUEUE_LEN 24

//Returns 0 if the frame is not displayable at all
//1 if displayable without load
//Any other number represents the time it takes to finish the memory testing
uint32_t check_res(uint32_t rtc_freq) {
    uint32_t millis_per_frame = 17;
    volatile uint32_t * const fail = reg32(AXI2HDMI_BASE, SYNC_FAIL_HAPPENED);
    //Wait for new settings to take effect on a few frames -> old frames might re-trigger and takes a while to adjust
    sleep_intr(rtc_freq * 30 * millis_per_frame / 1000);
    //Reset fail state
    fence();
    *fail = 0;
    //Fences just to be sure?
    fence();
    sleep_intr(rtc_freq * 30 * millis_per_frame / 1000);
    fence();
    if (*fail != 0) {
        return 1;
    }

    uint64_t time = clint_get_mtime();
    stress_ram(0x10);
    time = clint_get_mtime() - time;
    
    if (*fail != 0) {
        return 2;
    } else {
        return time;
    }
}

void automatic_tester(uint32_t rtc_freq) {
    uint32_t millis_per_frame = 17;

    uint32_t data[MAX_QUEUE_LEN + 1][MAX_QUEUE_LEN + 1];
    for(int i = 0; i < MAX_QUEUE_LEN + 1; i++) {
        for(int ii = 0; ii < MAX_QUEUE_LEN + 1; ii++) {
            data[i][ii] = 0;
        }
    }

    for(int queue_len = MIN_QUEUE_LEN; queue_len < MAX_QUEUE_LEN + 1; queue_len++) {
        for(int threshold = MIN_THRESHOLD; threshold < queue_len + 1; threshold++) {
            uint32_t refill_amount = queue_len - (threshold - 1);
            *reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD) = threshold;
            *reg32(AXI2HDMI_BASE, FIFO_MAX_REFILL_AMOUNT) = refill_amount;
            
            uint32_t result = check_res(rtc_freq);
            
            printstr("Settings t=");
            printstr(into_str(threshold));
            printstr("; q=");
            printstr(into_str(queue_len));
            printstr("; r=");
            printstr(into_str(refill_amount));


            data[queue_len][threshold] = result;
            switch(result) {
                case 1:
                    printstr(" failed immediately\r\n");
                    break;
                case 2:
                    printstr(" failed under stress\r\n");
                    break;
                default:
                    printstr(" OK: ");
                    printstr(into_str(result));
                    printstr("\r\n");
                    break;
            }
        }
    }

    printstr("Results (y-Axis: queue length, x-Axis: threshold):\r\n");
    printstr("----------");
    for(int threshold = 1; threshold < MAX_QUEUE_LEN + 1; threshold++) {
        printstr(";");
        printstr(into_str(threshold));
    }
    printstr("\r\n");

    for(int queue_len = 1; queue_len < MAX_QUEUE_LEN + 1; queue_len++) {
        printstr(into_str(queue_len));
        for(int threshold = 1; threshold < MAX_QUEUE_LEN + 1; threshold++) {
            printstr(";");
            printstr(into_str(data[queue_len][threshold]));
        }
        printstr("\r\n");
    }
}