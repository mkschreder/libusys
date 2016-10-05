/*
 * uloop - event loop implementation
 *
 * Copyright (C) 2015 Martin K. Schröder <mkschreder.uk@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * uloop - event loop implementation
 *
 * Copyright (C) 2010-2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#pragma once 

#include <sys/time.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
#define USE_KQUEUE
#else
#define USE_EPOLL
#endif

#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif
#include <sys/wait.h>

#include <utype/list.h>

#include "uloop_timeout.h"

struct uloop_fd;
struct uloop_timeout;
struct uloop_process;

typedef void (*uloop_fd_handler)(struct uloop_fd *u, unsigned int events);
typedef void (*uloop_process_handler)(struct uloop_process *c, int ret);

#define ULOOP_READ		(1 << 0)
#define ULOOP_WRITE		(1 << 1)
#define ULOOP_EDGE_TRIGGER	(1 << 2)
#define ULOOP_BLOCKING		(1 << 3)

#define ULOOP_EVENT_MASK	(ULOOP_READ | ULOOP_WRITE)

/* internal flags */
#define ULOOP_EVENT_BUFFERED	(1 << 4)
#ifdef USE_KQUEUE
#define ULOOP_EDGE_DEFER	(1 << 5)
#endif

#define ULOOP_ERROR_CB		(1 << 6)

struct uloop_fd
{
	uloop_fd_handler cb;
	int fd;
	bool eof;
	bool error;
	bool registered;
	uint8_t flags;
};

struct uloop_process
{
	struct list_head list;
	bool pending;

	uloop_process_handler cb;
	pid_t pid;
};

struct uloop_fd_event {
	struct uloop_fd *fd;
	unsigned int events;
};

struct uloop_fd_stack {
	struct uloop_fd_stack *next;
	struct uloop_fd *fd;
	unsigned int events;
};

#define ULOOP_MAX_EVENTS 10

struct uloop {
	struct uloop_fd_stack *fd_stack; 
	struct list_head timeouts; 
	struct list_head processes;
	int poll_fd;
	bool cancelled;
	bool do_sigchld;
	struct uloop_fd_event cur_fds[ULOOP_MAX_EVENTS];
	struct epoll_event events[ULOOP_MAX_EVENTS];
	int cur_fd, cur_nfds;
	int recursive_calls;
}; 

int uloop_add_fd(struct uloop *self, struct uloop_fd *sock, unsigned int flags);
int uloop_remove_fd(struct uloop *self, struct uloop_fd *sock);

int uloop_add_timeout(struct uloop *self, struct uloop_timeout *timeout);
int uloop_remove_timeout(struct uloop *self, struct uloop_timeout *timeout); 

int uloop_add_process(struct uloop *self, struct uloop_process *p);
int uloop_remove_process(struct uloop *self, struct uloop_process *p);

struct uloop *uloop_new(void); 
void uloop_delete(struct uloop **self); 

void uloop_init(struct uloop *self);
void uloop_destroy(struct uloop *self); 

int uloop_process_events(struct uloop *self); 

void uloop_run(struct uloop *self);

