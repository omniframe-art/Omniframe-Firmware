#include "render.h"

#include "epdiy.h"
#include "epd_board.h"
#include "epd_internals.h"

#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "esp_partition.h"

#include "output_common/render_context.h"
#include "output_common/render_method.h"
#include "output_common/lut.h"
#include "output_lcd/render_lcd.h"

static inline int min(int x, int y) { return x < y ? x : y; }
static inline int max(int x, int y) { return x > y ? x : y; }

const int clear_cycle_time = 12;

#define RTOS_ERROR_CHECK(x)                                                    \
    do {                                                                       \
        esp_err_t __err_rc = (x);                                              \
        if (__err_rc != pdPASS) {                                              \
            abort();                                                           \
        }                                                                      \
    } while (0)

static RenderContext_t render_context;

void epd_push_pixels(EpdRect area, int color) {
    render_context.area = area;
    epd_push_pixels_lcd(&render_context, color);
}

///////////////////////////// Coordination ///////////////////////////////

/**
 * Find the waveform temperature range index for a given temperature in °C.
 * If no range in the waveform data fits the given temperature, return the
 * closest one.
 * Returns -1 if the waveform does not contain any temperature range.
 */
int waveform_temp_range_index(const EpdWaveform *waveform, int temperature) {
    int idx = 0;
    if (waveform->num_temp_ranges == 0) {
        return -1;
    }
    while (idx < waveform->num_temp_ranges - 1 &&
           waveform->temp_intervals[idx].min < temperature) {
        idx++;
    }
    return idx;
}

static int get_waveform_index(const EpdWaveform *waveform,
                              enum EpdDrawMode mode) {
    for (int i = 0; i < waveform->num_modes; i++) {
        if (waveform->mode_data[i]->type == (mode & 0x3F)) {
            return i;
        }
    }
    return -1;
}

/////////////////////////////  API Procedures //////////////////////////////////

/// Rounded up display height for even division into multi-line buffers.
static inline int rounded_display_height() {
    return (((epd_height() + 7) / 8) * 8);
}

// FIXME: fix misleading naming:
//  area -> buffer dimensions
//  crop -> area taken out of buffer
enum EpdDrawError IRAM_ATTR epd_draw_base(
    EpdRect area, const uint8_t *data, EpdRect crop_to, enum EpdDrawMode mode,
    int temperature, const bool *drawn_lines, const EpdWaveform *waveform) {
    
    int waveform_range = 1;
    int waveform_index = 0;
    uint8_t frame_count = FRAMES;
    const EpdWaveformPhases *waveform_phases = NULL;

    load_waveform();

    /*    // no waveform required for monochrome mode
    if (!(mode & MODE_EPDIY_MONOCHROME)) {
        waveform_index = get_waveform_index(waveform, mode);
        if (waveform_index < 0) {
            return EPD_DRAW_MODE_NOT_FOUND;
        }

        waveform_phases =
            waveform->mode_data[waveform_index]->range_data[waveform_range];
        // FIXME: error if not present
        frame_count = waveform_phases->phases;
    } else {
        frame_count = 1;
    }

    if (crop_to.width < 0 || crop_to.height < 0) {
        return EPD_DRAW_INVALID_CROP;
    }

    const bool crop = (crop_to.width > 0 && crop_to.height > 0);
    if (crop && (crop_to.width > area.width || crop_to.height > area.height ||
                 crop_to.x > area.width || crop_to.y > area.height)) {
        return EPD_DRAW_INVALID_CROP;
    }
    */

    render_context.area = area;
    render_context.crop_to = crop_to;
    render_context.waveform_range = waveform_range;
    render_context.waveform_index = waveform_index;
    render_context.mode = mode;
    render_context.waveform = waveform;
    render_context.error = EPD_DRAW_SUCCESS;
    render_context.drawn_lines = drawn_lines;
    render_context.data_ptr = data;

    render_context.lines_prepared = 0;
    render_context.lines_consumed = 0;
    render_context.lines_total = 1200;
    render_context.current_frame = 0;
    render_context.cycle_frames = frame_count;
    render_context.phase_times = NULL;
    if (waveform_phases != NULL && waveform_phases->phase_times != NULL) {
        render_context.phase_times = waveform_phases->phase_times;
    }

    //ESP_LOGI("epdiy", "starting update, phases: %d", frame_count);

    lcd_do_update(&render_context);
    //lcd_do_update_sweep(&render_context);


    if (render_context.error != EPD_DRAW_SUCCESS) {
        return render_context.error;
    }
    return EPD_DRAW_SUCCESS;
}


