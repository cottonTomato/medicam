#include "camera_comm.h"
#include "uart.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main);

#define MSG_SIZE 12
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

const struct device *console;

void serial_cb(const struct device *dev, void *user_data);

void camera_char_send(char out_char)
{
	uart_poll_out(console, out_char);
}

void camera_buffer_send(uint8_t *buffer, uint32_t length)
{
	for (uint32_t i = 0; i < length; i++) {
		uart_poll_out(console, buffer[i]);
	}
}

uint8_t uart_available(uint8_t *p)
{
	return k_msgq_get(&uart_msgq, p, K_NO_WAIT);
}

int uart_setup(void)
{
	console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	if (!device_is_ready(console)) {
		LOG_ERR("%s: device not ready.", console->name);
		return -1;
	}
	uart_irq_callback_user_data_set(console, serial_cb, NULL);
	uart_irq_rx_enable(console);

	return 0;
}

void uart_main_loop(uint8_t (*callback)(uint8_t *buffer), int buff_len)
{
	uint8_t recv_buffer[256] = {0};

	while (1) {
		if (!uart_available(recv_buffer)) {
			callback(recv_buffer);
		}
		k_msleep(1);
	}
}

static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	while (uart_fifo_read(dev, &c, 1) == 1) {
		if (c == 0xAA && rx_buf_pos > 0) {
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);
			rx_buf_pos = 0;
		} else if (c == 0x55) {
			rx_buf_pos = 0;
		} else {
			rx_buf[rx_buf_pos] = c;
			if (++rx_buf_pos >= MSG_SIZE) {
				rx_buf_pos = 0;
			}
		}
	}
}
