#include "text.h"
#include "../../components/epdiy/src/epd_highlevel.h"
#include "../../components/epdiy/src/epdiy.h"
#include "wifi.h"
#include "config.h"
#include "image_data.h"
#include "esp_log.h" 
#include <string.h>


// Each element is just a pointer to the first element of each letter array
const uint8_t* const letter_arrays[TOTAL_CHARACTERS] = {
    // Letters A-Z
    ['A' - 'A'] = A,
    ['B' - 'A'] = B,
    ['C' - 'A'] = C,
    ['D' - 'A'] = D,
    ['E' - 'A'] = E,
    ['F' - 'A'] = F,
    ['G' - 'A'] = G,
    ['H' - 'A'] = H,
    ['I' - 'A'] = I,
    ['J' - 'A'] = J,
    ['K' - 'A'] = K,
    ['L' - 'A'] = L,
    ['M' - 'A'] = M,
    ['N' - 'A'] = N,
    ['O' - 'A'] = O,
    ['P' - 'A'] = P,
    ['Q' - 'A'] = Q,
    ['R' - 'A'] = R,
    ['S' - 'A'] = S,
    ['T' - 'A'] = T,
    ['U' - 'A'] = U,
    ['V' - 'A'] = V,
    ['W' - 'A'] = W,
    ['X' - 'A'] = X,
    ['Y' - 'A'] = Y,
    ['Z' - 'A'] = Z
    // Numbers will be added here
};

void draw_letter(uint8_t* framebuffer, char letter, int x, int y) {
    static const char *TAG = "draw_letter";

    // Determine index based on character type
    int array_index;
    if (letter >= 'A' && letter <= 'Z') {
        array_index = letter - 'A';
    } else if (letter >= '0' && letter <= '9') {
        array_index = NUM_LETTERS + (letter - '0');
    } else {
        ESP_LOGW(TAG, "Unsupported character: '%c' (0x%02x)", letter, letter);
        return;
    }

    uint8_t letter_width = LETTER_WIDTHS[array_index];
    const uint8_t* letter_data = letter_arrays[array_index];

    if (!letter_data) {
        ESP_LOGE(TAG, "No letter array defined for character '%c'", letter);
        return;
    }

    ESP_LOGI(TAG, "Drawing character '%c' (width: %d) at position (%d, %d)", 
             letter, letter_width, x, y);

    // Draw the letter pixel by pixel
    for (int row = 0; row < LETTER_SIZE; row++) {
        for (int col = 0; col < letter_width; col++) {
            int pos = (y + row) * 800 + (x + col);
            if (pos >= 0 && pos < (1600 / 2 * 1200)) {
                framebuffer[pos] = letter_data[row * letter_width + col];
            } else {
                ESP_LOGW(TAG, "Position out of bounds: (%d, %d) -> %d", x + col, y + row, pos);
                return;
            }
        }
    }
}

void draw_white_rectangle(uint8_t* framebuffer, int x, int y, int width, int height) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            framebuffer[(y + i) * 800 + (x + j)] = 0xFF;
        }
    }
}



void render_text(void){
    board_poweron(&ctrl_state);
   // clear();
    enum EpdDrawError _err = epd_hl_update_screen(&hl_state, MODE_GC16, 25);
    if (_err != EPD_DRAW_SUCCESS) {
        ESP_LOGE("render_text", "Failed to update screen: %d", _err);
    }
    board_poweroff(&ctrl_state);
}

void display_text(char* text) {
    disable_wifi();
    renderer_init();
    
    // Calculate total width based on individual letter widths
    int text_width = 0;
    for(int i = 0; i < strlen(text); i++) {
        int index;
        if (text[i] >= 'A' && text[i] <= 'Z') {
            index = text[i] - 'A';
        } else if (text[i] >= '0' && text[i] <= '9') {
            index = NUM_LETTERS + (text[i] - '0');
        } else {
            continue;  // Skip unsupported characters
        }
        text_width += LETTER_WIDTHS[index] + 1;
    }

    int rect_width = text_width + 30;
    int rect_height = 60;
    int rect_x = (800 - rect_width) / 2;
    int rect_y = (1200 - rect_height) / 2;

    int text_x = rect_x + 15;
    int text_y = rect_y + 20;
    
    uint8_t* framebuffer = epd_hl_get_framebuffer(&hl_state);
    draw_white_rectangle(framebuffer, rect_x, rect_y, rect_width, rect_height);
    
    // Draw each letter with proper spacing based on letter widths
    int current_x = text_x;
    for(int i = 0; i < strlen(text); i++) {
        if (text[i] == ' ') {
            current_x += 5;  // Add 5 pixels of space for space character
            continue;
        }
        
        draw_letter(framebuffer, text[i], current_x, text_y);
        
        // Move x position by the width of the current letter
        int index;
        if (text[i] >= 'A' && text[i] <= 'Z') {
            index = text[i] - 'A';
        } else if (text[i] >= '0' && text[i] <= '9') {
            index = NUM_LETTERS + (text[i] - '0');
        } else {
            continue;
        }
        current_x += LETTER_WIDTHS[index] + 1;
    }

    render_text();
   enable_wifi();
}