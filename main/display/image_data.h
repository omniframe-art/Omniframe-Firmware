#ifndef IMAGE_DATA_H
#define IMAGE_DATA_H

#include <stdbool.h>
#include <stdint.h>
#include "../components/epdiy/src/epd_highlevel.h"
#include "../components/epdiy/src/render.h"


#define IMAGE_PARTITION_SUBTYPE 0x82

extern EpdRect full_area;

bool get_from_flash(uint8_t* framebuffer, size_t framebuffer_size, uint32_t index);
bool write_to_flash(const char* filename, const uint8_t* data, uint32_t index);
void display_image(char* filename);
void clear();
void display_next_image();

#endif // IMAGE_DATA_H