#define MIN_QUEUE_LEN 1
#define MAX_QUEUE_LEN 4

void automatic_tester(uint32_t rtc_freq) {
    uint32_t millis_per_frame = 17;

    uint32_t data[MAX_QUEUE_LEN + 1][MAX_QUEUE_LEN + 1];
    for(int i = 0; i < MAX_QUEUE_LEN + 1; i++) {
        for(int ii = 0; ii < MAX_QUEUE_LEN + 1; ii++) {
            data[i][ii] = 0;
        }
    }

    for(int queue_len = 1; queue_len < MAX_QUEUE_LEN + 1; queue_len++) {
        for(int threshold = 1; threshold < queue_len; threshold++) {
            *reg32(AXI2HDMI_BASE, FIFO_REFILL_THRESHOLD) = threshold;
            *reg32(AXI2HDMI_BASE, FIFO_MAX_REFILL_AMOUNT) = queue_len - threshold;
            //Wait 10 frames before clearing the error status
            sleep_intr(rtc_freq * 10 * millis_per_frame / 1000);
            *reg32(AXI2HDMI_BASE, SYNC_FAIL_HAPPENED) = 0;

            fence();
            printstr(into_str(*reg32(AXI2HDMI_BASE, SYNC_FAIL_HAPPENED)));

            //Wait for one second to check wether this works
            sleep_intr(rtc_freq * 120 * millis_per_frame / 1000);
            if(*reg32(AXI2HDMI_BASE, SYNC_FAIL_HAPPENED) != 0) {
                data[queue_len][threshold] = 1;
                printstr("Settings t=");
                printstr(into_str(threshold));
                printstr("; q=");
                printstr(into_str(queue_len));
                printstr(" failed immediately\r\n");
                continue;
            } else {
                data[queue_len][threshold] = 2;
            }

            /*
            uint64_t time = clint_get_mtime();
            stress_ram(0x10);
            time = clint_get_mtime() - time;

            if(*reg32(AXI2HDMI_BASE, SYNC_FAIL_HAPPENED) != 0) {
                data[queue_len][threshold] = 2;
                printstr("Settings t=");
                printstr(into_str(threshold));
                printstr("; q=");
                printstr(into_str(queue_len));
                printstr(" failed under stress\r\n");
            } else {
                data[queue_len][threshold] = time / 1000;
                printstr("Settings t=");
                printstr(into_str(threshold));
                printstr("; q=");
                printstr(into_str(queue_len));
                printstr(" OK\r\n");
            }
            */
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