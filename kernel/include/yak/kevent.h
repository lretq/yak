#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/object.h>

/* semaphore that can only ever reach sigstate=1 */
struct kevent {
	struct kobject_header hdr;
};

void event_init(struct kevent *event, int sigstate);
void event_alarm(struct kevent *event);

#ifdef __cplusplus
}
#endif
