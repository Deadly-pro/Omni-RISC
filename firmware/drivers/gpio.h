// Omni-RISC APU — gpio driver header
#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

void gpio_write(uint8_t val);
uint8_t gpio_read(void);

#endif
