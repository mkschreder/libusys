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
#include <sys/time.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <assert.h>

#include "uloop.h"
#include "ustream.h"

//#include "utils.h"

#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif
#include <sys/wait.h>

void uloop_init(struct uloop *self){
	self->fd_stack = NULL; 
	INIT_LIST_HEAD(&self->timeouts); 
	INIT_LIST_HEAD(&self->processes); 
	self->cancelled = false; 
	self->do_sigchld = false; 
	self->cur_fd = self->cur_nfds = 0; 

	self->recursive_calls = 0; 
	self->poll_fd = epoll_create(32);
	fcntl(self->poll_fd, F_SETFD, fcntl(self->poll_fd, F_GETFD) | FD_CLOEXEC);
}

struct uloop *uloop_new(void){
	struct uloop *self = malloc(sizeof(struct uloop)); 
	uloop_init(self); 
	return self; 
}

/**
 * FIXME: uClibc < 0.9.30.3 does not define EPOLLRDHUP for Linux >= 2.6.17
 */
#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

static int _uloop_register_poll_fd(struct uloop *self, struct uloop_fd *fd, unsigned int flags){
	struct epoll_event ev;
	int op = fd->registered ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	memset(&ev, 0, sizeof(struct epoll_event));

	if (flags & ULOOP_READ)
		ev.events |= EPOLLIN | EPOLLRDHUP;

	if (flags & ULOOP_WRITE)
		ev.events |= EPOLLOUT;

	if (flags & ULOOP_EDGE_TRIGGER)
		ev.events |= EPOLLET;

	ev.data.fd = fd->fd;
	ev.data.ptr = fd;
	fd->flags = flags;

	return epoll_ctl(self->poll_fd, op, fd->fd, &ev);
}

static int _uloop_fetch_events(struct uloop *self, int timeout)
{
	int n, nfds;

	nfds = epoll_wait(self->poll_fd, self->events, sizeof(self->events) / sizeof(self->events[0]), timeout);
	for (n = 0; n < nfds; ++n) {
		struct uloop_fd_event *cur = &self->cur_fds[n];
		struct uloop_fd *u = self->events[n].data.ptr;
		unsigned int ev = 0;

		cur->fd = u;
		if (!u)
			continue;

		if (self->events[n].events & (EPOLLERR|EPOLLHUP)) {
			u->error = true;
			if (!(u->flags & ULOOP_ERROR_CB))
				uloop_remove_fd(self, u);
		}

		if(!(self->events[n].events & (EPOLLRDHUP|EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP))) {
			cur->fd = NULL;
			continue;
		}

		if(self->events[n].events & EPOLLRDHUP)
			u->eof = true;

		if(self->events[n].events & EPOLLIN)
			ev |= ULOOP_READ;

		if(self->events[n].events & EPOLLOUT)
			ev |= ULOOP_WRITE;

		cur->events = ev;
	}

	return nfds;
}

static bool _uloop_fd_stack_event(struct uloop *self, struct uloop_fd *fd, int events){
	struct uloop_fd_stack *cur;

	/*
	 * Do not buffer events for level-triggered fds, they will keep firing.
	 * Caller needs to take care of recursion issues.
	 */
	if (!(fd->flags & ULOOP_EDGE_TRIGGER))
		return false;

	for (cur = self->fd_stack; cur; cur = cur->next) {
		if (cur->fd != fd)
			continue;

		if (events < 0)
			cur->fd = NULL;
		else
			cur->events |= events | ULOOP_EVENT_BUFFERED;

		return true;
	}

	return false;
}

static void _uloop_run_events(struct uloop *self, int timeout)
{
	struct uloop_fd_event *cur;
	struct uloop_fd *fd;

	if (!self->cur_nfds) {
		self->cur_fd = 0;
		self->cur_nfds = _uloop_fetch_events(self, timeout);
		if (self->cur_nfds < 0)
			self->cur_nfds = 0;
	}

	while (self->cur_nfds > 0) {
		struct uloop_fd_stack stack_cur;
		unsigned int events;

		cur = &self->cur_fds[self->cur_fd++];
		self->cur_nfds--;

		fd = cur->fd;
		events = cur->events;
		if (!fd)
			continue;

		if (!fd->cb)
			continue;

		if (_uloop_fd_stack_event(self, fd, cur->events))
			continue;

		stack_cur.next = self->fd_stack;
		stack_cur.fd = fd;
		self->fd_stack = &stack_cur;
		do {
			stack_cur.events = 0;
			fd->cb(fd, events);
			events = stack_cur.events & ULOOP_EVENT_MASK;
		} while (stack_cur.fd && events);
		self->fd_stack = stack_cur.next;

		return;
	}
}

