#include <stdint.h>

#define SERVER_IP   "192.168.29.67"
#define SERVER_PORT 8080

#define WIFI_SSID "MySSID"
#define WIFI_PSK  "MyPasswd"

int wifi_setup(void);
int tcp_setup(void);
void tcp_main_loop(uint8_t (*callback)(uint8_t *buffer), int buff_len);
