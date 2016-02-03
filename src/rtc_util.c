#include "global.h"
#include "rtc.h"
#include "rtc_util.h"
#include "string_util.h"

extern const struct RtcInfo gDefaultRtcInfo;
extern const s32 gNumDaysInMonths[];

extern u16 gRtcStatus;
extern struct RtcInfo gRtcInfo;
extern u8 gRtcProbeResult;
extern u16 gRtcSavedIme;

extern struct Time gLocalTime;

void RtcDisableInterrupts();
void RtcRestoreInterrupts();
u32 ConvertBcdToBinary(u8 bcd);
bool8 IsLeapYear(u8 year);
u16 ConvertDateToDayCount(u8 year, u8 month, u8 day);
u16 RtcGetDayCount(struct RtcInfo *rtc);
void RtcGetInfo(struct RtcInfo *rtc);
void RtcGetDateTime(struct RtcInfo *rtc);
void RtcGetControl(struct RtcInfo *rtc);
void RtcGetRawInfo(struct RtcInfo *rtc);
u16 RtcCheckInfo(struct RtcInfo *rtc);
void RtcCalcTimeDifference(struct RtcInfo *rtc, struct Time *result, struct Time *t);

void RtcDisableInterrupts()
{
    gRtcSavedIme = REG_IME;
    REG_IME = 0;
}

void RtcRestoreInterrupts()
{
    REG_IME = gRtcSavedIme;
}

u32 ConvertBcdToBinary(u8 bcd)
{
    if (bcd > 0x9F)
        return 0xFF;

    if ((bcd & 0xF) <= 9)
        return (10 * ((bcd >> 4) & 0xF)) + (bcd & 0xF);
    else
        return 0xFF;
}

bool8 IsLeapYear(u8 year)
{
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        return TRUE;

    return FALSE;
}

u16 ConvertDateToDayCount(u8 year, u8 month, u8 day)
{
    s32 i;
    u16 dayCount = 0;

#if (REVISION < 2)
    // Revisions 0 and 1 don't add days for the year 2000,
    // causing the berry glitch.
    for (i = year - 1; i > 0; i--)
    {
        dayCount += 365;

        if (IsLeapYear(i) == TRUE)
            dayCount++;
    }
#else
    // Revision 2 has "i >= 0" as the condition instead of "i > 0",
    // which fixes the issue.
    for (i = year - 1; i >= 0; i--)
    {
        dayCount += 365;

        if (IsLeapYear(i) == TRUE)
            dayCount++;
    }
#endif

    for (i = 0; i < month - 1; i++)
        dayCount += gNumDaysInMonths[i];

    if (month > MONTH_FEB && IsLeapYear(year) == TRUE)
        dayCount++;

    dayCount += day;

    return dayCount;
}

u16 RtcGetDayCount(struct RtcInfo *rtc)
{
    u8 year = ConvertBcdToBinary(rtc->year);
    u8 month = ConvertBcdToBinary(rtc->month);
    u8 day = ConvertBcdToBinary(rtc->day);
    return ConvertDateToDayCount(year, month, day);
}

void RtcInit()
{
    gRtcStatus = 0;

    RtcDisableInterrupts();
    RTC_Unprotect();
    gRtcProbeResult = RTC_Probe();
    RtcRestoreInterrupts();

    if (!(gRtcProbeResult & 0xF))
    {
        gRtcStatus = RTC_STAT_INIT_ERROR;
        return;
    }

    if (gRtcProbeResult & 0xF0)
        gRtcStatus = RTC_STAT_INIT_WARNING;
    else
        gRtcStatus = 0;

    RtcGetRawInfo(&gRtcInfo);
    gRtcStatus = RtcCheckInfo(&gRtcInfo);
}

u16 RtcGetStatus()
{
    return gRtcStatus;
}

void RtcGetInfo(struct RtcInfo *rtc)
{
    if (gRtcStatus & RTC_STAT_ERROR_FLAGS)
        *rtc = gDefaultRtcInfo;
    else
        RtcGetRawInfo(rtc);
}

void RtcGetDateTime(struct RtcInfo *rtc)
{
    RtcDisableInterrupts();
    RTC_GetDateTime(rtc);
    RtcRestoreInterrupts();
}

void RtcGetControl(struct RtcInfo *rtc)
{
    RtcDisableInterrupts();
    RTC_GetControl(rtc);
    RtcRestoreInterrupts();
}

void RtcGetRawInfo(struct RtcInfo *rtc)
{
    RtcGetControl(rtc);
    RtcGetDateTime(rtc);
}