int uloop_add_fd(struct uloop *self, struct uloop_fd *sock, unsigned int flags){
	unsigned int fl;
	int ret;

	if (!(flags & (ULOOP_READ | ULOOP_WRITE)))
		return uloop_remove_fd(self, sock);

	if (!sock->registered && !(flags & ULOOP_BLOCKING)) {
		fl = fcntl(sock->fd, F_GETFL, 0);
		fl |= O_NONBLOCK;
		fcntl(sock->fd, F_SETFL, fl);
	}

	ret = _uloop_register_poll_fd(self, sock, flags);
	if (ret < 0)
		goto out;

	sock->registered = true;
	sock->eof = false;
	sock->error = false;

out:
	return ret;
}

int uloop_remove_fd(struct uloop *self, struct uloop_fd *fd){
	int i;

	for (i = 0; i < self->cur_nfds; i++) {
		if (self->cur_fds[self->cur_fd + i].fd != fd)
			continue;

		self->cur_fds[self->cur_fd + i].fd = NULL;
	}

	if (!fd->registered)
		return 0;

	fd->registered = false;
	_uloop_fd_stack_event(self, fd, -1);
	
	fd->flags = 0;
	return epoll_ctl(self->poll_fd, EPOLL_CTL_DEL, fd->fd, 0);
}

void uloop_add_ustream(struct uloop *self, struct ustream *s, bool write){
	struct ustream_fd *sf = container_of(s, struct ustream_fd, stream);
	struct ustream_buf *buf;
	unsigned int flags = ULOOP_EDGE_TRIGGER;

	if (!s->read_blocked && !s->eof)
		flags |= ULOOP_READ;

	buf = s->w.head;
	if (write || (buf && s->w.data_bytes && !s->write_error))
		flags |= ULOOP_WRITE;

	uloop_add_fd(self, &sf->fd, flags);
}

int uloop_add_timeout(struct uloop *self, struct uloop_timeout *timeout){
	struct uloop_timeout *tmp;
	struct list_head *h = &self->timeouts;

	if (timeout->pending)
		return -1;

	list_for_each_entry(tmp, &self->timeouts, list) {
		if (_tv_diff(&tmp->time, &timeout->time) > 0) {
			h = &tmp->list;
			break;
		}
	}

	list_add_tail(&timeout->list, h);
	timeout->pending = true;

	return 0;
}
int uloop_add_process(struct uloop *self, struct uloop_process *p){
	struct uloop_process *tmp;
	struct list_head *h = &self->processes;

	if (p->pending)
		return -1;

	list_for_each_entry(tmp, &self->processes, list) {
		if (tmp->pid > p->pid) {
			h = &tmp->list;
			break;
		}
	}

	list_add_tail(&p->list, h);
	p->pending = true;

	return 0;
}

int uloop_remove_process(struct uloop *self, struct uloop_process *p){
	if (!p->pending)
		return -1;

	list_del(&p->list);
	p->pending = false;

	return 0;
}

static void _uloop_handle_processes(struct uloop *self)
{
	struct uloop_process *p, *tmp;
	pid_t pid;
	int ret;

	self->do_sigchld = false;

	while (1) {
		pid = waitpid(-1, &ret, WNOHANG);
		if (pid <= 0)
			return;

		list_for_each_entry_safe(p, tmp, &self->processes, list) {
			if (p->pid < pid)
				continue;

			if (p->pid > pid)
				break;

			uloop_remove_process(self, p);
			p->cb(p, ret);
		}
	}

}

static void _uloop_handle_sigint(int signo){
	// TODO: keep a list of uloops and send signal to all!
	//self->cancelled = true;
}

static void _uloop_sigchld(int signo){
	// TODO: keep a list of uloops and send signal to all!
	//self->do_sigchld = true;
}

