#include "camera_comm.h"
#include "wifi.h"

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main);

int sock;

void camera_char_send(char out_char)
{
	send(sock, &out_char, 1, 0);
}

void camera_buffer_send(uint8_t *buffer, uint32_t length)
{
	send(sock, buffer, length, 0);
}

int wifi_setup(void)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params cnx = {0};

	cnx.ssid = (uint8_t *)WIFI_SSID;
	cnx.ssid_length = strlen(WIFI_SSID);
	cnx.psk = (uint8_t *)WIFI_PSK;
	cnx.psk_length = strlen(WIFI_PSK);
	cnx.security = WIFI_SECURITY_TYPE_PSK;
	cnx.band = WIFI_FREQ_BAND_2_4_GHZ;
	cnx.channel = 0;
	cnx.timeout = SYS_FOREVER_MS;

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx, sizeof(cnx))) {
		LOG_ERR("Wi-Fi connect request failed.");
		return -1;
	}

	LOG_INF("Wi-Fi connection Successful.");

	return 0;
}

int tcp_client(void (*callback)(uint8_t *buffer))
{
	struct sockaddr_in addr;
	char buf[256];

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_ERR("Socket creation failed");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	net_addr_pton(AF_INET, SERVER_IP, &addr.sin_addr);

	LOG_INF("Connecting to %s:%d...", SERVER_IP, SERVER_PORT);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		LOG_ERR("Connection to Server Failed.");
		return -1;
	}
	LOG_INF("Connected to %s:%d!.", SERVER_IP, SERVER_PORT);

	while (1) {
		int len = recv(sock, buf, sizeof(buf) - 1, 0);
		if (len > 0) {
			buf[len] = '\0';
			callback(buf);
		} else if (len == 0) {
			LOG_ERR("Server closed the connection.");
			break;
		} else {
			LOG_ERR("recv() error: %d", errno);
			break;
		}
	}

	close(sock);
	LOG_INF("Disconnected.");

	return 0;
}
