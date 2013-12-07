
#include <string.h>
#include <sys/select.h>

struct ngr_event_lib_context {
    fd_set  rfds,  wfds;
    fd_set _rfds, _wfds;
};

static int ngr_event_lib_init(ngr_event_t *ev)
{
    struct ngr_event_lib_context *ctx = malloc(sizeof(*ctx));

    if (!ctx) return -1;

    FD_ZERO(&ctx->rfds);
    FD_ZERO(&ctx->wfds);

    ev->ctx = ctx;
    return 0;
}

static void ngr_event_lib_free_context(ngr_event_t *ev)
{
    free(ev->ctx);
}

static int ngr_event_lib_add_event(ngr_event_t *ev, int fd, int mask)
{
    struct ngr_event_lib_context *ctx = ev->ctx;

    if (mask & NGR_EVENT_READABLE) FD_SET(fd, &ctx->rfds);
    if (mask & NGR_EVENT_WRITABLE) FD_SET(fd, &ctx->wfds);

    return 0;
}

static void ngr_event_lib_del_event(ngr_event_t *ev, int fd, int mask)
{
    struct ngr_event_lib_context *ctx = ev->ctx;

    if (mask & NGR_EVENT_READABLE) FD_CLR(fd, &ctx->rfds);
    if (mask & NGR_EVENT_WRITABLE) FD_CLR(fd, &ctx->wfds);
}

static int ngr_event_lib_poll(ngr_event_t *ev, struct timeval *tvp)
{
    struct ngr_event_lib_context *ctx = ev->ctx;
    int retval, j, numevents = 0;

    memcpy(&ctx->_rfds, &ctx->rfds, sizeof(fd_set));
    memcpy(&ctx->_wfds, &ctx->wfds, sizeof(fd_set));

    retval = select(ev->max_fd + 1, &ctx->_rfds, &ctx->_wfds, NULL, tvp);

    if (retval > 0) {

        for (j = 0; j <= ev->max_fd; j++) {
            int mask = 0;
            ngr_event_node_t *node = &ev->events[j];

            if (node->mask == NGR_EVENT_NONE) continue;
            if (node->mask & NGR_EVENT_READABLE && FD_ISSET(j, &ctx->_rfds))
                mask |= NGR_EVENT_READABLE;
            if (node->mask & NGR_EVENT_WRITABLE && FD_ISSET(j, &ctx->_wfds))
                mask |= NGR_EVENT_WRITABLE;

            if (mask == 0) continue; /* !events */

            ev->fired[numevents].fd = j;
            ev->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;
}

char *ngr_event_lib_name(void)
{
    return "select";
}
