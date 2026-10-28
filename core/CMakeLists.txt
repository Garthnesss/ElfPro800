add_library(p8core STATIC hal.c hal_mock.c p8_core.c engine_bridge.c)
target_link_libraries(p8core PUBLIC p8p600)
target_include_directories(p8core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(p8core PUBLIC c_std_99)

add_library(p8p600 STATIC
  p600fw_derived/common/adsr.c
  p600fw_derived/common/lfo.c
  p600fw_derived/common/assigner.c
  p600fw_derived/common/arp.c
  p600fw_derived/common/seq.c
  p600fw_derived/common/utils.c
  p600fw_derived/common/clock.c
  p600fw_derived/common/midi.c
  p600fw_derived/common/storage.c
  p600fw_derived/common/synth.c
  p600fw_derived/common/ui.c
  p600fw_derived/common/import.c
  p600fw_derived/common/tuner.c
  p600fw_derived/xnormidi/bytequeue/bytequeue.c
  p600fw_derived/xnormidi/midi.c
  p600fw_derived/xnormidi/midi_device.c
  p600fw_derived/xnormidi/sysex_tools.c
  p600fw_derived/compat.c
)
target_include_directories(p8p600 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/p600fw_derived/common
  ${CMAKE_CURRENT_SOURCE_DIR}/p600fw_derived/shims
)
if(MSVC)
  # cl rejects GCC-style -Wno-* flags outright (CI, 2026-10-28); the
  # imported code is warning-noisy by upstream standards, so silence it.
  # (No -fgnu89-inline needed: MSVC's C99 inline matches the clang path.)
  target_compile_options(p8p600 PRIVATE /W0)
else()
  target_compile_options(p8p600 PRIVATE -fgnu89-inline -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare)
endif()

if(NOT MSVC AND NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
  target_link_libraries(p8p600 PUBLIC m)
endif()
