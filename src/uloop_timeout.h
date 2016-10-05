#pragma once

#include <utype/utils.h>
#include <utype/list.h>

struct uloop_timeout; 

typedef void (*uloop_timeout_handler)(struct uloop_timeout *t);

struct uloop_timeout
{
	struct list_head list;
	bool pending;

	uloop_timeout_handler cb;
	struct timeval time;
};

typedef int64_t utick_t; 
utick_t utick_now(void); 
static inline bool utick_expired(utick_t t) { return (t - utick_now()) < 0; }

void clock_monotonic(struct timeval *tv); 
int uloop_timeout_set(struct uloop_timeout *timeout, int msecs);
int uloop_timeout_cancel(struct uloop_timeout *timeout);
int uloop_timeout_remaining(struct uloop_timeout *timeout);

static inline int _tv_diff(struct timeval *t1, struct timeval *t2){
	return
		(t1->tv_sec - t2->tv_sec) * 1000 +
		(t1->tv_usec - t2->tv_usec) / 1000;
}

