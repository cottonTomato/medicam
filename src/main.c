/**
 * SPDX-License-Identifier: Apache-2.0
 */

#include "camera.h"
#include "uart.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
LOG_MODULE_REGISTER(main);

int main(void)
{
	uint8_t recv_buffer[12] = {0};

	if (uart_setup() < 0) {
		return -1;
	}

	if (camera_setup() < 0) {
		return -1;
	}

	while (1) {
		if (!uart_available(recv_buffer)) {
			camera_recv_process(recv_buffer);
		}
		camera_video_preview();
		k_msleep(1);
	}

	return 0;
}
