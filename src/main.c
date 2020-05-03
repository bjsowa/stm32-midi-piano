#include <stdlib.h>

#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/usbd.h>

#include <midi_queue.h>
#include <usb_descriptors.h>

#define MIDI_EVENT_BUFFER_SIZE 128

/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[128];

static struct midi_event midi_event_buffer[MIDI_EVENT_BUFFER_SIZE];
static struct midi_queue midi_event_queue;

static usbd_device *usbd_dev;
static bool usb_configured = false;

static bool key_states[8];
static uint8_t key_number[8] = { 44, 45, 46, 47, 48, 49, 50, 51 };
static uint16_t key_gpio[8]
    = { GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7 };
static uint16_t key_gpios;

static void usbmidi_set_config(usbd_device *usbd_dev, uint16_t wValue) {
  usbd_ep_setup(usbd_dev, 0x81, USB_ENDPOINT_ATTR_BULK, 64, NULL);
  usb_configured = true;
}

static void clock_setup(void) {
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
}

static void gpio_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_AFIO);

  AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON;

  gpio_set_mode(
      GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO10);
  gpio_set(GPIOB, GPIO10);

  for (int i = 0; i < 8; ++i) key_gpios |= key_gpio[i];

  gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, key_gpios);
  gpio_clear(GPIOA, key_gpios);

  // LED
  gpio_set_mode(
      GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
  gpio_set(GPIOB, GPIO12);
}

static void systick_setup(void) {
  dwt_enable_cycle_counter();

  midi_queue_init(&midi_event_queue, midi_event_buffer, MIDI_EVENT_BUFFER_SIZE);

  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
  systick_set_reload(99999);
  systick_interrupt_enable();
  systick_counter_enable();
}

static void usb_setup(void) {
  desig_get_unique_id_as_string(usb_serial_number, sizeof(usb_serial_number));

  usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 3,
      usbd_control_buffer, sizeof(usbd_control_buffer));

  usbd_register_set_config_callback(usbd_dev, usbmidi_set_config);
}

void sys_tick_handler() {
  uint16_t key_presses = gpio_get(GPIOA, key_gpios);

  for (int i = 0; i < 8; ++i) {
    if (key_presses & key_gpio[i]) {
      if (!key_states[i]) {
        key_states[i] = true;
        struct midi_event event = {
          .type = NOTE_ON,
          .note = key_number[i],
          .velocity = 64,
        };
        midi_queue_push(&midi_event_queue, &event);
      }
    } else {
      if (key_states[i]) {
        key_states[i] = false;
        struct midi_event event = {
          .type = NOTE_OFF,
          .note = key_number[i],
        };
        midi_queue_push(&midi_event_queue, &event);
      }
    }
  }
}

static void midi_event_poll() {
  static uint8_t buffer[64];
  uint8_t events_nr = 0;

  systick_interrupt_disable();

  while (!midi_queue_empty(&midi_event_queue) && events_nr < 16) {
    struct midi_event event = midi_queue_pop(&midi_event_queue);
    if (event.type == NOTE_ON) {
      buffer[4 * events_nr + 0] = 0x09;
      buffer[4 * events_nr + 1] = 0x90;
    } else {
      buffer[4 * events_nr + 0] = 0x08;
      buffer[4 * events_nr + 1] = 0x80;
    }
    buffer[4 * events_nr + 2] = event.note;
    buffer[4 * events_nr + 3] = event.velocity;
    ++events_nr;
  }

  systick_interrupt_enable();

  if (events_nr == 0 || !usb_configured)
    return;

  while (usbd_ep_write_packet(usbd_dev, 0x81, buffer, 4 * events_nr) == 0)
    ;
}

int main(void) {
  clock_setup();
  gpio_setup();
  systick_setup();
  usb_setup();

  while (1) {
    usbd_poll(usbd_dev);
    midi_event_poll();
  }
}