u16 RtcCheckInfo(struct RtcInfo *rtc)
{
    u16 errorFlags = 0;
    s32 year;
    s32 month;
    s32 value;

    if (rtc->control & RTC_INFO_CTRL_POWER_FAILURE)
        errorFlags |= RTC_STAT_ERR_POWER_FAILURE;

    if (!(rtc->control & RTC_INFO_CTRL_24HOUR))
        errorFlags |= RTC_STAT_ERR_12HOUR_CLOCK;

    year = ConvertBcdToBinary(rtc->year);

    if (year == 0xFF)
        errorFlags |= RTC_STAT_ERR_INVALID_YEAR;

    month = ConvertBcdToBinary(rtc->month);

    if (month == 0xFF || month == 0 || month > 12)
        errorFlags |= RTC_STAT_ERR_INVALID_MONTH;

    value = ConvertBcdToBinary(rtc->day);

    if (value == 0xFF)
        errorFlags |= RTC_STAT_ERR_INVALID_DAY;

    if (month == MONTH_FEB)
    {
        if (value > IsLeapYear(year) + gNumDaysInMonths[month - 1])
            errorFlags |= RTC_STAT_ERR_INVALID_DAY;
    }
    else
    {
        if (value > gNumDaysInMonths[month - 1])
            errorFlags |= RTC_STAT_ERR_INVALID_DAY;
    }

    value = ConvertBcdToBinary(rtc->hour);

    if (value > 24)
        errorFlags |= RTC_STAT_ERR_INVALID_HOUR;

    value = ConvertBcdToBinary(rtc->minute);

    if (value > 60)
        errorFlags |= RTC_STAT_ERR_INVALID_MINUTE;

    value = ConvertBcdToBinary(rtc->second);

    if (value > 60)
        errorFlags |= RTC_STAT_ERR_INVALID_SECOND;

    return errorFlags;
}

void RtcReset()
{
    RtcDisableInterrupts();
    RTC_Reset();
    RtcRestoreInterrupts();
}

void FormatDecimalTime(u8 *dest, s32 hour, s32 minute, s32 second)
{
    dest = ConvertIntToDecimalStringN(dest, hour, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest++ = CHAR_COLON;
    dest = ConvertIntToDecimalStringN(dest, minute, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest++ = CHAR_COLON;
    dest = ConvertIntToDecimalStringN(dest, second, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest = EOS;
}

void FormatHexTime(u8 *dest, s32 hour, s32 minute, s32 second)
{
    dest = ConvertIntToHexStringN(dest, hour, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest++ = CHAR_COLON;
    dest = ConvertIntToHexStringN(dest, minute, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest++ = CHAR_COLON;
    dest = ConvertIntToHexStringN(dest, second, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest = EOS;
}

void FormatHexRtcTime(u8 *dest)
{
    FormatHexTime(dest, gRtcInfo.hour, gRtcInfo.minute, gRtcInfo.second);
}

void FormatDecimalDate(u8 *dest, s32 year, s32 month, s32 day)
{
    dest = ConvertIntToDecimalStringN(dest, year, STR_CONV_MODE_LEADING_ZEROS, 4);
    *dest++ = CHAR_HYPHEN;
    dest = ConvertIntToDecimalStringN(dest, month, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest++ = CHAR_HYPHEN;
    dest = ConvertIntToDecimalStringN(dest, day, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest = EOS;
}

void FormatHexDate(u8 *dest, s32 year, s32 month, s32 day)
{
    dest = ConvertIntToHexStringN(dest, year, STR_CONV_MODE_LEADING_ZEROS, 4);
    *dest++ = CHAR_HYPHEN;
    dest = ConvertIntToHexStringN(dest, month, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest++ = CHAR_HYPHEN;
    dest = ConvertIntToHexStringN(dest, day, STR_CONV_MODE_LEADING_ZEROS, 2);
    *dest = EOS;
}

void RtcCalcTimeDifference(struct RtcInfo *rtc, struct Time *result, struct Time *t)
{
    u16 days = RtcGetDayCount(rtc);
    result->seconds = ConvertBcdToBinary(rtc->second) - t->seconds;
    result->minutes = ConvertBcdToBinary(rtc->minute) - t->minutes;
    result->hours = ConvertBcdToBinary(rtc->hour) - t->hours;
    result->days = days - t->days;

    if (result->seconds < 0)
    {
        result->seconds += 60;
        --result->minutes;
    }

    if (result->minutes < 0)
    {
        result->minutes += 60;
        --result->hours;
    }

    if (result->hours < 0)
    {
        result->hours += 24;
        --result->days;
    }
}

void RtcCalcLocalTime()
{
    RtcGetInfo(&gRtcInfo);
    RtcCalcTimeDifference(&gRtcInfo, &gLocalTime, &gSaveBlock2.localTimeOffset);
}

void RtcInitLocalTimeOffset(s32 hour, s32 minute)
{
    RtcCalcLocalTimeOffset(0, hour, minute, 0);
}

void RtcCalcLocalTimeOffset(s32 days, s32 hours, s32 minutes, s32 seconds)
{
    gLocalTime.days = days;
    gLocalTime.hours = hours;
    gLocalTime.minutes = minutes;
    gLocalTime.seconds = seconds;
    RtcGetInfo(&gRtcInfo);
    RtcCalcTimeDifference(&gRtcInfo, &gSaveBlock2.localTimeOffset, &gLocalTime);
}

void CalcTimeDifference(struct Time *result, struct Time *t1, struct Time *t2)
{
    result->seconds = t2->seconds - t1->seconds;
    result->minutes = t2->minutes - t1->minutes;
    result->hours = t2->hours - t1->hours;
    result->days = t2->days - t1->days;

    if (result->seconds < 0)
    {
        result->seconds += 60;
        --result->minutes;
    }

    if (result->minutes < 0)
    {
        result->minutes += 60;
        --result->hours;
    }

    if (result->hours < 0)
    {
        result->hours += 24;
        --result->days;
    }
}

u32 RtcGetMinuteCount()
{
    RtcGetInfo(&gRtcInfo);
    return (24 * 60) * RtcGetDayCount(&gRtcInfo) + 60 * gRtcInfo.hour + gRtcInfo.minute;
}
