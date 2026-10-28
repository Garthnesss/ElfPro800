/* ElfChords P8 render tool (V0 placeholder DSP).
 *
 *   render <in.mid> <out_prefix>   -> writes <out_prefix>.csv (one row per
 *                                     2 kHz CvFrame) and <out_prefix>.wav
 *                                     (48 kHz mono PCM16)
 *   render --verify <file.wav>     -> checks WAV header, length and peak;
 *                                     exit 0 only if it looks like audio
 *
 * The DSP here is a *placeholder*: pitch comes from the MIDI notes
 * themselves; the engine's per-voice amp/filter CVs (assigned to notes in
 * note-on order, mirroring the assigner) shape gain and a 1-pole lowpass.
 * The real analog voice model is V1 work (targets/plugin/dsp).
 */
#include "p8_core.h"
#include "storage.h" /* preset_loadDefault */
#include "engine_bridge.h" /* refreshFullState decl */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 48000
#define FRAME_RATE P8_CONTROL_RATE_HZ /* 2000 */
#define SAMPLES_PER_FRAME (SR / FRAME_RATE) /* 24 */

typedef struct { uint32_t tick; uint8_t len; uint8_t bytes[3]; } ev_t;
static ev_t g_events[4096];
static int g_nevents;
static uint16_t g_ppqn = 480;
static uint32_t g_tempo = 500000; /* us per quarter */

static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static int parse_midi(const char *path)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long size;
    const uint8_t *p, *end;
    if (!fp) { fprintf(stderr, "cannot open %s\n", path); return -1; }
    fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 0, SEEK_SET);
    buf = malloc(size);
    if (fread(buf, 1, size, fp) != (size_t)size) { fclose(fp); free(buf); return -1; }
    fclose(fp);
    if (size < 14 || memcmp(buf, "MThd", 4)) { free(buf); return -1; }
    g_ppqn = rd16(buf + 12);
    p = buf + 14; end = buf + size;
    while (p + 8 <= end && g_nevents < 4000) {
        uint32_t len;
        const uint8_t *tp, *tend;
        uint32_t tick = 0;
        uint8_t status = 0;
        if (memcmp(p, "MTrk", 4)) break;
        len = rd32(p + 4);
        tp = p + 8; tend = tp + len;
        while (tp < tend) {
            uint32_t delta = 0;
            uint8_t b;
            do { b = *tp++; delta = (delta << 7) | (b & 0x7f); } while (b & 0x80);
            tick += delta;
            b = *tp;
            if (b & 0x80) { status = b; tp++; } /* else running status */
            if (status == 0xff) {
                uint8_t type = *tp++;
                uint32_t mlen = 0;
                do { b = *tp++; mlen = (mlen << 7) | (b & 0x7f); } while (b & 0x80);
                if (type == 0x51 && mlen == 3)
                    g_tempo = ((uint32_t)tp[0] << 16) | ((uint32_t)tp[1] << 8) | tp[2];
                tp += mlen;
            } else if (status == 0xf0 || status == 0xf7) {
                uint32_t slen = 0;
                do { b = *tp++; slen = (slen << 7) | (b & 0x7f); } while (b & 0x80);
                tp += slen;
            } else {
                int ndata = ((status & 0xe0) == 0xc0) ? 1 : 2;
                ev_t *e = &g_events[g_nevents++];
                e->tick = tick;
                e->len = (uint8_t)(1 + ndata);
                e->bytes[0] = status;
                e->bytes[1] = ndata > 0 ? *tp++ : 0;
                e->bytes[2] = ndata > 1 ? *tp++ : 0;
            }
        }
        p = tend;
    }
    free(buf);
    return 0;
}

static double tick_to_sec(uint32_t tick)
{
    return (double)tick / g_ppqn * (g_tempo / 1e6);
}

