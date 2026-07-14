#include "rgb_led_driver.h"

#include <stdbool.h>

#include "driver/ledc.h"

#define LOG_TAG "rgb_led"
#include "platform_log.h"

#define RGB_LED_RED_GPIO        4
#define RGB_LED_YELLOW_GPIO     5
#define RGB_LED_GREEN_GPIO      6
#define RGB_LED_PWM_FREQUENCY_HZ 5000
#define RGB_LED_ACTIVE_LOW      false

static uint32_t rgb_led_driver_duty(uint8_t brightness)
{
    return RGB_LED_ACTIVE_LOW ? UINT8_MAX - brightness : brightness;
}

esp_err_t rgb_led_driver_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = RGB_LED_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        return err;
    }

    const int gpio_nums[] = {RGB_LED_RED_GPIO, RGB_LED_YELLOW_GPIO, RGB_LED_GREEN_GPIO};
    for (int channel = LEDC_CHANNEL_0; channel <= LEDC_CHANNEL_2; ++channel) {
        const ledc_channel_config_t channel_config = {
            .gpio_num = gpio_nums[channel],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = rgb_led_driver_duty(0),
            .hpoint = 0,
        };
        err = ledc_channel_config(&channel_config);
        if (err != ESP_OK) {
            return err;
        }
    }

    log_info("LEDC PWM initialized: R=GPIO%d Y=GPIO%d G=GPIO%d, %d Hz, active_%s",
             RGB_LED_RED_GPIO, RGB_LED_YELLOW_GPIO, RGB_LED_GREEN_GPIO,
             RGB_LED_PWM_FREQUENCY_HZ, RGB_LED_ACTIVE_LOW ? "low" : "high");
    return ESP_OK;
}

esp_err_t rgb_led_driver_set(uint8_t red, uint8_t yellow, uint8_t green)
{
    const uint8_t brightness[] = {red, yellow, green};
    for (int channel = LEDC_CHANNEL_0; channel <= LEDC_CHANNEL_2; ++channel) {
        esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel,
                                      rgb_led_driver_duty(brightness[channel]));
        if (err != ESP_OK) {
            return err;
        }
        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}
