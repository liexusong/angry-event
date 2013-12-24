Angry Event Library
===================

Angry Event is a simple and easy to use event library. example:

<pre>
#include &lt;stdlib.h&gt;
#include &lt;stdio.h&gt;
#include "ngr_event.h"

uint64_t timer_handler(ngr_event_t *ev, void *data)
{
    printf("This is timer called\n");
    return 1000;
}

int main(int argc, char *argv[])
{
    ngr_event_t *ev = ngr_event_new(0);
    if (!ev) {
        printf("can not create event object\n");
        exit(-1);
    }

    ngr_event_create_timer(ev, 1000, &timer_handler, NULL, NULL);
    ngr_event_loop(ev);
    return 0;
}
</pre>
