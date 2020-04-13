#ifndef _XG50_APPS_RESOURCES_H
#define _XG50_APPS_RESOURCES_H

#include "message.h"

#ifndef ALIGN
#define ALIGN(x, a)            __ALIGN_MASK(x, (typeof(x)) (a) - 1)
#define __ALIGN_MASK(x, mask)  (((x) + (mask)) & ~(mask))
#define PTR_ALIGN(p, a)        ((typeof(p)) ALIGN((unsigned long) (p), (a)))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* mailbox */
#define MBOX_MAIN   "mailbox_main"

/* mempool */
#define MEMPOOL_GPS  "mempool_gps"

#define MSG_HEADER_SIZE     (sizeof(struct msg_header))

#define MEMPOOL_SIZE_GPS ALIGN((MSG_HEADER_SIZE + sizeof(struct msg_gps_data) + 16), 4)
#define MEMPOOL_NUMS_GPS 16

#define TASK_ID_MAIN   0
#define TASK_ID_GPS    1
#define TASK_ID_LAST   1

#endif /* _XG50_APPS_RESOURCES_H */
