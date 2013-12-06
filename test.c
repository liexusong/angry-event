#include <stdlib.h>
#include <stdio.h>
#include "ngr_event.h"

uint32_t timer_handler(ngr_event_t *ev, void *data)
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
    ngr_event_main_loop(ev);
    return 0;
}
