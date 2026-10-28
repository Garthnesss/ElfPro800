#include "hal.h"
#include <string.h>

void p8_frame_clear(p8_cv_frame_t *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->display[0] = ' ';
    frame->display[1] = ' ';
    frame->display[2] = ' ';
    frame->display[3] = ' ';
    frame->display[4] = '\0';
}
