/**
 * High-level API implementation for epdiy.
 */

#include "epd_highlevel.h"
#include "epdiy.h"
#include <assert.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

#ifndef _swap_int
#define _swap_int(a, b)                                                        \
  {                                                                            \
    int t = a;                                                                 \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

static bool already_initialized = 0;

EpdiyHighlevelState epd_hl_init(const EpdWaveform* waveform) {
  assert(!already_initialized);

  int fb_size = 1600 / 2 * 1200;

  #if !(defined(CONFIG_ESP32_SPIRAM_SUPPORT) || defined(CONFIG_ESP32S3_SPIRAM_SUPPORT))
    ESP_LOGW("EPDiy", "Please enable PSRAM for the ESP32 (menuconfig→ Component config→ ESP32-specific)");
  #endif
  EpdiyHighlevelState state;

  state.front_fb = heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
  assert(state.front_fb != NULL);

  state.waveform = waveform;

  memset(state.front_fb, 0xFF, fb_size);
  already_initialized = true;
  return state;

}


uint8_t* epd_hl_get_framebuffer(EpdiyHighlevelState* state) {
  assert(state != NULL);
  return state->front_fb;
}

enum EpdDrawError epd_hl_update_screen(EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature) {
  return epd_hl_update_area(state, mode, temperature, epd_full_screen());
}

enum EpdDrawError epd_hl_update_area(EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature, EpdRect area) {
  assert(state != NULL);
  
  uint32_t t1 = esp_timer_get_time() / 1000;
  enum EpdDrawError err;

  err = epd_draw_base(epd_full_screen(), state->front_fb, area, MODE_PACKING_2PPB | PREVIOUSLY_WHITE | mode, temperature, state->dirty_lines, state->waveform);

  uint32_t t2 = esp_timer_get_time() / 1000;
  printf("actual draw took %dms.\n", t2 - t1);

  return err;
}