typedef struct { int note; double phase, lp; int slot; } voice_render_t;

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "--verify")) {
        FILE *fp = fopen(argv[2], "rb");
        uint8_t hdr[44];
        long size;
        int16_t peak = 0;
        if (!fp) return 1;
        if (fread(hdr, 1, 44, fp) != 44 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) { fclose(fp); return 1; }
        fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 44, SEEK_SET);
        for (;;) { int16_t s; if (fread(&s, 2, 1, fp) != 1) break; if (s == INT16_MIN) s = INT16_MAX; if (abs(s) > peak) peak = (int16_t)abs(s); }
        fclose(fp);
        printf("verify: %ld bytes, peak %d\n", size, peak);
        return (size > 48000 && peak > 1000) ? 0 : 1;
    }
    if (argc != 3) { fprintf(stderr, "usage: render <in.mid> <out_prefix> | render --verify <file.wav>\n"); return 2; }
    if (parse_midi(argv[1])) { fprintf(stderr, "bad midi file\n"); return 1; }

    {
        char csvpath[512], wavpath[512];
        FILE *csv, *wav;
        p8_core_t core;
        p8_inputs_t in;
        double dur = 0.5;
        uint32_t total_frames, fi;
        int ei = 0;
        voice_render_t vr[16];
        int nvr = 0;

        snprintf(csvpath, sizeof(csvpath), "%s.csv", argv[2]);
        snprintf(wavpath, sizeof(wavpath), "%s.wav", argv[2]);
        if (g_nevents) { double t = tick_to_sec(g_events[g_nevents - 1].tick) + 0.5; if (t > dur) dur = t; }
        total_frames = (uint32_t)(dur * FRAME_RATE);

        csv = fopen(csvpath, "w");
        wav = fopen(wavpath, "wb");
        if (!csv || !wav) { fprintf(stderr, "cannot write outputs\n"); return 1; }
        /* WAV header placeholder, patched at end */
        { uint8_t h[44] = {0}; fwrite(h, 1, 44, wav); }
        fprintf(csv, "frame,amp0,amp1,amp2,amp3,amp4,amp5,osc_a0,fil0,mvol\n");

        memset(&in, 0, sizeof(in));
        memset(vr, 0, sizeof(vr));
        p8_core_init(&core);
        /* Select the init patch: booting into blank (all-zero) storage
         * loads an all-zero preset whose envelopes are 2 ms blips; the
         * firmware's own default preset + full refresh is the sane
         * starting point every hardware user gets from preset load. */
        preset_loadDefault(1);
        refreshFullState();
        for (fi = 0; fi < total_frames; fi++) {
            double tsec = (double)fi / FRAME_RATE;
            p8_cv_frame_t f;
            int s, v;
            /* deliver all MIDI events due by this frame; events are offset by a
             * 100 ms boot guard: the engine drops notes gated during its
             * post-init preset load (observed on host: note at frame 0 is
             * retuned but never gated; at frame >=100 it sounds). */
            in.midi_in = NULL; in.midi_in_len = 0;
            {
                static uint8_t chunk[3 * 64];
                uint16_t n = 0;
                while (ei < g_nevents && tick_to_sec(g_events[ei].tick) + 0.1 <= tsec && n + 3 <= sizeof(chunk)) {
                    ev_t *e = &g_events[ei++];
                    /* note bookkeeping for the placeholder DSP */
                    uint8_t st = e->bytes[0] & 0xf0, note = e->bytes[1], vel = e->bytes[2];
                    if (st == 0x90 && vel) {
                        if (nvr < 16) { vr[nvr].note = note; vr[nvr].phase = 0; vr[nvr].lp = 0; vr[nvr].slot = nvr; nvr++; }
                    } else if (st == 0x80 || (st == 0x90 && !vel)) {
                        int k;
                        for (k = 0; k < nvr; k++) if (vr[k].note == note) { memmove(&vr[k], &vr[k + 1], (nvr - k - 1) * sizeof(vr[0])); nvr--; break; }
                    }
                    memcpy(chunk + n, e->bytes, e->len);
                    n += e->len;
                }
                if (n) { in.midi_in = chunk; in.midi_in_len = n; }
            }
            p8_core_tick(&core, &in, &f);
            fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", fi,
                    f.amp[0], f.amp[1], f.amp[2], f.amp[3], f.amp[4], f.amp[5],
                    f.osc_a[0], f.fil[0], f.mvol);
            for (s = 0; s < SAMPLES_PER_FRAME; s++) {
                double mix = 0;
                for (v = 0; v < nvr; v++) {
                    double freq = 440.0 * pow(2.0, (vr[v].note - 69) / 12.0);
                    double saw, amp, cut, a;
                    vr[v].phase += freq / SR;
                    if (vr[v].phase >= 1.0) vr[v].phase -= 1.0;
                    saw = 2.0 * vr[v].phase - 1.0;
                    amp = vr[v].slot < 6 ? f.amp[vr[v].slot] / 65535.0 : 0.0;
                    cut = vr[v].slot < 6 ? f.fil[vr[v].slot] / 65535.0 : 0.5;
                    a = 0.02 + 0.9 * cut * cut;
                    vr[v].lp += a * (saw - vr[v].lp);
                    mix += vr[v].lp * amp;
                }
                {
                    int32_t smp = (int32_t)(mix * 0.25 * 32767.0);
                    int16_t o;
                    if (smp > 32767) smp = 32767;
                    if (smp < -32768) smp = -32768;
                    o = (int16_t)smp;
                    fwrite(&o, 2, 1, wav);
                }
            }
        }
        /* patch WAV header */
        {
            long bytes = ftell(wav);
            uint32_t data = (uint32_t)(bytes - 44);
            uint8_t h[44];
            memcpy(h, "RIFF", 4); h[4] = (bytes - 8) & 0xff; h[5] = ((bytes - 8) >> 8) & 0xff; h[6] = ((bytes - 8) >> 16) & 0xff; h[7] = ((bytes - 8) >> 24) & 0xff;
            memcpy(h + 8, "WAVEfmt ", 8);
            h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
            h[20] = 1; h[21] = 0; h[22] = 1; h[23] = 0;
            h[24] = SR & 0xff; h[25] = (SR >> 8) & 0xff; h[26] = (SR >> 16) & 0xff; h[27] = (SR >> 24) & 0xff;
            h[28] = (SR * 2) & 0xff; h[29] = ((SR * 2) >> 8) & 0xff; h[30] = ((SR * 2) >> 16) & 0xff; h[31] = ((SR * 2) >> 24) & 0xff;
            h[32] = 2; h[33] = 0; h[34] = 16; h[35] = 0;
            memcpy(h + 36, "data", 4);
            h[40] = data & 0xff; h[41] = (data >> 8) & 0xff; h[42] = (data >> 16) & 0xff; h[43] = (data >> 24) & 0xff;
            fseek(wav, 0, SEEK_SET);
            fwrite(h, 1, 44, wav);
        }
        fclose(csv);
        fclose(wav);
        printf("rendered %u frames (%.2f s) -> %s, %s\n", total_frames, dur, csvpath, wavpath);
    }
    return 0;
}
