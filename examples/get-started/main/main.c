#include "driver/i2c_master.h"
#include "ds1307.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCL_IO_PIN CONFIG_I2C_MASTER_SCL
#define SDA_IO_PIN CONFIG_I2C_MASTER_SDA
#define MASTER_FREQUENCY CONFIG_I2C_MASTER_FREQUENCY
#define PORT_NUMBER -1

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "Start");

    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = PORT_NUMBER,
        .scl_io_num = SCL_IO_PIN,
        .sda_io_num = SDA_IO_PIN,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    for (int i = 0; i < 128; i++) {
        esp_err_t ret = i2c_master_probe(bus_handle, i, 50);
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

    const ds1307_config_t ds1307_config = {
        .ds1307_device.device_address = DS1307_ADDRESS,
        .ds1307_device.scl_speed_hz = MASTER_FREQUENCY,
    };
    ds1307_handle_t ds1307_handle;
    ESP_ERROR_CHECK(ds1307_init(bus_handle, &ds1307_config, &ds1307_handle));

    while (1) {
        ds1307_data_t data;
        ESP_ERROR_CHECK(ds1307_get_data(ds1307_handle, &data));
        ESP_LOG_BUFFER_HEX(TAG, &data, sizeof(data));

        struct tm tm;
        ESP_ERROR_CHECK(ds1307_get_datetime(ds1307_handle, &tm));
        ESP_LOGI(TAG, "Get datetime: %s", asctime(&tm));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
