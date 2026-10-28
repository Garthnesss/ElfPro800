/* Recording mock HAL for host tests and the render tool: targets feed
 * inputs through it and read back every frame the core produced. */
#ifndef P8_HAL_MOCK_H
#define P8_HAL_MOCK_H

#include "hal.h"

#define P8_MOCK_MAX_FRAMES 65536

void p8_mock_reset(void);
void p8_mock_set_inputs(const p8_inputs_t *inputs);
const p8_inputs_t *p8_mock_inputs(void);
void p8_mock_record(const p8_cv_frame_t *frame);
uint32_t p8_mock_frame_count(void);
const p8_cv_frame_t *p8_mock_frame(uint32_t index);

#endif /* P8_HAL_MOCK_H */
