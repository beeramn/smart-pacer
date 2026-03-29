#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_gt911.h"

#include "esp32s3_box_lcd_config.h"

/**********************
 * Constants / Macros
 **********************/
static const char *TAG = "PACE_UI";

#define I2C_MASTER_NUM       I2C_NUM_1
#define I2C_MASTER_SDA_IO    8
#define I2C_MASTER_SCL_IO    18
#define I2C_MASTER_FREQ_HZ   100000

/**********************
 * File-scope state
 **********************/
static esp_lcd_touch_handle_t s_tp_handle = NULL;

/**********************
 * Forward Declarations
 **********************/
static const lv_font_t *get_ui_font_20(void);
static const lv_font_t *get_label_small_font(void);

static void default_start_btn_event_cb(lv_event_t *e);
static void default_stop_btn_event_cb(lv_event_t *e);

static esp_err_t ui_i2c_master_init(void);

/**********************
 * Public API
 **********************/

esp_err_t ui_init(ui_context_t *ui)
{
    if (ui == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(ui, 0, sizeof(*ui));

    esp_err_t err = ui_display_init(ui);
    if (err != ESP_OK) {
        return err;
    }

    err = ui_touch_init(ui);
    if (err != ESP_OK) {
        return err;
    }

    err = ui_create(ui);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t ui_display_init(ui_context_t *ui)
{
    if (ui == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t bus_config = {
        .sclk_io_num = EXAMPLE_PIN_NUM_SCLK,
        .mosi_io_num = EXAMPLE_PIN_NUM_MOSI,
        .miso_io_num = EXAMPLE_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle)
    );

    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .flags.reset_active_high = 1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle)
    );
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .flags = {
            .swap_bytes = true,
        },
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
    };

    ui->disp = lvgl_port_add_disp(&disp_cfg);
    if (ui->disp == NULL) {
        ESP_LOGE(TAG, "Failed to add LVGL display");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ui_touch_init(ui_context_t *ui)
{
    if (ui == NULL || ui->disp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing touch");
    ESP_ERROR_CHECK(ui_i2c_master_init());

    // Probe GT911 at 0x5D, then 0x14
    esp_err_t probe = ESP_FAIL;
    uint8_t addrs[] = {0x5D, 0x14};

    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addrs[i] << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        probe = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (probe == ESP_OK) {
            ESP_LOGI(TAG, "GT911 found at 0x%02X", addrs[i]);
            break;
        }
    }

    if (probe != ESP_OK) {
        ESP_LOGE(TAG, "GT911 not found at 0x5D or 0x14");
        return ESP_FAIL;
    }

    // Optional reset pulse on GPIO17
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_17),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&rst_conf));
    gpio_set_level(GPIO_NUM_17, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GPIO_NUM_17, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 0;  // required for legacy i2c driver


    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_i2c(
            (esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM,
            &tp_io_config,
            &tp_io_handle
        )
    );

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ESP_ERROR_CHECK(
        esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &s_tp_handle)
    );

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = ui->disp,
        .handle = s_tp_handle,
    };

    lvgl_port_lock(0);
    lvgl_port_add_touch(&touch_cfg);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "Touch registered with LVGL");
    return ESP_OK;
}

