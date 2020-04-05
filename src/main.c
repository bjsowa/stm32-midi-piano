#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

static void setup(void) {
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
  rcc_periph_clock_enable(RCC_GPIOB);
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,
                GPIO12);
  gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO11);
}

int main(void) {
  int i;
  setup();
  while (1) {
    gpio_toggle(GPIOB, GPIO12);
    for (i = 0; i < 1000000; i++) {
      __asm__("nop");
    }
  }

  return 0;
}
