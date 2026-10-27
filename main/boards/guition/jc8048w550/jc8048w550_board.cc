#include "application.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "JC8048W550Board"

// The stock chat UI registers no touch handler of its own, and the only
// physical switch (BOOT) shares IO0 with the speaker's bit clock. A tap is the
// only way to start talking.
class JC8048W550Display : public RgbLcdDisplay {
public:
    using RgbLcdDisplay::RgbLcdDisplay;

    void SetupUI() override {
        RgbLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);

        // LVGL delivers a click to the topmost clickable object under the
        // finger, and the chat area fills the screen, so cover the screen, the
        // container and the chat area.
        for (lv_obj_t* obj : {lv_screen_active(), container_, content_}) {
            if (obj == nullptr) {
                continue;
            }
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(obj, OnTap, LV_EVENT_CLICKED, nullptr);
        }

        // Chat bubbles are created per message after this runs; have them pass
        // their events up to the chat area instead of swallowing the tap.
        if (content_ != nullptr) {
            lv_obj_add_event_cb(
                content_,
                [](lv_event_t* e) {
                    lv_obj_t* chat = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
                    uint32_t count = lv_obj_get_child_cnt(chat);
                    if (count > 0) {
                        lv_obj_add_flag(lv_obj_get_child(chat, count - 1),
                                        LV_OBJ_FLAG_EVENT_BUBBLE);
                    }
                },
                LV_EVENT_CHILD_CREATED, content_);
        }
    }

private:
    static void OnTap(lv_event_t* e) {
        ESP_LOGI(TAG, "tap: toggling chat state");
        // ToggleChatState only sets an event bit, so the LVGL task may call it.
        Application::GetInstance().ToggleChatState();
    }
};

class JC8048W550Board : public WifiBoard {
private:
    i2c_master_bus_handle_t touch_i2c_bus_ = nullptr;
    LcdDisplay* display_ = nullptr;

    void InitializeDisplay() {
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_rgb_panel_config_t rgb_config = {};
        rgb_config.clk_src = LCD_CLK_SRC_DEFAULT;
        rgb_config.timings.pclk_hz = LCD_PCLK_HZ;
        rgb_config.timings.h_res = DISPLAY_WIDTH;
        rgb_config.timings.v_res = DISPLAY_HEIGHT;
        rgb_config.timings.hsync_pulse_width = 4;
        rgb_config.timings.hsync_back_porch = 8;
        rgb_config.timings.hsync_front_porch = 8;
        rgb_config.timings.vsync_pulse_width = 4;
        rgb_config.timings.vsync_back_porch = 8;
        rgb_config.timings.vsync_front_porch = 8;
        // The vendor demo runs hsync/vsync polarity 0, which Arduino_GFX turns
        // into idle-low sync lines.
        rgb_config.timings.flags.hsync_idle_low = true;
        rgb_config.timings.flags.vsync_idle_low = true;
        rgb_config.timings.flags.pclk_active_neg = true;
        rgb_config.data_width = 16;
        rgb_config.in_color_format = LCD_COLOR_FMT_RGB565;
        rgb_config.out_color_format = LCD_COLOR_FMT_RGB565;
        rgb_config.num_fbs = 2;
        rgb_config.bounce_buffer_size_px = DISPLAY_WIDTH * 10;
        rgb_config.dma_burst_size = 64;
        rgb_config.hsync_gpio_num = LCD_PIN_HSYNC;
        rgb_config.vsync_gpio_num = LCD_PIN_VSYNC;
        rgb_config.de_gpio_num = LCD_PIN_DE;
        rgb_config.pclk_gpio_num = LCD_PIN_PCLK;
        rgb_config.disp_gpio_num = LCD_PIN_DISP;
        rgb_config.data_gpio_nums[0] = LCD_PIN_DATA0;
        rgb_config.data_gpio_nums[1] = LCD_PIN_DATA1;
        rgb_config.data_gpio_nums[2] = LCD_PIN_DATA2;
        rgb_config.data_gpio_nums[3] = LCD_PIN_DATA3;
        rgb_config.data_gpio_nums[4] = LCD_PIN_DATA4;
        rgb_config.data_gpio_nums[5] = LCD_PIN_DATA5;
        rgb_config.data_gpio_nums[6] = LCD_PIN_DATA6;
        rgb_config.data_gpio_nums[7] = LCD_PIN_DATA7;
        rgb_config.data_gpio_nums[8] = LCD_PIN_DATA8;
        rgb_config.data_gpio_nums[9] = LCD_PIN_DATA9;
        rgb_config.data_gpio_nums[10] = LCD_PIN_DATA10;
        rgb_config.data_gpio_nums[11] = LCD_PIN_DATA11;
        rgb_config.data_gpio_nums[12] = LCD_PIN_DATA12;
        rgb_config.data_gpio_nums[13] = LCD_PIN_DATA13;
        rgb_config.data_gpio_nums[14] = LCD_PIN_DATA14;
        rgb_config.data_gpio_nums[15] = LCD_PIN_DATA15;
        rgb_config.flags.fb_in_psram = 1;

        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

        display_ = new JC8048W550Display(nullptr, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                         DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouchI2c() {
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = TOUCH_I2C_PORT;
        bus_config.sda_io_num = TOUCH_PIN_NUM_SDA;
        bus_config.scl_io_num = TOUCH_PIN_NUM_SCL;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &touch_i2c_bus_));
    }

