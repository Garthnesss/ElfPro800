add_library(p8core STATIC hal.c hal_mock.c p8_core.c)
target_include_directories(p8core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(p8core PUBLIC c_std_99)
