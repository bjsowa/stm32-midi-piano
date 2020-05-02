#include <stdlib.h>

#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/usbd.h>

#include <usb_descriptors.h>

/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[128];

static usbd_device *usbd_dev;
static bool usb_configured = false;

static bool key_states[8];
static uint8_t key_number[8] = { 44, 45, 46, 47, 48, 49, 50, 51 };
static uint16_t key_gpio[8]
    = { GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7 };

static void usbmidi_data_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
  char buf[64];
  int len = usbd_ep_read_packet(usbd_dev, 0x01, buf, 64);
}

static void usbmidi_set_config(usbd_device *usbd_dev, uint16_t wValue) {
  (void)wValue;

  usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, usbmidi_data_rx_cb);
  usbd_ep_setup(usbd_dev, 0x81, USB_ENDPOINT_ATTR_BULK, 64, NULL);

  usb_configured = true;
}

static void setup(void) {
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_AFIO);

  AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON;

  gpio_set_mode(
      GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO10);
  gpio_set(GPIOB, GPIO10);

  gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN,
      GPIO0 | GPIO1 | GPIO2 | GPIO3 | GPIO4 | GPIO5 | GPIO6 | GPIO7);
  gpio_clear(
      GPIOA, GPIO0 | GPIO1 | GPIO2 | GPIO3 | GPIO4 | GPIO5 | GPIO6 | GPIO7);

  dwt_enable_cycle_counter();
}

void sys_tick_handler() {
  if (!usb_configured) return;

  // char buf[4] = { 0x08, 0x80, 51, 64 };

  uint16_t key_presses = gpio_get(
      GPIOA, key_gpio[0] | key_gpio[1] | key_gpio[2] | key_gpio[3] | key_gpio[4]
                 | key_gpio[5] | key_gpio[6] | key_gpio[7]);

  for (int i = 0; i < 8; ++i) {
    if (key_presses & key_gpio[i]) {
      if (!key_states[i]) {
        char buf[4] = { 0x09, 0x90, key_number[i], 64 };
        key_states[i] = true;
        usbd_ep_write_packet(usbd_dev, 0x81, buf, sizeof(buf));
      }
    } else {
      if (key_states[i]) {
        char buf[4] = { 0x08, 0x80, key_number[i], 64 };
        key_states[i] = false;
        usbd_ep_write_packet(usbd_dev, 0x81, buf, sizeof(buf));
      }
    }
  }

  // if (pressed && !key_pressed) {
  //   key_pressed = true;
  //   buf[0] ^= 0x01;
  //   buf[1] ^= 0x10;
  //   usbd_ep_write_packet(usbd_dev, 0x81, buf, sizeof(buf));
  // } else if (!pressed && key_pressed) {
  //   key_pressed = false;
  //   usbd_ep_write_packet(usbd_dev, 0x81, buf, sizeof(buf));
  // }

  // uint32_t ticks = dwt_read_cycle_counter();
  // gpio_toggle(GPIOB, GPIO12);
}

int main(void) {
  setup();

  desig_get_unique_id_as_string(usb_serial_number, sizeof(usb_serial_number));

  usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 3,
      usbd_control_buffer, sizeof(usbd_control_buffer));

  usbd_register_set_config_callback(usbd_dev, usbmidi_set_config);

  usbd_poll(usbd_dev);

  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
  systick_set_reload(99999);
  systick_interrupt_enable();
  systick_counter_enable();

  while (1) {
    usbd_poll(usbd_dev);
  }
}
