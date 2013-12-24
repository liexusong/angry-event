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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ngr_event.h"

#ifdef HAVE_EPOLL
#include "ngr_epoll.c"
#else
# ifdef HAVE_KQUEUE
#  include "ngr_kqueue.c"
# else
#  include "ngr_select.c"
# endif
#endif


static int64_t ngr_event_current_time()
{
    struct timeval tv;
    int64_t ret;

    gettimeofday(&tv, NULL);

    ret  = (int64_t)tv.tv_sec * 1000;
    ret += (int64_t)tv.tv_usec / 1000;

    return ret;
}


ngr_event_t *ngr_event_new(int max_events)
{
    ngr_event_t *ev;
    int i;

    if (max_events <= 0) {
        max_events = NGR_DEFAULT_EVENTS;
    }

    ev = malloc(sizeof(*ev));
    if (ev == NULL) {
        return NULL;
    }

    ev->max_fd = -1;
    ev->max_events = max_events;
    ev->stop = 0;
    ev->free_timers = NULL;
    ev->free_timers_count = 0;

    ev->events = malloc(max_events * sizeof(ngr_event_node_t));
    if (ev->events == NULL) {
        free(ev);
        return NULL;
    }

    ev->fired = malloc(max_events * sizeof(ngr_event_fired_t));
    if (ev->fired == NULL) {
        free(ev->events);
        free(ev);
        return NULL;
    }

    rbtree_init(&ev->timer, &ev->sentinel); /* init timer */

    /* init event lib */
    if (ngr_event_lib_init(ev) != 0) {
        free(ev->events);
        free(ev->fired);
        free(ev);
        return NULL;
    }

    /* set all events to none */
    for (i = 0; i < ev->max_events; i++) {
        ev->events[i].mask = NGR_EVENT_NONE;
    }

    return ev;
}


void ngr_event_destroy(ngr_event_t *ev)
{
    ngr_event_timer_t *timer;

    ngr_event_lib_free_context(ev); /* free the event lib context */

    while (ev->free_timers) {
        timer = ev->free_timers;
        ev->free_timers = timer->next;
        free(timer);
    }

    free(ev->events);               /* free events array */
    free(ev->fired);                /* free fireds array */
    free(ev);                       /* free event object */
}


int ngr_event_create_io_event(ngr_event_t *ev, int fd, int mask,
    ngr_event_io_event_handler *handler, void *data)
{
    ngr_event_node_t *node;

    if (fd >= ev->max_events) return -1;

    /* add fd to event lib */
    if (ngr_event_lib_add_event(ev, fd, mask) == -1) return -1;

    node = &ev->events[fd]; /* event node */
    node->mask |= mask;
    node->fd = fd;
    node->data = data;

    if (mask & NGR_EVENT_READABLE) node->rev_handler = handler;
    if (mask & NGR_EVENT_WRITABLE) node->rev_handler = handler;

    if (fd > ev->max_fd) ev->max_fd = fd;

    return 0;
}


void ngr_event_del_io_event(ngr_event_t *ev, int fd, int mask)
{
    ngr_event_node_t *node;

    if (fd >= ev->max_events) return;

    node = &ev->events[fd];

    if (node->mask == NGR_EVENT_NONE) return;

    /* delete fd from event lib */
    ngr_event_lib_del_event(ev, fd, mask);

    node->mask = node->mask & (~mask);

    if (fd == ev->max_fd && node->mask == NGR_EVENT_NONE) {
        int j;

        for (j = ev->max_fd - 1; j >= 0; j--)
            if (ev->events[j].mask != NGR_EVENT_NONE) break;
        ev->max_fd = j;
    }
}


int ngr_event_create_timer(ngr_event_t *ev, int64_t timeout,
    ngr_event_timer_handler *handler, void *data,
    ngr_event_destroy_handler *destroy)
{
    ngr_event_timer_t *node;

    if (ev->free_timers_count > 0) {
        node = ev->free_timers;
        ev->free_timers = node->next;
        ev->free_timers_count--;

    } else {
        node = malloc(sizeof(*node));
        if (node == NULL) {
            return -1;
        }
    }

    node->handler = handler;
    node->data = data;
    node->destroy = destroy; /* destroy data handler */

    rbtree_node_init(&node->timer);
    node->timer.key = ngr_event_current_time() + timeout;
    node->timer.data = node; /* which timer belong to */

    rbtree_insert(&ev->timer, &node->timer);

    return 0;
}


static int ngr_event_process_timers(ngr_event_t *ev)
{
    struct rbnode *min_node;
    ngr_event_timer_t *timer;
    int64_t now, timeout;
    int processed = 0;

    while (1) {

        min_node = rbtree_min(&ev->timer); /* find the min timer node */
        if (min_node == NULL) {
            break;
        }

        now = ngr_event_current_time();

        if (min_node->key <= now) { /* timeout */

            timer = min_node->data;
            timeout = timer->handler(ev, timer->data);

            rbtree_delete(&ev->timer, min_node);

            if (timeout > 0) {  /* if had new timeout, we reinit this node */
                min_node->key = ngr_event_current_time() + timeout;
                min_node->data = timer;
                rbtree_insert(&ev->timer, min_node);

            } else {
                if (timer->destroy) {
                    timer->destroy(timer->data);
                }

                if (ev->free_timers_count < NGR_FREE_TIMERS_COUNT) {
                    timer->next = ev->free_timers;
                    ev->free_timers = timer;
                    ev->free_timers_count++;
                } else {
                    free(timer);
                }
            }

            processed++;
            continue;
        }

        break;
    }

    return processed;
}


int ngr_event_process_events(ngr_event_t *ev, int dont_wait)
{
    struct rbnode *min_node;
    struct timeval tv, *tvp;
    int num_events, j, processed = 0;

    min_node = rbtree_min(&ev->timer); /* find the min timer node */

    if (min_node != NULL) {

        int64_t now = ngr_event_current_time();
        int64_t remain = min_node->key - now;

        tvp = &tv;

        if (remain <= 0) {
            tvp->tv_sec  = 0;
            tvp->tv_usec = 0;
        } else {
            tvp->tv_sec  = remain / 1000;
            tvp->tv_usec = (remain % 1000) * 1000;
        }

    } else {
        if (dont_wait) { /* non-blocking */
            tvp = &tv;
            tvp->tv_sec  = 0;
            tvp->tv_usec = 0;
        } else {
            tvp = NULL;
        }
    }

    num_events = ngr_event_lib_poll(ev, tvp); /* waiting for event lib poll */

    for (j = 0; j < num_events; j++) {

        ngr_event_node_t *node = &ev->events[ev->fired[j].fd];
        int mask = ev->fired[j].mask;
        int fd = ev->fired[j].fd;
        int rfired = 0;

        if (node->mask & (mask & NGR_EVENT_READABLE)) { /* readable */
            rfired = 1;
            node->rev_handler(ev, fd, node->data, mask);
        }

        if (node->mask & (mask & NGR_EVENT_WRITABLE)) { /* writable */
            if (!rfired || node->wev_handler != node->rev_handler)
                node->wev_handler(ev, fd, node->data, mask);
        }

        processed++;
    }

    if (min_node != NULL) { /* process timer events */
        processed += ngr_event_process_timers(ev);
    }

    return processed;
}


void ngr_event_stop(ngr_event_t *ev)
{
    ev->stop = 1;
}


void ngr_event_loop(ngr_event_t *ev)
{
    while (!ev->stop) {
        (void)ngr_event_process_events(ev, 0);
    }
}
