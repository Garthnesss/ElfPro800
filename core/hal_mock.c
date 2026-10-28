#include "hal_mock.h"
#include <string.h>

static p8_inputs_t g_inputs;
static p8_cv_frame_t g_frames[P8_MOCK_MAX_FRAMES];
static uint32_t g_count;

void p8_mock_reset(void)
{
    memset(&g_inputs, 0, sizeof(g_inputs));
    g_count = 0;
}

void p8_mock_set_inputs(const p8_inputs_t *inputs) { g_inputs = *inputs; }
const p8_inputs_t *p8_mock_inputs(void) { return &g_inputs; }

void p8_mock_record(const p8_cv_frame_t *frame)
{
    if (g_count < P8_MOCK_MAX_FRAMES)
        g_frames[g_count++] = *frame;
}

uint32_t p8_mock_frame_count(void) { return g_count; }
const p8_cv_frame_t *p8_mock_frame(uint32_t index)
{
    return index < g_count ? &g_frames[index] : NULL;
}
