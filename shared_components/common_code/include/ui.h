#ifndef UI_H
#define UI_H

#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_disp_t *disp;

    lv_obj_t *screen;
    lv_obj_t *label_title;

    lv_obj_t *label_min;
    lv_obj_t *label_sec;

    lv_obj_t *dropdown_min;
    lv_obj_t *dropdown_sec;

    lv_obj_t *start_btn;
    lv_obj_t *stop_btn;

    lv_obj_t *start_label;
    lv_obj_t *stop_label;
} ui_context_t;

/*
 * High-level setup helpers
 */
esp_err_t ui_init(ui_context_t *ui);
esp_err_t ui_display_init(ui_context_t *ui);
esp_err_t ui_touch_init(ui_context_t *ui);
esp_err_t ui_create(ui_context_t *ui);

/*
 * Utility helpers
 */
esp_err_t ui_i2c_scan(void);
uint16_t ui_get_selected_minutes(const ui_context_t *ui);
uint16_t ui_get_selected_seconds(const ui_context_t *ui);
void ui_set_start_callback(ui_context_t *ui, lv_event_cb_t cb, void *user_data);
void ui_set_stop_callback(ui_context_t *ui, lv_event_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif // UI_H