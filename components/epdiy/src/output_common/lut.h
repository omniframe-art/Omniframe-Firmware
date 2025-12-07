#pragma once

#include "esp_attr.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "epdiy.h"
#include "render_context.h"


// Make a block of 4 pixels lighter on the EPD.
#define CLEAR_BYTE 0B10101010
// Make a block of 4 pixels darker on the EPD.
#define DARK_BYTE 0B01010101

#define SHADES 16
#define FRAMES 30
#define WAVEFORM_SIZE (SHADES * FRAMES)  // Assuming SHADES and FRAMES are defined

extern uint8_t custom_wave[SHADES][FRAMES];

///////////////////////////// Utils /////////////////////////////////////

/*
 * Reorder the output buffer to account for I2S FIFO order.
 */

typedef struct {
  int thread_id;
  const uint8_t *data_ptr;
  EpdRect crop_to;
  void (*done_cb)(void);
  SemaphoreHandle_t start_smphr;
  EpdRect area;
  int frame;
  /// index of the waveform mode when using vendor waveforms.
  /// This is not necessarily the mode number if the waveform header
  //only contains a selection of modes!
  int waveform_index;
  /// waveform range when using vendor waveforms
  int waveform_range;
  /// Draw time for the current frame in 1/10ths of us.
  int frame_time;
  const EpdWaveform* waveform;
  enum EpdDrawMode mode;
  enum EpdDrawError error;
  const bool *drawn_lines;
  // Queue of input data lines
  QueueHandle_t* pixel_queue;
  // Queue of display data lines
  QueueHandle_t* display_queue;

  // Lookup table size.
  size_t conversion_lut_size;
  // Lookup table space.
  uint8_t* conversion_lut;
} OutputParams;




void custom_lut_func(
    const uint32_t *ld,
    uint8_t *epd_input,
    uint8_t* lut,
    uint8_t frame
);

void get_frame_operations(RenderContext_t *ctx);

void save_waveform();
void load_waveform();

void calculate_lut(RenderContext_t *ctx);

