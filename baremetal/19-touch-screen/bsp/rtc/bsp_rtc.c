#include "bsp_rtc.h"

/*
 * 注意：不要使用 MCIMX6Y2.h 中已经定义过的 SNVS_xxx 宏名，避免宏重定义。
 */
#define RTC_SNVS_HPCOMR_NPSWA_EN        (1U << 31)
#define RTC_SNVS_HPCOMR_BIT8            (1U << 8)
#define RTC_SNVS_LPCR_SRTC_ENV          (1U << 0)
#define RTC_SNVS_SRTC_HIGH_SHIFT        (17U)
#define RTC_SNVS_SRTC_LOW_SHIFT         (15U)

/*
 * 有些 MCIMX6Y2.h 里的 SNVS_Type 没有暴露 LPSRTCMR/LPSRTCLR 成员，
 * 这里用寄存器偏移访问，避免结构体成员名不一致导致编译失败。
 * SNVS LP SRTC Counter MSB Register offset: 0x50
 * SNVS LP SRTC Counter LSB Register offset: 0x54
 */
#define RTC_SNVS_LPSRTCMR_OFFSET        (0x50U)
#define RTC_SNVS_LPSRTCLR_OFFSET        (0x54U)
#define RTC_REG32(addr)                 (*(volatile unsigned int *)(addr))
#define RTC_SNVS_BASE_ADDR              ((unsigned int)SNVS)
#define RTC_SNVS_LPSRTCMR               RTC_REG32(RTC_SNVS_BASE_ADDR + RTC_SNVS_LPSRTCMR_OFFSET)
#define RTC_SNVS_LPSRTCLR               RTC_REG32(RTC_SNVS_BASE_ADDR + RTC_SNVS_LPSRTCLR_OFFSET)

static const unsigned short g_month_days_before[13] = {
    0U, 0U, 31U, 59U, 90U, 120U, 151U, 181U, 212U, 243U, 273U, 304U, 334U
};

static const unsigned char g_days_per_month[13] = {
    0U, 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U
};

/* 
 * 描述:初始化RTC
 */
void rtc_init(void)
{
    /*
     * 设置HPCOMR寄存器
     * bit[31] 1 : 允许访问SNVS寄存器，一定要置1
     * bit[8]  1 : 此位置1，需要签署NDA协议才能看到此位的详细说明，
     *             这里不置1也没问题
     */
    SNVS->HPCOMR |= RTC_SNVS_HPCOMR_NPSWA_EN | RTC_SNVS_HPCOMR_BIT8;

#if 0
    struct rtc_datetime rtcdate;

    rtcdate.year = 2018U;
    rtcdate.month = 12U;
    rtcdate.day = 13U;
    rtcdate.hour = 14U;
    rtcdate.minute = 52U;
    rtcdate.second = 0U;
    rtc_setdatetime(&rtcdate); /* 初始化时间和日期 */
#endif

    rtc_enable(); /* 使能RTC */
}

/*
 * 描述: 开启RTC
 */
void rtc_enable(void)
{
    /* LPCR寄存器bit0置1，使能RTC */
    SNVS->LPCR |= RTC_SNVS_LPCR_SRTC_ENV;
    while ((SNVS->LPCR & RTC_SNVS_LPCR_SRTC_ENV) == 0U) {
        ; /* 等待使能完成 */
    }
}

/*
 * 描述: 关闭RTC
 */
void rtc_disable(void)
{
    /* LPCR寄存器bit0置0，关闭RTC */
    SNVS->LPCR &= ~RTC_SNVS_LPCR_SRTC_ENV;
    while ((SNVS->LPCR & RTC_SNVS_LPCR_SRTC_ENV) != 0U) {
        ; /* 等待关闭完成 */
    }
}

/*
 * @description : 判断指定年份是否为闰年，闰年条件如下:
 * @param - year: 要判断的年份
 * @return      : 1 是闰年，0 不是闰年
 */
unsigned char rtc_isleapyear(unsigned short year)
{
    return (unsigned char)(((year % 400U) == 0U) ||
                           (((year % 4U) == 0U) && ((year % 100U) != 0U)));
}

/*
 * @description     : 将时间转换为秒数
 * @param - datetime: 要转换日期和时间。
 * @return          : 转换后的秒数
 */
