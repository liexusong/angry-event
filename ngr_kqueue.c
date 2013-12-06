
#include <sys/event.h>

struct ngr_event_lib_context {
    int kqfd;
    struct kevent *events;
};


static int ngr_event_lib_init(ngr_event_t *ev)
{
    struct ngr_event_lib_context *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        return -1;
    }

    ctx->events = malloc(sizeof(struct kevent) * ev->max_events);
    if (!ctx->events) {
        free(ctx);
        return -1;
    }

    ctx->kqfd = kqueue();
    if (ctx->kqfd == -1) {
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

    close(ctx->kqfd);
    free(ctx->events);
    free(ctx);
}

static int ngr_event_lib_add_event(ngr_event_t *ev, int fd, int mask)
{
    struct ngr_event_lib_context *ctx = ev->ctx;
    struct kevent ke;

    if (mask & NGR_EVENT_READABLE) {
        EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(ctx->kqfd, &ke, 1, NULL, 0, NULL) == -1) return -1;
    }
    if (mask & NGR_EVENT_WRITABLE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if (kevent(ctx->kqfd, &ke, 1, NULL, 0, NULL) == -1) return -1;
    }
    return 0;
}

static void ngr_event_lib_del_event(ngr_event_t *ev, int fd, int mask)
{
    struct ngr_event_lib_context *ctx = ev->ctx;
    struct kevent ke;

    if (mask & NGR_EVENT_READABLE) {
        EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        kevent(ctx->kqfd, &ke, 1, NULL, 0, NULL);
    }
    if (mask & NGR_EVENT_WRITABLE) {
        EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        kevent(ctx->kqfd, &ke, 1, NULL, 0, NULL);
    }
}

static int ngr_event_lib_poll(ngr_event_t *ev, struct timeval *tvp)
{
    struct ngr_event_lib_context *ctx = ev->ctx;
    int retval, numevents = 0;

    if (tvp != NULL) {
        struct timespec timeout;

        timeout.tv_sec = tvp->tv_sec;
        timeout.tv_nsec = tvp->tv_usec * 1000;

        retval = kevent(ctx->kqfd, NULL, 0, ctx->events, ev->max_events,
              &timeout);

    } else {
        retval = kevent(ctx->kqfd, NULL, 0, ctx->events, ev->max_events, NULL);
    }    

    if (retval > 0) {
        int j;
        
        numevents = retval;
        for(j = 0; j < numevents; j++) {
            int mask = 0;
            struct kevent *e = ctx->events + j;

            if (e->filter == EVFILT_READ)  mask |= NGR_EVENT_READABLE;
            if (e->filter == EVFILT_WRITE) mask |= NGR_EVENT_WRITABLE;

            ev->fired[j].fd = e->ident; 
            ev->fired[j].mask = mask;           
        }
    }

    return numevents;
}

char *ngr_event_lib_name(void)
{
    return "kqueue";
}
