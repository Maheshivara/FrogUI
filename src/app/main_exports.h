#ifndef MAIN_EXPORTS_H
#define MAIN_EXPORTS_H
#include "app_common.h"
/* Defined in frogui_libretro.c; shrinks as the View refactor proceeds. */
extern retro_environment_t environ_cb;
extern retro_audio_sample_t audio_cb;
extern retro_audio_sample_batch_t audio_batch_cb;

#endif
