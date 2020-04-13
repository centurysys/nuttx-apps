#ifndef _XG50_APPS_MESSAGE_H
#define _XG50_APPS_MESSAGE_H

#include <time.h>
#include <sys/time.h>
#include "gpsutils/minmea.h"

typedef enum message_code {
  /* GPS -> Main */
  GPS_INFO = 0x0101,
} message_code_t;

typedef struct msg_header {
  int task_id;
  message_code_t code;
} msg_header_t;

typedef struct msg {
  msg_header_t header;
  char buf[0];
} msg_t;


/****************************************************************************
 * Message contents
 ****************************************************************************/

/* GPS -> Main */
struct msg_gps_data {
  struct minmea_float latitude;
  struct minmea_float longitude;
};

#endif /* _XG50_APPS_MESSAGE_H */
