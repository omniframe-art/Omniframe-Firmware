#pragma once

#include "../output_common/render_context.h"

/**
 * Lighten / darken picels using the LCD driving method.
 */
void epd_push_pixels_lcd(RenderContext_t *ctx, int color);

/**
 * Do a full update cycle with a configured context.
 */
void lcd_do_update(RenderContext_t *ctx);
void lcd_do_update_sweep(RenderContext_t *ctx);

/**
 * Worker thread for output calculation.
 * In LCD mode, both threads do the same thing.
 */
void lcd_calculate_frame(RenderContext_t *ctx, int thread_id);
void lcd_calculate_frame_sweep(RenderContext_t *ctx, int thread_id, int min_y, int max_y);

void render_stripe_frame(RenderContext_t *ctx);

