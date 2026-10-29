/* CV calibration probe: prints engine frame values for scripted changes. */
#include <stdio.h>
#include <string.h>
#include "p8_core.h"
#include "storage.h"
#include "engine_bridge.h"

static p8_core_t core;
static p8_cv_frame_t fr;
static void ticks(int n, const uint8_t *midi, uint16_t len) {
    for (int i = 0; i < n; i++) {
        p8_inputs_t in; memset(&in, 0, sizeof in);
        if (i == 0 && midi) { in.midi_in = midi; in.midi_in_len = len; }
        p8_core_tick(&core, &in, &fr);
    }
}
static void on(int note)  { uint8_t m[3] = {0x90, (uint8_t)note, 100}; ticks(30, m, 3); }
static void off(int note) { uint8_t m[3] = {0x80, (uint8_t)note, 0};   ticks(30, m, 3); }
static void setcp(int cp, double v) { currentPreset.continuousParameters[cp] = (uint16_t)(v * 65535.0); refreshFullState(); ticks(30, NULL, 0); }

int main(void) {
    p8_core_init(&core);
    preset_loadDefault(1);
    settings.panelLayout = 1; mixer_updatePanelLayout(1);
    refreshFullState();
    ticks(200, NULL, 0);
    printf("defaults: mvol=%u res=%u pw_a=%u vol_a=%u vol_b=%u bits=%u\n",
           fr.mvol, fr.res, fr.pw_a, fr.vol_a, fr.vol_b, fr.voice_bits);
    static const int notes[] = {36, 48, 60, 69, 72, 84, 96};
    for (unsigned i = 0; i < sizeof(notes)/sizeof(notes[0]); i++) {
        on(notes[i]);
        printf("note %3d: osc_a=%5u osc_b=%5u fil=%5u amp=%5u\n", notes[i], fr.osc_a[0], fr.osc_b[0], fr.fil[0], fr.amp[0]);
        off(notes[i]); ticks(40, NULL, 0);
    }
    on(60);
    for (double c = 0.0; c <= 1.001; c += 0.25) { setcp(cpCutoff, c); printf("cutoff %.2f: fil=%u\n", c, fr.fil[0]); }
    for (double r = 0.0; r <= 1.001; r += 0.5)  { setcp(cpResonance, r); printf("reso %.2f: res=%u fil=%u\n", r, fr.res, fr.fil[0]); }
    setcp(cpResonance, 0.2);
    for (double f = 0.0; f <= 1.001; f += 0.25) { setcp(cpFreqA, f); printf("freqA %.2f: osc_a=%u\n", f, fr.osc_a[0]); }
    setcp(cpFreqA, 0.5);
    /* glide ramp */
    setcp(cpGlide, 0.85); off(60); ticks(60, NULL, 0); on(60);
    uint8_t m[3] = {0x90, 72, 100}; p8_inputs_t in; memset(&in,0,sizeof in); in.midi_in=m; in.midi_in_len=3;
    p8_core_tick(&core, &in, &fr);
    for (int t = 0; t < 12; t++) { ticks(25, NULL, 0); printf("glide t+%3dms: osc_a=%u\n", (t+1)*25/2, fr.osc_a[0]); }
    /* mvol pot */
    off(72);
    { p8_inputs_t in2; memset(&in2,0,sizeof in2); in2.pots[14] = 65535; in2.pots[15] = 32768;
      for (int i=0;i<30;i++) p8_core_tick(&core,&in2,&fr); }
    printf("mvol with pot full: %u\n", fr.mvol);
    return 0;
}
