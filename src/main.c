#include <stdlib.h>

#include <libopencm3/stm32/desig.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/usbd.h>

#include <usb_descriptors.h>

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

/* SysEx identity message, preformatted with correct USB framing information */
const uint8_t sysex_identity[] = {
  0x04, /* USB Framing (3 byte SysEx) */
  0xf0, /* SysEx start */
  0x7e, /* non-realtime */
  0x00, /* Channel 0 */
  0x04, /* USB Framing (3 byte SysEx) */
  0x7d, /* Educational/prototype manufacturer ID */
  0x66, /* Family code (byte 1) */
  0x66, /* Family code (byte 2) */
  0x04, /* USB Framing (3 byte SysEx) */
  0x51, /* Model number (byte 1) */
  0x19, /* Model number (byte 2) */
  0x00, /* Version number (byte 1) */
  0x04, /* USB Framing (3 byte SysEx) */
  0x00, /* Version number (byte 2) */
  0x01, /* Version number (byte 3) */
  0x00, /* Version number (byte 4) */
  0x05, /* USB Framing (1 byte SysEx) */
  0xf7, /* SysEx end */
  0x00, /* Padding */
  0x00, /* Padding */
};

static void usbmidi_data_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
  (void)ep;

  char buf[64];
  int len = usbd_ep_read_packet(usbd_dev, 0x01, buf, 64);

  /* This implementation treats any message from the host as a SysEx
   * identity request. This works well enough providing the host
   * packs the identify request in a single 8 byte USB message.
   */
  if (len) {
    while (usbd_ep_write_packet(
               usbd_dev, 0x81, sysex_identity, sizeof(sysex_identity))
           == 0)
      ;
  }

  gpio_toggle(GPIOC, GPIO5);
}

static void usbmidi_set_config(usbd_device *usbd_dev, uint16_t wValue) {
  (void)wValue;

  usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, usbmidi_data_rx_cb);
  usbd_ep_setup(usbd_dev, 0x81, USB_ENDPOINT_ATTR_BULK, 64, NULL);
}

static void button_send_event(usbd_device *usbd_dev, int pressed) {
  char buf[4] = {
    0x08, /* USB framing: virtual cable 0, note on */
    0x80, /* MIDI command: note on, channel 1 */
    60,   /* Note 60 (middle C) */
    64,   /* "Normal" velocity */
  };

  buf[0] |= pressed;
  buf[1] |= pressed << 4;

  while (usbd_ep_write_packet(usbd_dev, 0x81, buf, sizeof(buf)) == 0)
    ;
}

static void button_poll(usbd_device *usbd_dev) {
  static uint32_t button_state = 0;

  /* This is a simple shift based debounce. It's simplistic because
   * although this implements debounce adequately it does not have any
   * noise suppression. It is also very wide (32-bits) because it can
   * be polled in a very tight loop (no debounce timer).
   */
  uint32_t old_button_state = button_state;
  button_state = (button_state << 1) | (GPIOA_IDR & 1);
  if ((0 == button_state) != (0 == old_button_state)) {
    button_send_event(usbd_dev, !!button_state);
  }
}
static void setup(void) {
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_AFIO);

  AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON;

  gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO0);
  gpio_clear(GPIOA, GPIO0);
}

int main(void) {
  usbd_device *usbd_dev;

  setup();

  desig_get_unique_id_as_string(usb_serial_number, sizeof(usb_serial_number));

  usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 3,
      usbd_control_buffer, sizeof(usbd_control_buffer));

  usbd_register_set_config_callback(usbd_dev, usbmidi_set_config);

  while (1) {
    usbd_poll(usbd_dev);
    button_poll(usbd_dev);
  }
  // while (1) {
  //   gpio_toggle(GPIOB, GPIO12);
  //   for (i = 0; i < 1000000; i++) {
  //     __asm__("nop");
  //   }
  // }
}
