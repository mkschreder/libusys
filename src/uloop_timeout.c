#include "uloop_timeout.h"

void clock_monotonic(struct timeval *tv){
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
}

int uloop_timeout_set(struct uloop_timeout *self, int msecs){
	struct timeval *time = &self->time;

	if (self->pending)
		return -1; 
		//uloop_remove_timeout(self->loop, self);

	clock_monotonic(time);

	time->tv_sec += msecs / 1000;
	time->tv_usec += (msecs % 1000) * 1000;

	if (time->tv_usec > 1000000) {
		time->tv_sec++;
		time->tv_usec -= 1000000;
	}
	
	return 0; 
	// TODO: make sure it works in uloop
	//return uloop_add_timeout(self->loop, self);
}

int uloop_timeout_cancel(struct uloop_timeout *self){
	if (!self->pending)
		return -1;

	list_del(&self->list);
	self->pending = false;

	return 0;
}

int uloop_timeout_remaining(struct uloop_timeout *self){
	struct timeval now;

	if (!self->pending)
		return -1;

	clock_monotonic(&now);

	return _tv_diff(&self->time, &now);
}


