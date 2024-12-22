//
// Created by gengke on 2024/12/7.
//

#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H


struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#endif //KERNEL_TIME_H
