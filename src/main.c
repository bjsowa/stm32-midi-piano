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
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_AFIO);

  AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON;

  // gpio_set_mode(GPIOA, G PIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO0);
  // gpio_clear(GPIOA, GPIO0);
  gpio_set_mode(
      GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
  gpio_clear(GPIOB, GPIO12);

  dwt_enable_cycle_counter();
}

void sys_tick_handler() {
  char buf[4] = {
    0x08, /* USB framing: virtual cable 0, note on */
    0x80, /* MIDI command: note on, channel 1 */
    60,   /* Note 60 (middle C) */
    64,   /* "Normal" velocity */
  };
  uint32_t ticks = dwt_read_cycle_counter();
  if (usb_configured) {
    usbd_ep_write_packet(usbd_dev, 0x81, buf, sizeof(buf));
  }
  gpio_toggle(GPIOB, GPIO12);
}

int main(void) {
  setup();

  desig_get_unique_id_as_string(usb_serial_number, sizeof(usb_serial_number));

  usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 3,
      usbd_control_buffer, sizeof(usbd_control_buffer));

  usbd_register_set_config_callback(usbd_dev, usbmidi_set_config);

  usbd_poll(usbd_dev);

  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
  systick_set_reload(899999);
  systick_interrupt_enable();
  systick_counter_enable();

  while (1) {
    usbd_poll(usbd_dev);
  }
}
