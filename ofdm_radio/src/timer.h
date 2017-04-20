/*
 *
 * Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
 * 
 */

//
// timer
//

#ifndef __TIMER_H__
#define __TIMER_H__

// 
// timer object interface declarations
//

typedef struct timer_s * timer;

// create timer object
timer timer_create();

// destroy timer object
void timer_destroy(timer _q);

// reset timer
void timer_tic(timer _q);

// get elapsed time since 'tic' in seconds
float timer_toc(timer _q);

#endif // __TIMER_H__

