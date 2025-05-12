#include <stdint.h>

int uart_setup(void);
void uart_main_loop(uint8_t (*callback)(uint8_t *buffer), int buff_len);
