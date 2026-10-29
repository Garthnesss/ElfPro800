#include "engine_bridge.h"
#include "hardware.h"
#include "sh.h"
#include "dac.h"
#include "display.h"
#include "potmux.h"
#include "scanner.h"
#include "uart_6850.h"
#include <string.h>

static uint16_t g_cv[P8_BRIDGE_CV_COUNT];
static int8_t g_gate[7];
static uint16_t g_pots[POTMUX_POT_COUNT];
static int8_t g_buttons[40];
static int8_t g_keys[128];
static int8_t g_led_on[16];
static int8_t g_led_blink[16];
static char g_display[5];
static uint8_t g_mem[65536];
static uint8_t g_io[256];
static uint8_t g_storage[STORAGE_SIZE];
static uint8_t g_midi_out[P8_BRIDGE_MIDI_OUT_MAX];
static uint16_t g_midi_out_len;
static p600Pot_t g_last_changed;
static uint16_t g_last_changed_val[32];
static int8_t g_pot_changed[32];

void p8_bridge_reset(void)
{
    memset(g_cv, 0, sizeof(g_cv));
    memset(g_gate, 0, sizeof(g_gate));
    memset(g_pots, 0, sizeof(g_pots));
    memset(g_buttons, 0, sizeof(g_buttons));
    memset(g_keys, 0, sizeof(g_keys));
    memset(g_led_on, 0, sizeof(g_led_on));
    memset(g_led_blink, 0, sizeof(g_led_blink));
    memset(g_mem, 0, sizeof(g_mem));
    memset(g_io, 0, sizeof(g_io));
    /* The storage image is the synth's EEPROM: erase it only on the
       first reset in the process. A plugin host may construct and
       prepare several instances (scan, load, restore); their user
       presets must survive that, so re-inits keep the image. */
    static int storage_erased_once = 0;
    if (!storage_erased_once) {
        memset(g_storage, 0xff, sizeof(g_storage)); /* erased EEPROM reads 0xFF, like the real 24LC512 */
        storage_erased_once = 1;
    }
    memset(g_pot_changed, 0, sizeof(g_pot_changed));
    g_display[0] = ' '; g_display[1] = ' '; g_display[2] = ' '; g_display[3] = ' '; g_display[4] = '\0';
    g_midi_out_len = 0;
    g_last_changed = ppNone;
}

/* ---- inputs ---- */
void p8_bridge_set_pot(p600Pot_t pot, uint16_t value)
{
    if (pot >= 0 && pot < POTMUX_POT_COUNT) {
        if (g_pots[pot] != value) { g_pot_changed[pot] = 1; g_last_changed = pot; }
        g_pots[pot] = value;
    }
}
void p8_bridge_set_button(p600Button_t button, int8_t pressed)
{
    if (button >= 0 && button < 40) g_buttons[button] = pressed;
}
void p8_bridge_set_key(uint8_t key, int8_t pressed)
{
    if (key < 128) g_keys[key] = pressed;
}

/* ---- outputs ---- */
uint16_t p8_bridge_cv(p600CV_t cv) { return (cv >= 0 && cv < P8_BRIDGE_CV_COUNT) ? g_cv[cv] : 0; }
int8_t p8_bridge_gate(p600Gate_t gate) { return (gate >= 0 && gate < 7) ? g_gate[gate] : 0; }
void p8_bridge_display(char out[5]) { memcpy(out, g_display, 5); }
uint32_t p8_bridge_leds(void)
{
    uint32_t m = 0;
    int i;
    for (i = 0; i < 16; i++) if (g_led_on[i]) m |= (1u << i);
    return m;
}
uint16_t p8_bridge_midi_out_read(uint8_t *buf, uint16_t max)
{
    uint16_t n = g_midi_out_len < max ? g_midi_out_len : max;
    memcpy(buf, g_midi_out, n);
    memmove(g_midi_out, g_midi_out + n, g_midi_out_len - n);
    g_midi_out_len -= n;
    return n;
}

/* ---- hardware.h ---- */
void mem_write(uint16_t address, uint8_t value) { g_mem[address] = value; }
uint8_t mem_read(uint16_t address) { return g_mem[address]; }
void io_write(uint8_t address, uint8_t value) { g_io[address] = value; }
uint8_t io_read(uint8_t address) { (void)address; return 0; }
int8_t hardware_getNMIState(void) { return 0; }
void storage_write(uint32_t pageIdx, uint8_t *buf)
{
    uint32_t off = pageIdx * STORAGE_PAGE_SIZE;
    if (off + STORAGE_PAGE_SIZE <= STORAGE_SIZE) memcpy(g_storage + off, buf, STORAGE_PAGE_SIZE);
}
void storage_read(uint32_t pageIdx, uint8_t *buf)
{
    uint32_t off = pageIdx * STORAGE_PAGE_SIZE;
    if (off + STORAGE_PAGE_SIZE <= STORAGE_SIZE) memcpy(buf, g_storage + off, STORAGE_PAGE_SIZE);
    else memset(buf, 0, STORAGE_PAGE_SIZE);
}

