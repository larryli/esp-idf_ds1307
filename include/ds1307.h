#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <time.h>

#define DS1307_ADDRESS (0x68)
#define DS1307_RAM_SIZE (56)

typedef struct {
    uint8_t second; // BCD encoded seconds, same below
    uint8_t minute;
    uint8_t hour; // 1-12 for 12-hour mode, 0-23 for 24-hour mode
    uint8_t day;  // 1=Sunday, 7=Saturday
    uint8_t date;
    uint8_t month;
    uint8_t year;
    uint8_t hour_12 : 1;
    uint8_t hour_pm : 1; // 12:30 PM is 12:30 for 24-hour mode
} ds1307_data_t;

/***
 * 12:00 AM = 00:00
 * ...
 * 12:59 AM = 00:59
 *  1:00 AM = 01:00
 * ...
 * 11:59 AM = 11:59
 * 12:00 PM = 12:00
 * ...
 * 12:59 PM = 12:59
 *  1:00 PM = 13:00
 * ...
 * 11:59 PM = 23:59
 */

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
 * @brief Initialize a DS1307 device handle
 *
 * This function allocates and returns a device handle that encapsulates I2C
 * device information for subsequent operations.
 *
 * @param[in] bus_handle I2C master bus handle (i2c_master_bus_handle_t)
 * @param[in] ds1307_config Pointer to ds1307_config_t to configure device
 *                         address/speed and century handling
 * @param[out] ds1307_handle Returned device handle. The caller must call
 *                          ds1307_deinit when the handle is no longer needed.
 * @return
 *      - ESP_OK: Initialization succeeded
 *      - ESP_ERR_INVALID_ARG: Invalid argument (e.g. NULL bus_handle or
 * ds1307_config)
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *      - Other error codes returned by underlying I2C operations
 */
esp_err_t ds1307_init(i2c_master_bus_handle_t bus_handle,
                      const ds1307_config_t *ds1307_config,
                      ds1307_handle_t *ds1307_handle);

/**
 * @brief Deinitialize the DS1307 device
 *
 * Frees resources allocated by ds1307_init and removes the device from the
 * I2C bus.
 *
 * @param[in] ds1307_handle Device handle to free
 * @return
 *      - ESP_OK: Deinitialization succeeded
 *      - ESP_ERR_INVALID_ARG / ESP_ERR_NO_MEM: Invalid handle
 *      - Other error codes returned by i2c_master_bus_rm_device
 */
esp_err_t ds1307_deinit(ds1307_handle_t ds1307_handle);

/**
 * @brief Read current date and time from the DS1307 into a struct tm
 *
 * Reads seconds, minutes, hours, weekday, day, month and year registers and
 * converts them to a standard struct tm. Handles 12/24 hour conversion and
 * computes the full year based on the century configured at initialization.
 *
 * @param[in] ds1307_handle Device handle
 * @param[out] tm Pointer to struct tm to be filled (must not be NULL)
 * @return
 *      - ESP_OK: Read succeeded and tm is populated
 *      - ESP_ERR_INVALID_ARG / ESP_ERR_NO_MEM: Invalid input
 *      - Other I2C-related error codes
 */
esp_err_t ds1307_get_datetime(ds1307_handle_t ds1307_handle, struct tm *tm);

/**
 * @brief Set the DS1307 date and time from a struct tm
 *
 * Converts struct tm fields to BCD and writes them to the DS1307 registers.
 * The CH (Clock Halt) bit in the seconds register is preserved to avoid
 * unintentionally stopping/starting the clock. Note that tm->tm_mon is
 * expected as 0-11 and tm->tm_year is a full year (e.g. 2025); the
 * implementation writes the lower two digits combined with the configured
 * century.
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] tm Pointer to struct tm containing the time to set (must not be
 * NULL)
 * @return
 *      - ESP_OK: Write succeeded
 *      - ESP_ERR_INVALID_ARG / ESP_ERR_NO_MEM: Invalid input
 *      - Other I2C-related error codes
 */
esp_err_t ds1307_set_datetime(ds1307_handle_t ds1307_handle,
                              const struct tm *tm);