esp_err_t ui_create(ui_context_t *ui)
{
    if (ui == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const lv_font_t *readable_font = get_ui_font_20();
    const lv_font_t *small_lbl_font = get_label_small_font();

    lvgl_port_lock(0);

    ui->screen = lv_scr_act();

    ui->label_title = lv_label_create(ui->screen);
    lv_label_set_text(ui->label_title, "Desired Pace:");
    lv_obj_set_style_text_font(ui->label_title, readable_font, 0);
    lv_obj_align(ui->label_title, LV_ALIGN_TOP_MID, 0, 20);

    ui->label_min = lv_label_create(ui->screen);
    lv_label_set_text(ui->label_min, "Min");
    lv_obj_set_style_text_font(ui->label_min, small_lbl_font, 0);
    lv_obj_align(ui->label_min, LV_ALIGN_TOP_MID, -70, 52);

    ui->dropdown_min = lv_dropdown_create(ui->screen);
    lv_dropdown_set_options(ui->dropdown_min,
                            "6\n"
                            "7\n"
                            "8\n"
                            "9\n"
                            "10\n"
                            "11\n"
                            "12");
    lv_obj_set_width(ui->dropdown_min, 90);
    lv_obj_align(ui->dropdown_min, LV_ALIGN_TOP_MID, -70, 78);

    ui->label_sec = lv_label_create(ui->screen);
    lv_label_set_text(ui->label_sec, "Sec");
    lv_obj_set_style_text_font(ui->label_sec, small_lbl_font, 0);
    lv_obj_align(ui->label_sec, LV_ALIGN_TOP_MID, 70, 52);

    ui->dropdown_sec = lv_dropdown_create(ui->screen);
    lv_dropdown_set_options(ui->dropdown_sec, "0\n15\n30\n45");
    lv_obj_set_width(ui->dropdown_sec, 90);
    lv_obj_align(ui->dropdown_sec, LV_ALIGN_TOP_MID, 70, 78);

    ui->start_btn = lv_btn_create(ui->screen);
    lv_obj_set_size(ui->start_btn, 120, 60);
    lv_obj_align(ui->start_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(ui->start_btn, lv_color_hex(0x339B33), 0);

    ui->start_label = lv_label_create(ui->start_btn);
    lv_label_set_text(ui->start_label, "Start");
    lv_obj_set_style_text_font(ui->start_label, readable_font, 0);
    lv_obj_center(ui->start_label);
    lv_obj_add_event_cb(ui->start_btn, default_start_btn_event_cb, LV_EVENT_CLICKED, NULL);

    ui->stop_btn = lv_btn_create(ui->screen);
    lv_obj_set_size(ui->stop_btn, 120, 60);
    lv_obj_align(ui->stop_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(ui->stop_btn, lv_color_hex(0xFF0000), 0);

    ui->stop_label = lv_label_create(ui->stop_btn);
    lv_label_set_text(ui->stop_label, "Stop");
    lv_obj_set_style_text_font(ui->stop_label, readable_font, 0);
    lv_obj_center(ui->stop_label);
    lv_obj_add_event_cb(ui->stop_btn, default_stop_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lvgl_port_unlock();

    return ESP_OK;
}

esp_err_t ui_i2c_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");

    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at 0x%02X", addr);
        }
    }

    return ESP_OK;
}

uint16_t ui_get_selected_minutes(const ui_context_t *ui)
{
    if (ui == NULL || ui->dropdown_min == NULL) {
        return 0;
    }

    char buf[8] = {0};
    lv_dropdown_get_selected_str(ui->dropdown_min, buf, sizeof(buf));
    return (uint16_t)atoi(buf);
}

uint16_t ui_get_selected_seconds(const ui_context_t *ui)
{
    if (ui == NULL || ui->dropdown_sec == NULL) {
        return 0;
    }

    char buf[8] = {0};
    lv_dropdown_get_selected_str(ui->dropdown_sec, buf, sizeof(buf));
    return (uint16_t)atoi(buf);
}

void ui_set_start_callback(ui_context_t *ui, lv_event_cb_t cb, void *user_data)
{
    if (ui == NULL || ui->start_btn == NULL || cb == NULL) {
        return;
    }

    lv_obj_remove_event_cb(ui->start_btn, default_start_btn_event_cb);
    lv_obj_add_event_cb(ui->start_btn, cb, LV_EVENT_CLICKED, user_data);
}

void ui_set_stop_callback(ui_context_t *ui, lv_event_cb_t cb, void *user_data)
{
    if (ui == NULL || ui->stop_btn == NULL || cb == NULL) {
        return;
    }

    lv_obj_remove_event_cb(ui->stop_btn, default_stop_btn_event_cb);
    lv_obj_add_event_cb(ui->stop_btn, cb, LV_EVENT_CLICKED, user_data);
}

/**********************
 * Private Helpers
 **********************/

static esp_err_t ui_i2c_master_init(void)
{
    static bool initialized = false;

    if (initialized) {
        return ESP_OK;
    }

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0));

    initialized = true;
    return ESP_OK;
}

static void default_start_btn_event_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Start pressed");
}

static void default_stop_btn_event_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Stop pressed");
}

static const lv_font_t *get_ui_font_20(void)
{
#if LV_FONT_MONTSERRAT_20
    return &lv_font_montserrat_20;
#else
    return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t *get_label_small_font(void)
{
#if LV_FONT_MONTSERRAT_14
    return &lv_font_montserrat_14;
#elif LV_FONT_MONTSERRAT_12
    return &lv_font_montserrat_12;
#else
    return LV_FONT_DEFAULT;
#endif
}