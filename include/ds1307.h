#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <time.h>

#define DS1307_ADDRESS (0x68)
#define DS1307_RAM_SIZE (56)

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
    uint8_t hour_12 : 1;
    uint8_t hour_pm : 1;
} ds1307_data_t;

typedef enum {
    DS1307_RATE_SELECT_1HZ = 0,
    DS1307_RATE_SELECT_4096HZ,
    DS1307_RATE_SELECT_8192HZ,
    DS1307_RATE_SELECT_32768HZ,
} ds1307_rate_select_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_device_config_t ds1307_device; /*!< Configuration for ds1307 device */
    int century;                       /*!< Century 21 is 20xx */
} ds1307_config_t;

typedef struct ds1307_t *ds1307_handle_t;

/**
 * @brief Init an DS1307 device.
 *
 * @param[in] bus_handle I2C master bus handle
 * @param[in] ds1307_config Configuration of DS1307
 * @param[out] ds1307_handle Handle of DS1307
 * @return ESP_OK: Init success. ESP_FAIL: Not success.
 */
esp_err_t ds1307_init(i2c_master_bus_handle_t bus_handle,
                      const ds1307_config_t *ds1307_config,
                      ds1307_handle_t *ds1307_handle);
esp_err_t ds1307_deinit(ds1307_handle_t ds1307_handle);

esp_err_t ds1307_get_datetime(ds1307_handle_t ds1307_handle, struct tm *tm);
esp_err_t ds1307_set_datetime(ds1307_handle_t ds1307_handle,
                              const struct tm *tm);

esp_err_t ds1307_get_data(ds1307_handle_t ds1307_handle, ds1307_data_t *data);
esp_err_t ds1307_set_data(ds1307_handle_t ds1307_handle,
                          const ds1307_data_t *data);

esp_err_t ds1307_get_12_hour(ds1307_handle_t ds1307_handle, bool *mode);
esp_err_t ds1307_set_12_hour(ds1307_handle_t ds1307_handle, const bool mode);

esp_err_t ds1307_get_halt(ds1307_handle_t ds1307_handle, bool *halt);
esp_err_t ds1307_set_halt(ds1307_handle_t ds1307_handle, const bool halt);
esp_err_t ds1307_get_output(ds1307_handle_t ds1307_handle, bool *output);
esp_err_t ds1307_set_output(ds1307_handle_t ds1307_handle, const bool output);
esp_err_t ds1307_get_square_wave_enable(ds1307_handle_t ds1307_handle,
                                        bool *enable);
esp_err_t ds1307_set_square_wave_enable(ds1307_handle_t ds1307_handle,
                                        const bool enable);
esp_err_t ds1307_get_rate_select(ds1307_handle_t ds1307_handle,
                                 ds1307_rate_select_t *rs);
esp_err_t ds1307_set_rate_select(ds1307_handle_t ds1307_handle,
                                 const ds1307_rate_select_t rs);
esp_err_t ds1307_get_ram(ds1307_handle_t ds1307_handle, uint8_t offset,
                         uint8_t *data, uint8_t size);
esp_err_t ds1307_set_ram(ds1307_handle_t ds1307_handle, uint8_t offset,
                         const uint8_t *data, uint8_t size);
