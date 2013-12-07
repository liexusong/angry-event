/*
 * Copyright (c) 2012-2013, Liexusong <liexusong at qq dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NGR_EVENT_H
#define _NGR_EVENT_H

#include "ngr_rbtree.h"

#define NGR_DEFAULT_EVENTS 10240

#define NGR_EVENT_NONE      0
#define NGR_EVENT_READABLE  1
#define NGR_EVENT_WRITABLE  2

typedef unsigned char ngr_uint8_t;
typedef struct ngr_event_s ngr_event_t;
typedef struct ngr_event_timer_s ngr_event_timer_t;

typedef void ngr_event_ioevent_handler(ngr_event_t *ev, int fd, void *data,
    int mask);
typedef uint64_t ngr_event_timer_handler(ngr_event_t *ev, void *data);
typedef void ngr_event_timer_destroy_handler(void *data);


typedef struct ngr_event_node_s {
    int mask;
    ngr_event_ioevent_handler *rev_handler;
    ngr_event_ioevent_handler *wev_handler;
    void *data;
    ngr_event_timer_t *timer;
    ngr_uint8_t timer_set:1;
    ngr_uint8_t timeout:1;
} ngr_event_node_t;


typedef struct ngr_event_fired_s {
    int fd;
    int mask;
} ngr_event_fired_t;


struct ngr_event_timer_s {
    ngr_event_timer_handler *handler;
    void *data;
    ngr_event_timer_destroy_handler *destroy;
    struct rbnode timer;
};


struct ngr_event_s {
    int max_fd;
    int max_events;
    ngr_event_node_t *events;
    ngr_event_fired_t *fired;
    struct rbtree timer;
    struct rbnode sentinel;
    void *ctx;
    ngr_uint8_t stop:1;
};


ngr_event_t *ngr_event_new(int max_events);
int ngr_event_create_ioevent(ngr_event_t *ev, int fd, int mask,
    ngr_event_ioevent_handler *handler, void *data, uint64_t timeout);
void ngr_event_del_ioevent(ngr_event_t *ev, int fd, int mask);
ngr_event_timer_t *ngr_event_create_timer(ngr_event_t *ev, int64_t timeout,
    ngr_event_timer_handler *handler, void *data,
    ngr_event_timer_destroy_handler *destroy);
void ngr_event_del_timer(ngr_event_t *ev, ngr_event_timer_t *node);
int ngr_event_process_events(ngr_event_t *ev, int dont_wait);
void ngr_event_main_stop(ngr_event_t *ev);
void ngr_event_main_loop(ngr_event_t *ev);
char *ngr_event_lib_name();

#endif
