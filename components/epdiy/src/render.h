#pragma once

#include "epdiy.h"
/**
 * Initialize the EPD renderer and its render context.
 */
void epd_renderer_init();

/**
 * Deinitialize the EPD renderer and free up its resources.
 */
void epd_renderer_deinit();

void render_stripes();

void render_image_custom();

void epd_clear_area_cycles(EpdRect area, int cycles);
