#ifndef PHASE_CUT_H_INCLUDED
#define PHASE_CUT_H_INCLUDED

#include <stdint.h>


void phase_cut_init(void);
void phase_cut_set_period(uint32_t usecs);
void phase_cut_timer_enable(int enable);
void phase_cut_set_percentage(unsigned int perc);
void phase_cut_stop(void);

#endif