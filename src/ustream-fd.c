/*
 * ustream - library for stream buffer management
 *
 * Copyright (C) 2012 Felix Fietkau <nbd@openwrt.org>
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

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include "ustream.h"


static void ustream_fd_set_read_blocked(struct ustream *s){
	assert(0); 
	// TODO: add to uloop properly
	//ustream_fd_set_uloop(s, false);
}

static void ustream_fd_set_uloop(struct ustream *self, bool write){
	fprintf(stderr, "%s: not implemented!\n", __FUNCTION__); 
}
/*
static void ustream_fd_delete(struct ustream *self){
	fprintf(stderr, "%s: not implemented!\n", __FUNCTION__); 
}*/

static void ustream_fd_read_pending(struct ustream_fd *sf, bool *more){
	struct ustream *s = &sf->stream;
	int buflen = 0;
	ssize_t len;
	char *buf;

	do {
		if (s->read_blocked)
			break;

		buf = ustream_reserve(s, 1, &buflen);
		if (!buf)
			break;

		len = read(sf->fd.fd, buf, buflen);
		if (len < 0) {
			if (errno == EINTR)
				continue;

			if (errno == EAGAIN)
				return;

			len = 0;
		}

		if (!len) {
			if (!s->eof)
				ustream_state_change(s);
			s->eof = true;
			ustream_fd_set_uloop(s, false);
			return;
		}

		ustream_fill_read(s, len);
		*more = true;
	} while (1);
}

static int ustream_fd_write(struct ustream *s, const char *buf, int buflen, bool more)
{
	struct ustream_fd *sf = container_of(s, struct ustream_fd, stream);
	ssize_t ret = 0, len;

	if (!buflen)
		return 0;

	while (buflen) {
		len = write(sf->fd.fd, buf, buflen);

		if (len < 0) {
			if (errno == EINTR)
				continue;

			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;

			return -1;
		}

		ret += len;
		buf += len;
		buflen -= len;
	}

	if (buflen)
		ustream_fd_set_uloop(s, true);

	return ret;
}

static bool __ustream_fd_poll(struct ustream_fd *sf, unsigned int events)
{
	struct ustream *s = &sf->stream;
	bool more = false;

	if (events & ULOOP_READ)
		ustream_fd_read_pending(sf, &more);

	if (events & ULOOP_WRITE) {
		bool no_more = ustream_write_pending(s);
		if (no_more)
			ustream_fd_set_uloop(s, false);
	}

	return more;
}

static bool ustream_fd_poll(struct ustream *s)
{
	struct ustream_fd *sf = container_of(s, struct ustream_fd, stream);

	return __ustream_fd_poll(sf, ULOOP_READ | ULOOP_WRITE);
}

static void ustream_uloop_cb(struct uloop_fd *fd, unsigned int events)
{
	struct ustream_fd *sf = container_of(fd, struct ustream_fd, fd);

	__ustream_fd_poll(sf, events);
}

static void ustream_fd_free(struct ustream *s)
{
	//struct ustream_fd *sf = container_of(s, struct ustream_fd, stream);

	assert(0); 
	// TODO: implement ability to add fd to uloop but in a proper way
	//uloop_fd_delete(&sf->fd);
}

void ustream_fd_init(struct ustream_fd *sf, int fd)
{
	struct ustream *s = &sf->stream;

	ustream_init_defaults(s);

	sf->fd.fd = fd;
	sf->fd.cb = ustream_uloop_cb;
	s->set_read_blocked = ustream_fd_set_read_blocked;
	s->write = ustream_fd_write;
	s->free = ustream_fd_free;
	s->poll = ustream_fd_poll;
	ustream_fd_set_uloop(s, false);
}