/* ---- sh.h / dac.h ---- */
void sh_setCV(p600CV_t cv, uint16_t value, uint8_t flags) { (void)flags; if (cv >= 0 && cv < P8_BRIDGE_CV_COUNT) g_cv[cv] = value; }
void sh_setCV32Sat(p600CV_t cv, int32_t value, uint8_t flags)
{
    if (value < 0) value = 0;
    if (value > 65535) value = 65535;
    sh_setCV(cv, (uint16_t)value, flags);
}
void sh_setCV_FastPath(p600CV_t cv, uint16_t value) { sh_setCV(cv, value, 0); }
void sh_setCV32Sat_FastPath(p600CV_t cv, int32_t value) { sh_setCV32Sat(cv, value, 0); }
void sh_setGate(p600Gate_t gate, int8_t on) { if (gate >= 0 && gate < 7) g_gate[gate] = on; }
void sh_init(void) {}
void sh_update(void) {}
void dac_write(uint16_t value) { (void)value; }

/* ---- display.h ---- */
void sevenSeg_scrollText(const char *text, int8_t times) { (void)times; if (text) { g_display[0] = text[0] ? text[0] : ' '; g_display[1] = text[1] ? text[1] : ' '; g_display[2] = text[2] ? text[2] : ' '; g_display[3] = text[3] ? text[3] : ' '; } }
void sevenSeg_setAscii(char left, char right) { g_display[0] = left; g_display[1] = right; }
void sevenSeg_setNumber(int32_t n)
{
    char buf[16];
    int i = 0, j;
    long v = n;
    if (v < 0) v = -v;
    if (v == 0) buf[i++] = '0';
    while (v && i < 15) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    g_display[0] = ' '; g_display[1] = ' '; g_display[2] = ' '; g_display[3] = ' ';
    for (j = 0; j < i && j < 4; j++) g_display[3 - j] = buf[j];
}
void sevenSeg_setRelative(comparator_t comparator) { (void)comparator; }
int led_getOn(p600LED_t led) { return (led >= 0 && led < 16) ? g_led_on[led] : 0; }
int led_getBlinking(p600LED_t led) { return (led >= 0 && led < 16) ? g_led_blink[led] : 0; }
void led_set(p600LED_t led, int8_t on, int8_t blinking) { if (led >= 0 && led < 16) { g_led_on[led] = on; g_led_blink[led] = blinking; } }
void display_clear(void) { g_display[0] = g_display[1] = g_display[2] = g_display[3] = ' '; }
void display_init(void) {}
void display_update(int8_t fullUpdate) { (void)fullUpdate; }

/* ---- potmux.h ---- */
uint16_t potmux_getValue(p600Pot_t pot) { return (pot >= 0 && pot < POTMUX_POT_COUNT) ? g_pots[pot] : 0; }
int8_t potmux_hasChanged(p600Pot_t pot) { return (pot >= 0 && pot < POTMUX_POT_COUNT) ? g_pot_changed[pot] : 0; }
p600Pot_t potmux_lastChanged(void) { return g_last_changed; }
void potmux_resetChanged(void)
{
    p600Pot_t p = g_last_changed;
    memset(g_pot_changed, 0, sizeof(g_pot_changed));
    g_last_changed = ppNone;
    (void)p;
}
void potmux_resetChangedFull(void) { potmux_resetChanged(); }
void potmux_resetSpeedPot(void) {}
void potmux_init(void) {}
void potmux_update(uint8_t updateAll) { (void)updateAll; }
uint8_t comparePotVal(p600Pot_t pot, uint16_t potValue, uint16_t compareValue)
{
    (void)pot; (void)potValue; (void)compareValue;
    return 0;
}
int8_t potmux_isPotZeroCentered(p600Pot_t pot, uint8_t layout) { (void)pot; (void)layout; return 0; }

/* ---- scanner.h ---- */
int8_t scanner_keyState(uint8_t key) { return key < 128 ? g_keys[key] : 0; }
int8_t scanner_buttonState(p600Button_t button) { return (button >= 0 && button < 40) ? g_buttons[button] : 0; }
void scanner_init(void) {}
void scanner_update(int8_t fullScan) { (void)fullScan; }
int8_t scanner_isKeyDown(uint8_t note) { return note < 128 ? g_keys[note] : 0; }

/* ---- uart_6850.h ---- */
void uart_init(void) {}
void uart_send(uint8_t data)
{
    if (g_midi_out_len < P8_BRIDGE_MIDI_OUT_MAX) g_midi_out[g_midi_out_len++] = data;
}
void uart_update(void) {}
