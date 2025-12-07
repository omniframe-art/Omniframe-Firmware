#ifndef IMAGE_QUEUE_H
#define IMAGE_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include <esp_log.h>

#define QUEUE_SIZE 4               // Number of elements in the queue
#define MAX_FILENAME_LENGTH 128     // Maximum filename length

// NVS namespace and key
#define NVS_NAMESPACE "storage"
#define NVS_QUEUE_KEY "image_queue"

// Current queue for filenames
extern char current_queue[QUEUE_SIZE][MAX_FILENAME_LENGTH];
extern char new_queue[QUEUE_SIZE][MAX_FILENAME_LENGTH];


// Function declarations
esp_err_t get_queue_nvs(void);
esp_err_t set_queue_nvs(void);
void compare_queues(void);
void advance_queue(void);
void move_back_queue(void);

#endif // IMAGE_QUEUE_H
