
#include <sys/epoll.h>

struct ngr_event_lib_context {
    int epfd;
    struct epoll_event *events;
};

static int ngr_event_lib_init(ngr_event_t *ev)
{
    struct ngr_event_lib_context *ctx = malloc(sizeof(*ctx));

    if (!ctx) return -1;

    ctx->events = malloc(sizeof(struct epoll_event) * ev->max_events);
    if (!ctx->events) {
        free(ctx);
        return -1;
    }

    ctx->epfd = epoll_create(1024); /* 1024 is just an hint for the kernel */
    if (ctx->epfd == -1) {
        free(ctx->events);
        free(ctx);
        return -1;
    }

    ev->ctx = ctx;
    return 0;
}

static void ngr_event_lib_free_context(ngr_event_t *ev)
{
    struct ngr_event_lib_context *ctx = ev->ctx;

    close(ctx->epfd);
    free(ctx->events);
    free(ctx);
}

static int ngr_event_lib_add_event(ngr_event_t *ev, int fd, int mask)
{
    struct ngr_event_lib_context *ctx = ev->ctx;
    struct epoll_event ee;
    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. */
    int op = ev->events[fd].mask == NGR_EVENT_NONE ?
                                     EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= ev->events[fd].mask;
    if (mask & NGR_EVENT_READABLE) ee.events |= EPOLLIN;
    if (mask & NGR_EVENT_WRITABLE) ee.events |= EPOLLOUT;

    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;

    if (epoll_ctl(ctx->epfd, op, fd, &ee) == -1)
        return -1;
    return 0;
}

static void ngr_event_lib_del_event(ngr_event_t *ev, int fd, int delmask)
{
    struct ngr_event_lib_context *ctx = ev->ctx;
    struct epoll_event ee;
    int mask = ev->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & NGR_EVENT_READABLE) ee.events |= EPOLLIN;
    if (mask & NGR_EVENT_WRITABLE) ee.events |= EPOLLOUT;

    ee.data.u64 = 0; /* avoid valgrind warning */
    ee.data.fd = fd;

    if (mask != NGR_EVENT_NONE) {
        epoll_ctl(ctx->epfd, EPOLL_CTL_MOD, fd, &ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

static int ngr_event_lib_poll(ngr_event_t *ev, struct timeval *tvp)
{
    struct ngr_event_lib_context *ctx = ev->ctx;
    int retval, numevents = 0;

    retval = epoll_wait(ctx->epfd, ctx->events, ev->max_events,
            tvp ? (tvp->tv_sec * 1000 + tvp->tv_usec / 1000) : -1);

    if (retval > 0) {
        int j;

        numevents = retval;

        for (j = 0; j < numevents; j++) {

            int mask = 0;
            struct epoll_event *e = ctx->events + j;

            if (e->events & EPOLLIN)  mask |= NGR_EVENT_READABLE;
            if (e->events & EPOLLOUT) mask |= NGR_EVENT_WRITABLE;

            ev->fired[j].fd = e->data.fd;
            ev->fired[j].mask = mask;
        }
    }

    return numevents;
}

char *ngr_event_lib_name(void)
{
    return "epoll";
}
