#ifndef _LIB_LIBSERIAL_H
#define _LIB_LIBSERIAL_H

#include <termios.h>

typedef struct {
  int fd;
  struct termios oldtermio;
} serial_t;


serial_t *serial_open(char *port, int baudrate, int rtscts);
int serial_close(serial_t *ser);

int ser_wait_recv(serial_t *ser, int timeout_ms);
int ser_readline(serial_t *ser, char *buf, int buflen, char delim, int timeout_ms);
int ser_write(serial_t *ser, char *buf, int buflen);

#endif
