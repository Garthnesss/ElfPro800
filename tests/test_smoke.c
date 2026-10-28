#include "p8_core.h"
#include "hal_mock.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    p8_core_t core;
    p8_inputs_t in;
    p8_cv_frame_t frame;
    int i;

    memset(&in, 0, sizeof(in));
    p8_mock_reset();
    p8_core_init(&core);
    for (i = 0; i < 100; i++) {
        p8_core_tick(&core, &in, &frame);
        p8_mock_record(&frame);
    }
    assert(p8_mock_frame_count() == 100);
    assert(strcmp(p8_mock_frame(0)->display, "P8  ") == 0);
    assert(core.tick == 100);
    return 0;
}
