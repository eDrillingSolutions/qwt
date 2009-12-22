/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "qwt_system_clock.h"
#include <qdatetime.h>
#include <unistd.h>

#if defined(_POSIX_TIMERS)
#include <time.h>
#define QWT_HIGH_RESOLUTION_CLOCK
#elif defined(Q_OS_WIN)
#define QWT_HIGH_RESOLUTION_CLOCK
#include <qt_windows.h>
#endif

#if defined(QWT_HIGH_RESOLUTION_CLOCK)

class QwtHighResolutionClock
{
public:
    QwtHighResolutionClock();

    void start();
    double restart();
    double elapsed() const;

    bool isValid() const;

    static double precision();

private:

#if defined(_POSIX_TIMERS)

    static double msecsTo(const struct timespec &, 
        const struct timespec &);

    static bool isMonotonic();

    struct timespec d_timeStamp;
    clockid_t d_clockId;

#elif defined(Q_OS_WIN)

    LARGE_INTEGER d_startTicks;
    long d_ticksPerSecond;
#endif
};

#if defined(_POSIX_TIMERS)

QwtHighResolutionClock::QwtHighResolutionClock()
{
    d_clockId = isMonotonic() ? CLOCK_MONOTONIC : CLOCK_REALTIME;
    d_timeStamp.tv_sec = d_timeStamp.tv_nsec = 0;
}

double QwtHighResolutionClock::precision() 
{
    struct timespec resolution;

    int clockId = isMonotonic() ? CLOCK_MONOTONIC : CLOCK_REALTIME;
    ::clock_getres(clockId, &resolution);

    return resolution.tv_nsec / 1e3;
}

inline bool QwtHighResolutionClock::isValid() const
{
    return d_timeStamp.tv_sec > 0 && d_timeStamp.tv_nsec > 0;
}

inline void QwtHighResolutionClock::start()
{
    ::clock_gettime(d_clockId, &d_timeStamp);
}

double QwtHighResolutionClock::restart()
{
    struct timespec timeStamp;
    ::clock_gettime(d_clockId, &timeStamp);

    double elapsed = 0.0;
    if ( isValid() )
        elapsed = msecsTo(d_timeStamp, timeStamp);

    d_timeStamp = timeStamp;
    return elapsed;
}

inline double QwtHighResolutionClock::elapsed() const
{
    struct timespec timeStamp;
    ::clock_gettime(d_clockId, &timeStamp);
    
    return msecsTo(d_timeStamp, timeStamp);
}   

inline double QwtHighResolutionClock::msecsTo(
    const struct timespec &t1, const struct timespec &t2) 
{
    return (t2.tv_sec - t1.tv_sec) * 1e3
            + (t2.tv_nsec - t1.tv_nsec) * 1e-6;
}

bool QwtHighResolutionClock::isMonotonic()
{
    // code copied from qcore_unix.cpp

#if (_POSIX_MONOTONIC_CLOCK-0 > 0) || defined(Q_OS_MAC)
    return true;
#else
    static int returnValue = 0;

    if (returnValue == 0) {
#if (_POSIX_MONOTONIC_CLOCK-0 < 0)
        returnValue = -1;
#elif (_POSIX_MONOTONIC_CLOCK == 0)
        // detect if the system support monotonic timers
        const long x = sysconf(_SC_MONOTONIC_CLOCK);
        returnValue = (x >= 200112L) ? 1 : -1;
#endif
    }

    return returnValue != -1;
#endif
}

#elif defined(Q_OS_WIN)

QwtHighResolutionClock::QwtHighResolutionClock():
    d_startTicks(0)
{
    QueryPerformanceFrequency(&d_ticksPerSecond);
}

double QwtHighResolutionClock::precision() 
{
    LARGE_INTEGER ticks;
    QueryPerformanceFrequency(&ticks);

    return (ticks <= 0) ? 0.0 : 1e3 / ticks;
}

inline bool QwtHighResolutionClock::isValid() const
{   
    return d_startTicks > 0;
}

inline void QwtHighResolutionClock::start()
{
    QueryPerformanceCounter(&d_startTicks);
}

inline double QwtHighResolutionClock::restart()
{
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);

    const double dt = ticks.QuadPart - d_startTicks.QuadPart;
    d_startTicks = ticks;

    return dt / d_ticksPerSecond * 1e3;
}

inline double QwtHighResolutionClock::elapsed() const
{
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);

    const double dt = ticks.QuadPart - d_startTicks.QuadPart;
    return dt / d_ticksPerSecond * 1e3;
}

#endif

#endif // QWT_HIGH_RESOLUTION_CLOCK

class QwtSystemClock::PrivateData
{
public:
#if defined(QWT_HIGH_RESOLUTION_CLOCK)
    QwtHighResolutionClock *clock;
#endif
    QTime time;
};

QwtSystemClock::QwtSystemClock()
{
    d_data = new PrivateData;

#if defined(QWT_HIGH_RESOLUTION_CLOCK)
    d_data->clock = NULL;
    if ( QwtHighResolutionClock::precision() > 0.0 )
        d_data->clock = new QwtHighResolutionClock;
#endif
}

QwtSystemClock::~QwtSystemClock()
{
#if defined(QWT_HIGH_RESOLUTION_CLOCK)
    delete d_data->clock;
#endif
    delete d_data;
}

bool QwtSystemClock::isValid() const
{
#if defined(QWT_HIGH_RESOLUTION_CLOCK)
    if ( d_data->clock )
        return d_data->clock->isValid();
#endif

    return d_data->time.isValid();
}

void QwtSystemClock::start()
{
#if defined(QWT_HIGH_RESOLUTION_CLOCK)
    if ( d_data->clock )
    {
        d_data->clock->start();
        return;
    }
#endif

    d_data->time.start();
}

double QwtSystemClock::restart()
{
#if defined(QWT_HIGH_RESOLUTION_CLOCK)
    if ( d_data->clock )
        return d_data->clock->restart();
#endif

    return d_data->time.restart();
}

double QwtSystemClock::elapsed() const
{
    double elapsed = 0.0;

#if defined(QWT_HIGH_RESOLUTION_CLOCK)
    if ( d_data->clock )
    {
        if ( d_data->clock->isValid() )
            elapsed = d_data->clock->elapsed();

        return elapsed;
    }
#endif

    if ( d_data->time.isValid() )
        elapsed = d_data->time.elapsed();

    return elapsed;
}

double QwtSystemClock::precision()
{
    static double prec = 0.0;
    if ( prec <= 0.0 )
    {
#if defined(QWT_HIGH_RESOLUTION_CLOCK)
        prec = QwtHighResolutionClock::precision();
#endif
        if ( prec <= 0.0 )
            prec = 1.0; // QTime offers 1 ms
    }

    return prec;
}

