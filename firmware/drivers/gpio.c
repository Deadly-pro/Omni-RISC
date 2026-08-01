// Omni-RISC APU — gpio driver implementation
#include "gpio.h"

#define GPIO_BASE 0x40001000
#define GPIO_OUT  (*(volatile uint32_t *)(GPIO_BASE + 0x00))

void gpio_write(uint8_t val) {
    GPIO_OUT = (uint32_t)val;
}

uint8_t gpio_read(void) {
    return (uint8_t)GPIO_OUT;
}
