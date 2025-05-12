#include <stdint.h>

#define SERVER_IP   "192.168.29.67"
#define SERVER_PORT 8080

#define WIFI_SSID "MySSID"
#define WIFI_PSK  "MyPasswd"

int wifi_setup(void);
int tcp_client(void (*callback)(uint8_t *buffer));