static void _uloop_install_handler(int signum, void (*handler)(int), struct sigaction* old, bool add){
	struct sigaction s;
	struct sigaction *act;

	act = NULL;
	sigaction(signum, NULL, &s);

	if (add) {
		if (s.sa_handler == SIG_DFL) { /* Do not override existing custom signal handlers */
			memcpy(old, &s, sizeof(struct sigaction));
			s.sa_handler = handler;
			s.sa_flags = 0;
			act = &s;
		}
	}
	else if (s.sa_handler == handler) { /* Do not restore if someone modified our handler */
			act = old;
	}

	if (act != NULL)
		sigaction(signum, act, NULL);
}

static void _uloop_ignore_signal(int signum, bool ignore)
{
	struct sigaction s;
	void *new_handler = NULL;

	sigaction(signum, NULL, &s);

	if (ignore) {
		if (s.sa_handler == SIG_DFL) /* Ignore only if there isn't any custom handler */
			new_handler = SIG_IGN;
	} else {
		if (s.sa_handler == SIG_IGN) /* Restore only if noone modified our SIG_IGN */
			new_handler = SIG_DFL;
	}

	if (new_handler) {
		s.sa_handler = new_handler;
		s.sa_flags = 0;
		sigaction(signum, &s, NULL);
	}
}

static void _uloop_setup_signals(bool add)
{
	static struct sigaction old_sigint, old_sigchld, old_sigterm;

	_uloop_install_handler(SIGINT, _uloop_handle_sigint, &old_sigint, add);
	_uloop_install_handler(SIGTERM, _uloop_handle_sigint, &old_sigterm, add);
	_uloop_install_handler(SIGCHLD, _uloop_sigchld, &old_sigchld, add);

	_uloop_ignore_signal(SIGPIPE, add);
}

static int _uloop_get_next_timeout(struct uloop *self, struct timeval *tv)
{
	struct uloop_timeout *timeout;
	int diff;

	if (list_empty(&self->timeouts))
		return -1;

	timeout = list_first_entry(&self->timeouts, struct uloop_timeout, list);
	diff = _tv_diff(&timeout->time, tv);
	if (diff < 0)
		return 0;

	return diff;
}

static void _uloop_process_timeouts(struct uloop *self, struct timeval *tv){
	struct uloop_timeout *t;

	while (!list_empty(&self->timeouts)) {
		t = list_first_entry(&self->timeouts, struct uloop_timeout, list);

		if (_tv_diff(&t->time, tv) > 0)
			break;

		uloop_timeout_cancel(t);
		if (t->cb)
			t->cb(t);
	}
}

static void _uloop_clear_timeouts(struct uloop *self){
	struct uloop_timeout *t, *tmp;

	list_for_each_entry_safe(t, tmp, &self->timeouts, list)
		uloop_timeout_cancel(t);
}

static void _uloop_clear_processes(struct uloop *self){
	struct uloop_process *p, *tmp;

	list_for_each_entry_safe(p, tmp, &self->processes, list)
		uloop_remove_process(self, p);
}

int uloop_process_events(struct uloop *self){
	struct timeval tv;
	clock_monotonic(&tv);
	_uloop_process_timeouts(self, &tv);

	if (self->do_sigchld)
		_uloop_handle_processes(self);

	if (self->cancelled)
		return -1;

	clock_monotonic(&tv);
	_uloop_run_events(self, _uloop_get_next_timeout(self, &tv));
	return 0; 
}

void uloop_run(struct uloop *self){

	/*
	 * Handlers are only updated for the first call to uloop_run() (and restored
	 * when this call is done).
	 */
	if (!self->recursive_calls++)
		_uloop_setup_signals(true);

	self->cancelled = false;
	while(!self->cancelled){
		uloop_process_events(self); 
	}

	if (!--self->recursive_calls)
		_uloop_setup_signals(false);
}

void uloop_destroy(struct uloop *self){
	if (self->poll_fd < 0)
		return;

	close(self->poll_fd);
	self->poll_fd = -1;

	_uloop_clear_timeouts(self);
	_uloop_clear_processes(self);
}

void uloop_delete(struct uloop **self){
	assert(self); 
	uloop_destroy(*self); 
	*self = NULL; 
}

