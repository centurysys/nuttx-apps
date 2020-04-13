#ifndef _LIB_CONFIGURATION_H
#define _LIB_CONFIGURATION_H

#define LEN_APN      32
#define LEN_USER     16
#define LEN_PASSWORD 16
#define LEN_HOST     32

typedef struct {
  char apn[LEN_APN];
  char user[LEN_USER];
  char password[LEN_PASSWORD];

  char host[LEN_HOST];
  uint32_t port;
  uint16_t id;

  int valid;
} config_t;

int load_config(config_t *config);
int save_config(config_t *config);

int configuration(int argc, char **argv);
const char *server_host(config_t *config);

#endif