/**
 * @brief Read raw register-encoded time data into ds1307_data_t
 *
 * This API does not fully convert BCD values to integers. Instead it fills
 * ds1307_data_t with values as encoded in the registers (useful for direct
 * access to 12/24 hour flags, AM/PM bits, etc.).
 *
 * @param[in] ds1307_handle Device handle
 * @param[out] data Pointer to ds1307_data_t to receive the raw register data
 * (must not be NULL)
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_get_data(ds1307_handle_t ds1307_handle, ds1307_data_t *data);

/**
 * @brief Write ds1307_data_t back to the chip in raw register format
 *
 * Fields in data must follow register bit-field formats (for example,
 * data->hour in 12-hour mode should be encoded 1-12 with hour_12 set).
 * The CH bit of the seconds register is preserved when writing seconds.
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] data Pointer to ds1307_data_t to write (must not be NULL)
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_set_data(ds1307_handle_t ds1307_handle,
                          const ds1307_data_t *data);

/**
 * @brief Get whether the device is in 12-hour mode
 *
 * @param[in] ds1307_handle Device handle
 * @param[out] mode Output: true if 12-hour mode, false if 24-hour mode
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_get_12_hour(ds1307_handle_t ds1307_handle, bool *mode);

/**
 * @brief Set 12/24 hour mode
 *
 * When switching modes the function reads the current hour register and
 * converts it to preserve the hour semantics (e.g. 00:xx <-> 12:xx).
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] mode true to set 12-hour mode, false to set 24-hour mode
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_set_12_hour(ds1307_handle_t ds1307_handle, const bool mode);

/**
 * @brief Read the CH (Clock Halt) bit from the seconds register
 *
 * Indicates whether the clock is stopped.
 *
 * @param[in] ds1307_handle Device handle
 * @param[out] halt Output: true if the clock is halted, false if running
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_get_halt(ds1307_handle_t ds1307_handle, bool *halt);

/**
 * @brief Set or clear the CH (Clock Halt) bit to stop/start the clock
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] halt true to halt the clock (set CH), false to run the clock
 * (clear CH)
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_set_halt(ds1307_handle_t ds1307_handle, const bool halt);

/**
 * @brief Read the OUT bit from the control register
 *
 * When SQWE=0 the OUT bit determines the static output level (0 or 1).
 *
 * @param[in] ds1307_handle Device handle
 * @param[out] output Output: current value of the OUT bit
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_get_output(ds1307_handle_t ds1307_handle, bool *output);

/**
 * @brief Set the OUT bit in the control register (effective when SQWE=0)
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] output Value to set for OUT (true=1, false=0)
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_set_output(ds1307_handle_t ds1307_handle, const bool output);

/**
 * @brief Get the SQWE (square-wave enable) bit
 *
 * @param[in] ds1307_handle Device handle
 * @param[out] enable Output: true if square-wave output is enabled
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_get_square_wave_enable(ds1307_handle_t ds1307_handle,
                                        bool *enable);

/**
 * @brief Set the SQWE (square-wave enable) bit
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] enable true to enable square-wave output, false to disable
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_set_square_wave_enable(ds1307_handle_t ds1307_handle,
                                        const bool enable);
/**
 * @brief Get the square-wave frequency selection bits (RS1/RS0)
 *
 * @param[in] ds1307_handle Device handle
 * @param[out] rs Output: returned ds1307_rate_select_t enum value
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_get_rate_select(ds1307_handle_t ds1307_handle,
                                 ds1307_rate_select_t *rs);

/**
 * @brief Set the square-wave frequency selection bits (RS1/RS0)
 *
 * Works together with SQWE to determine the output frequency.
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] rs Frequency enum to set (see ds1307_rate_select_t)
 * @return ESP_OK on success or an I2C error code
 */
esp_err_t ds1307_set_rate_select(ds1307_handle_t ds1307_handle,
                                 const ds1307_rate_select_t rs);
/**
 * @brief Read data from DS1307 internal RAM
 *
 * The RAM region starts at register address 0x08 and has DS1307_RAM_SIZE bytes.
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] offset Offset from the start of RAM (0-based)
 * @param[out] data Output buffer with at least size bytes (must not be NULL)
 * @param[in] size Number of bytes to read
 * @return
 *      - ESP_OK: Read succeeded
 *      - ESP_ERR_INVALID_ARG: offset + size exceeds RAM size or invalid args
 *      - Other I2C-related error codes
 */
esp_err_t ds1307_get_ram(ds1307_handle_t ds1307_handle, uint8_t offset,
                         uint8_t *data, uint8_t size);

/**
 * @brief Write data to DS1307 internal RAM
 *
 * @param[in] ds1307_handle Device handle
 * @param[in] offset Offset from the start of RAM (0-based)
 * @param[in] data Data buffer to write (must not be NULL)
 * @param[in] size Number of bytes to write
 * @return
 *      - ESP_OK: Write succeeded
 *      - ESP_ERR_INVALID_ARG: offset + size exceeds RAM size or invalid args
 *      - Other I2C-related error codes
 */
esp_err_t ds1307_set_ram(ds1307_handle_t ds1307_handle, uint8_t offset,
                         const uint8_t *data, uint8_t size);
