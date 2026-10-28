/* ElfChords P8 core entry points. The engine glue (P600fw-derived) is
 * attached in engine_bridge.c; this API is the only one targets use. */
#ifndef P8_CORE_H
#define P8_CORE_H

#include "hal.h"

typedef struct {
    uint32_t tick;
} p8_core_t;

void p8_core_init(p8_core_t *core);
void p8_core_tick(p8_core_t *core, const p8_inputs_t *inputs, p8_cv_frame_t *frame);

#endif /* P8_CORE_H */
