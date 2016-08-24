
#include "config.h"

#include <lo/lo.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include <mapper/mapper.h>

static double multiplier = 1.0/((double)(1LL<<32));

/*! Internal function to get the current time. */
double mapper_get_current_time()
{
#ifdef HAVE_GETTIMEOFDAY
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
#else
#error No timing method known on this platform.
#endif
}

void mapper_clock_init(mapper_clock clock)
{
    mapper_clock_now(clock, &clock->now);
    clock->next_ping = clock->now.sec;
}

void mapper_clock_now(mapper_clock clock,
                      mapper_timetag_t *timetag)
{
    lo_timetag_now((lo_timetag*)timetag);
}

double mapper_timetag_difference(const mapper_timetag_t a,
                                 const mapper_timetag_t b)
{
    return (double)a.sec - (double)b.sec +
        ((double)a.frac - (double)b.frac) * multiplier;
}

void mapper_timetag_add_double(mapper_timetag_t *a, double b)
{
    if (!b)
        return;

    b += (double)a->frac * multiplier;
    a->sec += floor(b);
    b -= floor(b);
    if (b < 0.0) {
        --a->sec;
        b = 1.0 - b;
    }
    a->frac = (uint32_t) (((double)b) * (double)(1LL<<32));
}

void mapper_timetag_multiply(mapper_timetag_t *tt, double d)
{
    d *= mapper_timetag_double(*tt);
    tt->sec = floor(d);
    d -= tt->sec;
    tt->frac = (uint32_t) (d * (double)(1LL<<32));
}

void mapper_timetag_add(mapper_timetag_t *tt, mapper_timetag_t addend)
{
    tt->sec += addend.sec;
    tt->frac += addend.frac;
    if (tt->frac < addend.frac) // overflow
        tt->sec++;
}

void mapper_timetag_subtract(mapper_timetag_t *tt, mapper_timetag_t subtrahend)
{
    tt->sec -= subtrahend.sec;
    tt->frac -= subtrahend.frac;
    if (tt->frac > subtrahend.frac) // overflow
        tt->sec--;
}

double mapper_timetag_double(mapper_timetag_t timetag)
{
    return (double)timetag.sec + (double)timetag.frac * multiplier;
}

void mapper_timetag_set_double(mapper_timetag_t *tt, double value)
{
    tt->sec = floor(value);
    value -= tt->sec;
    tt->frac = (uint32_t) (((double)value) * (double)(1LL<<32));
}

void mapper_timetag_copy(mapper_timetag_t *ttl, mapper_timetag_t ttr)
{
    ttl->sec = ttr.sec;
    ttl->frac = ttr.frac;
}