unsigned int rtc_coverdate_to_seconds(struct rtc_datetime *datetime)
{
    unsigned short year;
    unsigned int days = 0U;

    for (year = YEAR_RANGE_START; year < datetime->year; year++) {
        days += DAYS_IN_A_YEAR;
        if (rtc_isleapyear(year) != 0U) {
            days += 1U;
        }
    }

    days += g_month_days_before[datetime->month];
    if ((rtc_isleapyear(datetime->year) != 0U) && (datetime->month >= 3U)) {
        days += 1U;
    }

    days += (unsigned int)datetime->day - 1U;

    return (days * SECONDS_IN_A_DAY) +
           ((unsigned int)datetime->hour * SECONDS_IN_A_HOUR) +
           ((unsigned int)datetime->minute * SECONDS_IN_A_MINUTE) +
           (unsigned int)datetime->second;
}

/*
 * @description     : 设置时间和日期
 * @param - datetime: 要设置的日期和时间
 * @return          : 无
 */
void rtc_setdatetime(struct rtc_datetime *datetime)
{
    unsigned int seconds;
    unsigned int lpcr = SNVS->LPCR;

    /* 设置寄存器LPSRTCMR和LPSRTCLR的时候一定要先关闭RTC */
    rtc_disable();

    seconds = rtc_coverdate_to_seconds(datetime);

    RTC_SNVS_LPSRTCMR = (unsigned int)(seconds >> RTC_SNVS_SRTC_HIGH_SHIFT);
    RTC_SNVS_LPSRTCLR = (unsigned int)(seconds << RTC_SNVS_SRTC_LOW_SHIFT);

    /* 如果此前RTC是打开的，在设置完RTC时间以后需要重新打开RTC */
    if ((lpcr & RTC_SNVS_LPCR_SRTC_ENV) != 0U) {
        rtc_enable();
    }
}

/*
 * @description     : 将秒数转换为时间
 * @param - seconds : 要转换的秒数
 * @param - datetime: 转换后的日期和时间
 * @return          : 无
 */
void rtc_convertseconds_to_datetime(u64 seconds, struct rtc_datetime *datetime)
{
    u64 days;
    u64 seconds_remaining;
    unsigned short days_in_year;
    unsigned char month;

    seconds_remaining = seconds;
    days = (seconds_remaining / SECONDS_IN_A_DAY) + 1U;
    seconds_remaining %= SECONDS_IN_A_DAY;

    /* 计算时、分、秒 */
    datetime->hour = (unsigned char)(seconds_remaining / SECONDS_IN_A_HOUR);
    seconds_remaining %= SECONDS_IN_A_HOUR;
    datetime->minute = (unsigned char)(seconds_remaining / SECONDS_IN_A_MINUTE);
    datetime->second = (unsigned char)(seconds_remaining % SECONDS_IN_A_MINUTE);

    /* 计算年 */
    datetime->year = YEAR_RANGE_START;
    days_in_year = (rtc_isleapyear(datetime->year) != 0U) ?
                   (DAYS_IN_A_YEAR + 1U) : DAYS_IN_A_YEAR;

    while (days > days_in_year) {
        days -= days_in_year;
        datetime->year++;
        days_in_year = (rtc_isleapyear(datetime->year) != 0U) ?
                       (DAYS_IN_A_YEAR + 1U) : DAYS_IN_A_YEAR;
    }

    /* 根据剩余的天数计算月份 */
    for (month = 1U; month <= 12U; month++) {
        unsigned char days_this_month = g_days_per_month[month];

        if ((month == 2U) && (rtc_isleapyear(datetime->year) != 0U)) {
            days_this_month = 29U;
        }

        if (days <= days_this_month) {
            datetime->month = month;
            break;
        }

        days -= days_this_month;
    }

    datetime->day = (unsigned char)days;
}

/*
 * @description : 获取RTC当前秒数
 * @param       : 无
 * @return      : 当前秒数 
 */
unsigned int rtc_getseconds(void)
{
    return (unsigned int)((RTC_SNVS_LPSRTCMR << RTC_SNVS_SRTC_HIGH_SHIFT) |
                          (RTC_SNVS_LPSRTCLR >> RTC_SNVS_SRTC_LOW_SHIFT));
}

/*
 * @description     : 获取当前时间
 * @param - datetime: 获取到的时间，日期等参数
 * @return          : 无 
 */
void rtc_getdatetime(struct rtc_datetime *datetime)
{
    rtc_convertseconds_to_datetime((u64)rtc_getseconds(), datetime);
}
