#include "rtc.h"

volatile struct rtc_ctrl_reg* ctrl_reg = (volatile struct rtc_ctrl_reg*)RCR2_ADDR;
volatile struct rtc_sec_reg* sec_reg = (volatile struct rtc_sec_reg*)RSECCNT_ADDR;
volatile struct rtc_min_reg* min_reg = (volatile struct rtc_min_reg*)RMINCNT_ADDR;
volatile struct rtc_hour_reg* hour_reg = (volatile struct rtc_hour_reg*)RHRCNT_ADDR;
volatile struct rtc_day_reg* day_reg = (volatile struct rtc_day_reg*)RDAYCNT_ADDR;
volatile struct rtc_mon_reg* mon_reg = (volatile struct rtc_mon_reg*)RMONCNT_ADDR;
volatile struct rtc_year_reg* year_reg = (volatile struct rtc_year_reg*)RYRCNT_ADDR;
volatile struct rtc_week_reg* week_reg = (volatile struct rtc_week_reg*)RWKCNT_ADDR;

/**
 * Checks if a specific frequency bit is set in the 64Hz counter register.
 *
 * @param hz_mask The bitmask to check (e.g., R64CNT_HZ1).
 * @return true if the bit is set, false otherwise.
 */
bool rtc_check_hz(uint8_t hz_mask) {
    return (*(volatile uint8_t*)R64CNT_ADDR & hz_mask);
}

/**
 * Sleeps for a specified number of milliseconds using the RTC 64Hz counter.
 *
 * @param ms The number of milliseconds to sleep.
 */
void rtc_sleep_ms(uint32_t ms) {
    if (ms == 0) return;
    uint32_t ticks_needed = (ms * 128) / 1000;
    if (ticks_needed == 0) ticks_needed = 1;

    uint8_t last_state = *(volatile uint8_t*)R64CNT_ADDR & R64CNT_HZ64;
    uint32_t elapsed_ticks = 0;

    while (elapsed_ticks < ticks_needed) {
        uint8_t current_state = *(volatile uint8_t*)R64CNT_ADDR & R64CNT_HZ64;
        if (current_state != last_state) {
            elapsed_ticks++;
            last_state = current_state;
        }
    }
}

/**
 * Starts the Real-Time Clock counters.
 */
void rtc_start() {
	ctrl_reg->start = 1;
}

/**
 * Stops the Real-Time Clock counters.
 * This must be called before overwriting any counter registers.
 */
void rtc_stop() {
    ctrl_reg->start = 0;
}

/**
 * Checks if the RTC is currently running.
 *
 * @return true if running, false if stopped.
 */
bool rtc_running() {
    return ctrl_reg->start != 0;
}

/**
 * Gets or sets the seconds counter.
 * 
 * @param sec The seconds to set (0-59), or a negative value to get the current value.
 * @return The current seconds if getting, or -1 if setting.
 */
int rtc_get_set_sec(int sec) {
    rtc_stop();
    if (sec < 0) {
        int val = sec_reg->tens * 10 + sec_reg->ones;
        rtc_start();
        return val;
    } else if (sec >= 0 && sec <= 59) {
        sec_reg->tens = sec / 10;
        sec_reg->ones = sec % 10;
    }
    rtc_start();
    return -1;
}

/**
 * Gets or sets the minutes counter.
 * 
 * @param min The minutes to set (0-59), or a negative value to get the current value.
 * @return The current minutes if getting, or -1 if setting.
 */
int rtc_get_set_min(int min) {
    rtc_stop();
    if (min < 0) {
        int val = min_reg->tens * 10 + min_reg->ones;
        rtc_start();
        return val;
    } else if (min >= 0 && min <= 59) {
        min_reg->tens = min / 10;
        min_reg->ones = min % 10;
    }
    rtc_start();
    return -1;
}

/**
 * Gets or sets the hours counter.
 * 
 * @param hour The hour to set (0-23), or a negative value to get the current value.
 * @return The current hour if getting, or -1 if setting.
 */
int rtc_get_set_hour(int hour) {
    rtc_stop();
    if (hour < 0) {
        int val = hour_reg->tens * 10 + hour_reg->ones;
        rtc_start();
        return val;
    } else if (hour >= 0 && hour <= 23) {
        hour_reg->tens = hour / 10;
        hour_reg->ones = hour % 10;
    }
    rtc_start();
    return -1;
}

/**
 * Gets or sets the day of the month counter.
 * 
 * @param day The day to set (1-31), or a negative value to get the current value.
 * @return The current day if getting, or -1 if setting.
 */
int rtc_get_set_day(int day) {
    rtc_stop();
    if (day < 0) {
        int val = day_reg->tens * 10 + day_reg->ones;
        rtc_start();
        return val;
    } else if (day >= 1 && day <= 31) {
        day_reg->tens = day / 10;
        day_reg->ones = day % 10;
    }
    rtc_start();
    return -1;
}

/**
 * Gets or sets the month counter.
 * 
 * @param mon The month to set (1-12), or a negative value to get the current value.
 * @return The current month if getting, or -1 if setting.
 */
int rtc_get_set_mon(int mon) {
    rtc_stop();
    if (mon < 0) {
        int val = mon_reg->tens * 10 + mon_reg->ones;
        rtc_start();
        return val;
    } else if (mon >= 1 && mon <= 12) {
        mon_reg->tens = mon / 10;
        mon_reg->ones = mon % 10;
    }
    rtc_start();
    return -1;
}

// This is not working on the CP400 and I don't know why.
// /**
//  * Gets or sets the year counter.
//  * 
//  * @param year The year to set (0-9999), or a negative value to get the current value.
//  * @return The current year if getting, or -1 if setting.
//  */
// int rtc_get_set_year(int year) {
//     rtc_stop();
//     if (year < 0) {
//         int val = year_reg->thousands * 1000 + year_reg->hundreds * 100 + year_reg->tens * 10 + year_reg->ones;
//         rtc_start();
//         return val;
//     } else if (year >= 0 && year <= 9999) {
//         year_reg->thousands = (year / 1000) % 10;
//         year_reg->hundreds = (year / 100) % 10;
//         year_reg->tens = (year / 10) % 10;
//         year_reg->ones = year % 10;
//     }
//     rtc_start();
//     return -1;
// }

/**
 * Gets or sets the day of the week counter.
 * 
 * @param day_of_week The day to set (0-6, see DAY_* defines), or a negative value to get the current value.
 * @return The current day of the week if getting, or -1 if setting.
 */
int rtc_get_set_week(int day_of_week) {
    rtc_stop();
    if (day_of_week < 0) {
        int val = week_reg->day;
        rtc_start();
        return val;
    } else if (day_of_week >= DAY_SUN && day_of_week <= DAY_SAT) {
        week_reg->day = day_of_week;
    }
    rtc_start();
    return -1;
}
