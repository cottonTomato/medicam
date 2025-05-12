/**
 * SPDX-License-Identifier: Apache-2.0
 */

#include "camera.h"

#ifdef USE_UART

#include "uart.h"

#elif defined(USE_WIFI)

#include "wifi.h"

#endif

#include <zephyr/logging/log.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
LOG_MODULE_REGISTER(main);

int main(void)
{
	int buff_len = 256;
	if (camera_setup() < 0) {
		return -1;
	}

#ifdef USE_UART

	if (uart_setup() < 0) {
		return -1;
	}

	uart_main_loop(camera_recv_process, buff_len);

#elif defined(USE_WIFI)

	if (wifi_setup() < 0) {
		return -1;
	}

	if (tcp_setup() < 0) {
		return -1;
	}

	tcp_main_loop(camera_recv_process, buff_len);

#endif

	// Infinite Loop
	while (1) {
	}

	return 0;
}
