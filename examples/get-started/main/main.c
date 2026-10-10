#include "driver/i2c.h"
#include "ds1307.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCL_IO_PIN CONFIG_I2C_MASTER_SCL
#define SDA_IO_PIN CONFIG_I2C_MASTER_SDA
#define MASTER_FREQUENCY CONFIG_I2C_MASTER_FREQUENCY

static const char *TAG = "app_main";

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_IO_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = SCL_IO_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .clk_stretch_tick = 300,
    };
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER));
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    return ESP_OK;
}

static void i2c_scan(void)
{
    for (int i = 0; i < 128; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            if (i == DS1307_ADDRESS) {
                ESP_LOGI(TAG, "found ds1307 address: 0x%02X", i);
            } else if (i == 0x50) {
                ESP_LOGI(TAG, "found eeprom address: 0x%02X", i);
            } else {
                ESP_LOGI(TAG, "found i2c device address: 0x%02X", i);
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Start");

    ESP_ERROR_CHECK(i2c_master_init());
    i2c_scan();

    const ds1307_config_t ds1307_config = {
        .i2c_port = I2C_NUM_0,
        .device_address = DS1307_ADDRESS,
    };
    ds1307_handle_t ds1307_handle;
    ESP_ERROR_CHECK(ds1307_init(&ds1307_config, &ds1307_handle));

    while (1) {
        ds1307_data_t data;
        ESP_ERROR_CHECK(ds1307_get_data(ds1307_handle, &data));
        uint8_t *p = (uint8_t *)&data;
        ESP_LOGI(TAG, "Get data:");
        for (int i = 0; i < (int)sizeof(data); i++) {
            printf(" %02X", p[i]);
        }
        printf("\n");

        struct tm tm;
        ESP_ERROR_CHECK(ds1307_get_datetime(ds1307_handle, &tm));
        ESP_LOGI(TAG, "Get datetime: %s", asctime(&tm));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