static void IRAM_ATTR render_thread(void* arg) {
    int thread_id = (int)arg;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        lcd_calculate_frame(&render_context, thread_id);

        xSemaphoreGive(render_context.feed_done_smphr[thread_id]);
    }
}



void epd_clear_area_cycles(EpdRect area, int cycles) {
    cycles = 1;
    for (int c = 0; c < cycles; c++) {
        for (int i = 0; i < 10; i++) {
            epd_push_pixels(area, 0);
        }
        for (int i = 0; i < 10; i++) {
            epd_push_pixels(area, 1);
        }
        for (int i = 0; i < 5; i++) {
            epd_push_pixels(area, 2);
        }
    }
}

void epd_renderer_init() {

    render_context.display_width = 1600;
    render_context.display_height = 1200;

    render_context.frame_done = xSemaphoreCreateBinary();

    for (int i = 0; i < NUM_RENDER_THREADS; i++) {
        render_context.feed_done_smphr[i] = xSemaphoreCreateBinary();
    }

    // When using the LCD peripheral, we may need padding lines to
    // satisfy the bounce buffer size requirements
    render_context.line_threads = (uint8_t *)heap_caps_malloc(
        1200, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);


    int queue_len = 150;

    //size_t queue_elem_size = 400;

    for (int i = 0; i < NUM_RENDER_THREADS; i++) {
/*
        render_context.line_queues[i].size = queue_len;
        render_context.line_queues[i].element_size = queue_elem_size;
        render_context.line_queues[i].current = 0;
        render_context.line_queues[i].last = 0;
       // render_context.line_queues[i].buf = (uint8_t *)heap_caps_malloc(
         //   queue_len * queue_elem_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
       render_context.line_queues[i].buf = (uint8_t *)heap_caps_aligned_alloc(
    16, // Alignment in bytes (32-byte alignment for DMA)
    queue_len * queue_elem_size,
    MALLOC_CAP_INTERNAL
);
*/

        render_context.line_queues[i] = lq_init(queue_len);
        
        render_context.feed_line_buffers[i] = (uint8_t *)heap_caps_malloc(render_context.display_width, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        assert(render_context.feed_line_buffers[i] != NULL);
        RTOS_ERROR_CHECK(xTaskCreatePinnedToCore(
            render_thread, "epd_prep", 1 << 11, (void *)i,
            configMAX_PRIORITIES-1, &render_context.feed_tasks[i], i));
       
       // if (render_context.line_queues[i].buf == NULL) {
         //   ESP_LOGE("epd", "could not allocate line queue!");
           // abort();
        //}

    
    }
}


void epd_renderer_deinit() {
    const EpdBoardDefinition* epd_board = epd_current_board();

    epd_board->poweroff(epd_ctrl_state());

    for (int i = 0; i < NUM_RENDER_THREADS; i++) {
        vTaskDelete(render_context.feed_tasks[i]);
        lq_free(&render_context.line_queues[i]);
        heap_caps_free(render_context.feed_line_buffers[i]);
        vSemaphoreDelete(render_context.feed_done_smphr[i]);
    }

    epd_control_reg_deinit();

    if (epd_board->deinit) {
        epd_board->deinit();
    }

    heap_caps_free(render_context.conversion_lut);
    heap_caps_free(render_context.line_threads);
   // heap_caps_free(render_context.line_mask);
    vSemaphoreDelete(render_context.frame_done);
    /*
    const EpdBoardDefinition* epd_board = epd_current_board();

    epd_board->poweroff(epd_ctrl_state());

    epd_control_reg_deinit();

    if (epd_board->deinit) {
        epd_board->deinit();
    }

    for (int i = 0; i < NUM_RENDER_THREADS; i++) {
        free(render_context.line_queues[i].buf);
        free(render_context.feed_line_buffers[i]);
        vSemaphoreDelete(render_context.feed_done_smphr[i]);
        vTaskDelete(render_context.feed_tasks[i]);
    }

    free(render_context.conversion_lut);
    free(render_context.line_threads);
    vSemaphoreDelete(render_context.frame_done);
    */
}

EpdRect epd_difference_image_base(const uint8_t *to, const uint8_t *from,
                                  EpdRect crop_to, int fb_width, int fb_height,
                                  uint8_t *interlaced, bool *dirty_lines,
                                  uint8_t *from_or, uint8_t *from_and) {
    assert(from_or != NULL);
    assert(from_and != NULL);
    // OR over all pixels of the "from"-image
    *from_or = 0x00;
    // AND over all pixels of the "from"-image
    *from_and = 0x0F;

    uint8_t* dirty_cols = calloc(epd_width(), 1);
    assert (dirty_cols != NULL);

    int x_end = min(fb_width, crop_to.x + crop_to.width);
    int y_end = min(fb_height, crop_to.y + crop_to.height);

    for (int y = crop_to.y; y < y_end; y++) {
        uint8_t dirty = 0;
        for (int x = crop_to.x; x < x_end; x++) {
            uint8_t t = *(to + y * fb_width / 2 + x / 2);
            t = (x % 2) ? (t >> 4) : (t & 0x0f);
            uint8_t f = *(from + y * fb_width / 2 + x / 2);
            f = (x % 2) ? (f >> 4) : (f & 0x0f);
            *from_or |= f;
            *from_and &= f;
            dirty |= (t ^ f);
            dirty_cols[x] |= (t ^ f);
            interlaced[y * fb_width + x] = (t << 4) | f;
        }
        dirty_lines[y] = dirty > 0;
    }
    int min_x, min_y, max_x, max_y;
    for (min_x = crop_to.x; min_x < x_end; min_x++) {
        if (dirty_cols[min_x] != 0)
            break;
    }
    for (max_x = x_end - 1; max_x >= crop_to.x; max_x--) {
        if (dirty_cols[max_x] != 0)
            break;
    }
    for (min_y = crop_to.y; min_y < y_end; min_y++) {
        if (dirty_lines[min_y] != 0)
            break;
    }
    for (max_y = y_end - 1; max_y >= crop_to.y; max_y--) {
        if (dirty_lines[max_y] != 0)
            break;
    }
    EpdRect crop_rect = {
        .x = min_x,
        .y = min_y,
        .width = max(max_x - min_x + 1, 0),
        .height = max(max_y - min_y + 1, 0),
    };

    free(dirty_cols);
    return crop_rect;
    
}

EpdRect epd_difference_image(const uint8_t *to, const uint8_t *from,
                             uint8_t *interlaced, bool *dirty_lines) {
    uint8_t from_or = 0;
    uint8_t from_and = 0;
    return epd_difference_image_base(to, from, epd_full_screen(), epd_width(),
                                     epd_height(), interlaced, dirty_lines,
                                     &from_or, &from_and);
}

EpdRect epd_difference_image_cropped(const uint8_t *to, const uint8_t *from,
                                     EpdRect crop_to, uint8_t *interlaced,
                                     bool *dirty_lines, bool *previously_white,
                                     bool *previously_black) {

    uint8_t from_or, from_and;

    EpdRect result =
        epd_difference_image_base(to, from, crop_to, epd_width(), epd_height(),
                                  interlaced, dirty_lines, &from_or, &from_and);

    if (previously_white != NULL)
        *previously_white = (from_and == 0x0F);
    if (previously_black != NULL)
        *previously_black = (from_or == 0x00);
    return result;
}

void render_stripes(){
    render_stripe_frame(&render_context);
}


