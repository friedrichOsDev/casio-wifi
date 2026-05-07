#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * !!! Note that the RTC has to be stopped, whenever you want to overwrite a counter !!!
 */

/**
 * Addresses
 */
#define R64CNT_ADDR 0xA413FEC0
#define RSECCNT_ADDR 0xA413FEC2
#define RMINCNT_ADDR 0xA413FEC4
#define RHRCNT_ADDR  0xA413FEC6
#define RWKCNT_ADDR  0xA413FEC8
#define RDAYCNT_ADDR 0xA413FECA
#define RMONCNT_ADDR 0xA413FECC
#define RYRCNT_ADDR  0xA413FECE
#define RCR2_ADDR    0xA413FEDE

/**
 * Days
 */
#define DAY_SUN 0
#define DAY_MON 1
#define DAY_TUE 2
#define DAY_WED 3
#define DAY_THU 4
#define DAY_FRI 5
#define DAY_SAT 6

/** 
 * Usage:
 * 
 * uint8_t val = *(volatile uint8_t*)R64CNT_ADDR;
 * if (val & R64CNT_HZ1) {
 *     // The 1Hz bit is set (toggles every 0.5s)
 * }
 */
#define R64CNT_HZ1 (1 << 6) // Bit 6: 1 Hz counter
#define R64CNT_HZ2 (1 << 5) // Bit 5: 2 Hz counter
#define R64CNT_HZ4 (1 << 4) // Bit 4: 4 Hz counter
#define R64CNT_HZ8 (1 << 3) // Bit 3: 8 Hz counter
#define R64CNT_HZ16 (1 << 2) // Bit 2: 16 Hz counter
#define R64CNT_HZ32 (1 << 1) // Bit 1: 32 Hz counter
#define R64CNT_HZ64 (1 << 0) // Bit 0: 64 Hz counter

/**
 * Usage:
 * 
 * rtc_sec_reg* sec_reg = (rtc_sec_reg*)RSECCNT_ADDR;
 * uint8_t seconds = sec_reg->tens * 10 + sec_reg->ones
 */
struct rtc_sec_reg {
    uint8_t resv : 1; // Bit 7: Reserved (0)
    uint8_t tens : 3; // Bits 4-6: Ten's place (0-5)
    uint8_t ones : 4; // Bits 0-3: One's place (0-9)
};

/**
 * Usage:
 * 
 * rtc_min_reg* min_reg = (rtc_min_reg*)RMINCNT_ADDR;
 * uint8_t minutes = min_reg->tens * 10 + min_reg->ones
 */
struct rtc_min_reg {
    uint8_t resv : 1; // Bit 7: Reserved (0)
    uint8_t tens : 3; // Bits 4-6: Ten's place (0-5)
    uint8_t ones : 4; // Bits 0-3: One's place (0-9)
};

/**
 * Usage:
 * 
 * rtc_hour_reg* hour_reg = (rtc_hour_reg*)RHRCNT_ADDR;
 * uint8_t hours = hour_reg->tens * 10 + hour_reg->ones
 * 
 */
struct rtc_hour_reg {
    uint8_t resv : 2; // Bits 6-7: Reserved (0)
    uint8_t tens : 2; // Bits 4-5: Ten's place (0-2)
    uint8_t ones : 4; // Bits 0-3: One's place (0-9)
};

/**
 * Usage:
 * 
 * rtc_week_reg* week_reg = (rtc_week_reg*)RWKCNT_ADDR;
 * uint8_t day = week_reg->day;
 */
struct rtc_week_reg {
    uint8_t resv : 5; // Bits 3-7: Reserved (0)
    uint8_t day : 3; // Bits 0-2: Day of week (0-6)
};

/**
 * Usage:
 * 
 * rtc_day_reg* day_reg = (rtc_day_reg*)RDAYCNT_ADDR;
 * uint8_t day = day_reg->tens * 10 + day_reg->ones;
 */
struct rtc_day_reg {
    uint8_t resv : 2; // Bits 6-7: Reserved (0)
    uint8_t tens : 2; // Bits 4-5: Ten's place (0-3)
    uint8_t ones : 4; // Bits 0-3: One's place (0-9)
};

/**
 * Usage:
 * 
 * rtc_mon_reg* mon_reg = (rtc_mon_reg*)RMONCNT_ADDR;
 * uint8_t month = mon_reg->tens * 10 + mon_reg->ones;
 */
struct rtc_mon_reg {
    uint8_t resv : 3; // Bits 5-7: Reserved (0)
    uint8_t tens : 1; // Bit 4: Ten's place (0-1)
    uint8_t ones : 4; // Bits 0-3: One's place (0-9)
};

// This is not working on the CP400 and I don't know why.
// /**
//  * Usage:
//  * 
//  * rtc_year_reg* year_reg = (rtc_year_reg*)RYRCNT_ADDR;
//  * uint16_t year = year_reg->thousands * 1000 + year_reg->hundreds * 100 + year_reg->tens * 10 + year_reg->ones;
//  */
// struct rtc_year_reg {
//     uint8_t thousands : 4; // Bits 4-7: Thousand's place (0-9, but realistically only 0-2)
//     uint8_t hundreds : 4; // Bits 0-3: Hundred's
//     uint8_t tens : 4; // Bits 4-7: Ten's place (0-9)
//     uint8_t ones : 4; // Bits 0-3: One's place (0-9)
// };

/**
 * Usage:
 * 
 * rtc_ctrl_reg* ctrl_reg = (rtc_ctrl_reg*)RCR2_ADDR;
 * ctrl_reg->start = 1;
 */
struct rtc_ctrl_reg {
    uint8_t interrupt_flag : 1; // Bit 7: Interrupt trigger/acknowledge
    uint8_t interrupt_period : 3; // Bits 4-6: Period of interrupts
    uint8_t halt_oscillator : 1; // Bit 3: Set to halt crystal oscillator
    uint8_t adjust_30sec : 1; // Bit 2: 30 second adjust (Write 1 to round)
    uint8_t reset_divider : 1; // Bit 1: Reset divider circuit (Write 1 to reset)
    uint8_t start : 1; // Bit 0: Start/Stop counters
};

/**
 * Functions
 */
#ifdef __cplusplus
extern "C" {
#endif

bool rtc_check_hz(uint8_t hz_mask);
void rtc_sleep_ms(uint32_t ms);
void rtc_start();
void rtc_stop();
bool rtc_running();
int rtc_get_set_sec(int sec);
int rtc_get_set_min(int min);
int rtc_get_set_hour(int hour);
int rtc_get_set_day(int day);
int rtc_get_set_mon(int mon);
// This is not working on the CP400 and I don't know why.
// int rtc_get_set_year(int year);
int rtc_get_set_week(int day_of_week);

#ifdef __cplusplus
}
#endif