    bool TryInitializeTouchAt(uint8_t dev_addr) {
        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.dev_addr = dev_addr;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 16;
        tp_io_config.flags.disable_control_phase = 1;
        tp_io_config.scl_speed_hz = 400000;
        if (esp_lcd_new_panel_io_i2c(touch_i2c_bus_, &tp_io_config, &tp_io_handle) != ESP_OK) {
            return false;
        }

        esp_lcd_touch_config_t tp_cfg = {};
        tp_cfg.x_max = DISPLAY_WIDTH;
        tp_cfg.y_max = DISPLAY_HEIGHT;
        tp_cfg.rst_gpio_num = TOUCH_PIN_NUM_RST;
        tp_cfg.int_gpio_num = TOUCH_PIN_NUM_INT;
        tp_cfg.levels.reset = 0;
        tp_cfg.levels.interrupt = 0;
        tp_cfg.flags.swap_xy = DISPLAY_SWAP_XY;
        tp_cfg.flags.mirror_x = DISPLAY_MIRROR_X;
        tp_cfg.flags.mirror_y = DISPLAY_MIRROR_Y;

        esp_lcd_touch_handle_t tp = nullptr;
        esp_err_t err = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "No GT911 at 0x%02X: %s", dev_addr, esp_err_to_name(err));
            esp_lcd_panel_io_del(tp_io_handle);
            return false;
        }

        lvgl_port_touch_cfg_t touch_cfg = {};
        touch_cfg.disp = lv_display_get_default();
        touch_cfg.handle = tp;
        if (touch_cfg.disp == nullptr) {
            ESP_LOGE(TAG, "LVGL display is not initialized, touch stays inactive");
            return false;
        }
        if (lvgl_port_add_touch(&touch_cfg) == nullptr) {
            ESP_LOGE(TAG, "lvgl_port_add_touch failed, touch stays inactive");
            return false;
        }

        ESP_LOGI(TAG, "GT911 ready at 0x%02X", dev_addr);
        return true;
    }

    void InitializeTouch() {
        // INT is not wired, so the chip's I2C address is not driven: it comes
        // up on 0x5D or 0x14. Try both rather than leave touch dead silently.
        if (TryInitializeTouchAt(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS)) {
            return;
        }
        if (TryInitializeTouchAt(ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP)) {
            return;
        }
        // Not fatal: the display stays usable.
        ESP_LOGE(TAG, "GT911 not found at 0x5D or 0x14, touch disabled");
    }

public:
    JC8048W550Board() {
        InitializeDisplay();
        InitializeTouchI2c();
        InitializeTouch();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
                                               AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        // The stock 25 kHz leaves this board's backlight dark; the vendor/ESPHome
        // definition runs it at about 1.2 kHz.
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                      DISPLAY_BACKLIGHT_PWM_HZ);
        return &backlight;
    }
};

DECLARE_BOARD(JC8048W550Board);
