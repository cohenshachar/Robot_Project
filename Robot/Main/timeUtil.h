#pragma once
#include <sys/time.h>
#include <time.h>

inline String timeHMSms() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);

    char hms[9];
    strftime(hms, sizeof(hms), "%H-%M-%S", &tm);

    char stamp[16];
    snprintf(stamp, sizeof(stamp), "%s-%03ld", hms, tv.tv_usec / 1000L);

    return String(stamp);
}