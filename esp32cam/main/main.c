#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "esp_psram.h"

/* AI-Thinker ESP32-CAM / OV2640 pinout confirmed on this board. */
#define MOTION_PIXEL_THRESHOLD 18
#define MOTION_MIN_PIXELS 180
#define MOTION_MAX_PIXELS 1800

typedef struct { bool active; int x, y, area, score; const char *state; } motion_result_t;

static const char *classify_motion(const motion_result_t *m, int width) {
    if (!m->active) return "QUIET";
    if (m->score < 25) return "UNKNOWN";
    if (m->x < width / 3) return "LEFT";
    if (m->x >= (width * 2) / 3) return "RIGHT";
    return "CENTER";
}

static motion_result_t compare_frames(const uint8_t *previous, const uint8_t *current, int width, int height) {
    motion_result_t result = {0};
    int sum_x = 0, sum_y = 0;
    const int total = width * height;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        int delta = current[y * width + x] - previous[y * width + x];
        if (delta < 0) delta = -delta;
        if (delta >= MOTION_PIXEL_THRESHOLD) { result.area++; sum_x += x; sum_y += y; }
    }
    result.active = result.area >= MOTION_MIN_PIXELS;
    if (result.active) { result.x = sum_x / result.area; result.y = sum_y / result.area; }
    result.score = (result.area * 100) / (total / 10);
    if (result.score > 100) result.score = 100;
    result.state = (result.area > MOTION_MAX_PIXELS) ? "UNKNOWN" : classify_motion(&result, width);
    return result;
}

void app_main(void) {
    printf("PRIMITIVE_VISION=v0.1\n");
    printf("PSRAM_BYTES=%u\n", (unsigned)esp_psram_get_size());
    camera_config_t camera = {
        .pin_pwdn=32, .pin_reset=-1, .pin_xclk=0, .pin_sccb_sda=26, .pin_sccb_scl=27,
        .pin_d0=5, .pin_d1=18, .pin_d2=19, .pin_d3=21, .pin_d4=36, .pin_d5=39,
        .pin_d6=34, .pin_d7=35, .pin_vsync=25, .pin_href=23, .pin_pclk=22,
        .xclk_freq_hz=20000000, .ledc_timer=LEDC_TIMER_0, .ledc_channel=LEDC_CHANNEL_0,
        .pixel_format=PIXFORMAT_GRAYSCALE, .frame_size=FRAMESIZE_QQVGA,
        .jpeg_quality=12, .fb_count=1, .fb_location=CAMERA_FB_IN_PSRAM,
        .grab_mode=CAMERA_GRAB_WHEN_EMPTY,
    };
    esp_err_t error = esp_camera_init(&camera);
    printf("CAMERA_INIT=%s\n", esp_err_to_name(error));
    if (error != ESP_OK) return;
    printf("SENSOR_PID=0x%04x\n", esp_camera_sensor_get()->id.PID);
    printf("MOTION_CONFIG=width=160 height=120 pixel_threshold=%d min_pixels=%d max_pixels=%d\n",
           MOTION_PIXEL_THRESHOLD, MOTION_MIN_PIXELS, MOTION_MAX_PIXELS);
    camera_fb_t *first = esp_camera_fb_get();
    if (!first) { printf("FRAME_CAPTURE_FAILED\n"); return; }
    const size_t frame_size = first->width * first->height;
    uint8_t *previous = heap_caps_malloc(frame_size, MALLOC_CAP_8BIT);
    if (!previous) { printf("FRAME_BUFFER_ALLOC_FAILED\n"); esp_camera_fb_return(first); return; }
    memcpy(previous, first->buf, frame_size);
    const int width = first->width, height = first->height;
    int previous_luminance = 0;
    for (size_t i = 0; i < frame_size; ++i) previous_luminance += previous[i];
    previous_luminance /= (int)frame_size;
    esp_camera_fb_return(first);
    printf("FRAME_CONFIG=%dx%d\n", width, height);
    while (true) {
        camera_fb_t *current = esp_camera_fb_get();
        if (!current) { printf("FRAME_CAPTURE_FAILED\n"); vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        motion_result_t motion = compare_frames(previous, current->buf, width, height);
        int luminance = 0;
        for (size_t i = 0; i < frame_size; ++i) luminance += current->buf[i];
        luminance /= (int)frame_size;
        const int luminance_delta = luminance - previous_luminance;
        const char *light_state = (luminance_delta < -25 || luminance_delta > 25) ? "LIGHT_CHANGE" :
                                  (luminance < 60 ? "DARK" : luminance > 190 ? "BRIGHT" : "NORMAL");
        printf("MOTION state=%s x=%d y=%d area=%d score=%d threshold=%d luminance=%d light=%s\n",
               motion.state, motion.x, motion.y, motion.area, motion.score,
               MOTION_PIXEL_THRESHOLD, luminance, light_state);
        memcpy(previous, current->buf, frame_size);
        previous_luminance = luminance;
        esp_camera_fb_return(current);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
