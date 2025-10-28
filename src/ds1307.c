#include "ds1307.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

/***
 * Addr | Bit7 | 6     | 5     | 4        | 3 | 2 | 1   | 0   | Func    | Range
 * 0x00 | CH   | 10 Seconds               | Seconds           | Seconds | 00–59
 * 0x01 | 0    | 10 Minutes               | Minutes           | Minutes | 00–59
 * 0x02 | 0    | 12/24 | 10 Hours         | Hour              | Hours   | 00-23
 *                     | AM/PM | 10 Hours | Hour              | Hours   | 01-12
 * 0x03 | 0    | 0     | 0     | 0        | 0 | Day           | Day     | 01–07
 * 0x04 | 0    | 0     | 10 Date          | Date              | Date    | 01–31
 * 0x05 | 0    | 0     | 0     | 10 Month | Month             | Month   | 01–12
 * 0x06 | 10 Year                         | Year              | Year    | 00–99
 * 0x07 | OUT  | 0     | 0     | SQWE     | 0 | 0 | RS1 | RS0 | Control | —
 * 0x08 |                                                     | RAM     | 0x00~
 * 0x3F |                                                     | 56 x 8  | ~0xFF
 ***/

/***
 * OUT | SQWE | RS1 | RS0 | SQW/OUT Output
 * -   | 1    | 0   | 0   | 1Hz
 * -   | 1    | 0   | 1   | 4.096kHz
 * -   | 1    | 1   | 0   | 8.192kHz
 * -   | 1    | 1   | 1   | 32.768kHz
 * 0   | 0    | -   | -   | 0
 * 1   | 0    | -   | -   | 1
 ***/

#define BUF_SIZE 7
#define BUF_HOUR_SIZE 3
#define SEC_REG 0
#define SEC_OFFSET 0
#define SEC_CH_BIT (1 << 7)
#define SEC_MASK 0x7f
#define MIN_OFFSET 1
#define HOUR_OFFSET 2
#define HOUR_12_BIT (1 << 6)
#define HOUR_PM_BIT (1 << 5)
#define HOUR_12_MASK 0x1f
#define DAY_OFFSET 3
#define DATE_OFFSET 4
#define MON_OFFSET 5
#define YEAR_OFFSET 6
#define CTRL_REG 7
#define CTRL_OUT_BIT (1 << 7)
#define CTRL_SQWE_BIT (1 << 4)
#define CTRL_RS_MASK 0x3
#define RAM_REG 8

static const char TAG[] = "ds1307";

static uint8_t int2bcd(uint8_t x) { return ((x / 10) << 4) + (x % 10); }

static uint8_t bcd2int(uint8_t x) { return (x >> 4) * 10 + (x & 0x0f); }

struct ds1307_t {
    i2c_master_dev_handle_t i2c_dev; /*!< I2C device handle */
    int tm_year_start;
};

esp_err_t ds1307_init(i2c_master_bus_handle_t bus_handle,
                      const ds1307_config_t *ds1307_config,
                      ds1307_handle_t *ds1307_handle)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid i2c master bus");
    ESP_RETURN_ON_FALSE(ds1307_config, ESP_ERR_INVALID_ARG, TAG,
                        "invalid ds1307 config");
    esp_err_t ret = ESP_OK;
    ds1307_handle_t out_handle =
        (ds1307_handle_t)calloc(1, sizeof(struct ds1307_t));
    ESP_GOTO_ON_FALSE(out_handle, ESP_ERR_NO_MEM, err, TAG,
                      "no memory for i2c ds1307 device");
    int century = ds1307_config->century;
    if (century == 0) {
        century = 21;
    } else if (century < 0) {
        century++;
    }
    out_handle->tm_year_start = (century - 20) * 100;

    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = ds1307_config->ds1307_device.scl_speed_hz,
        .device_address = ds1307_config->ds1307_device.device_address,
    };
    if (out_handle->i2c_dev == NULL) {
        ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(bus_handle, &i2c_dev_conf,
                                                    &out_handle->i2c_dev),
                          err, TAG, "i2c new bus failed");
    }

    *ds1307_handle = out_handle;

    return ESP_OK;

err:
    if (out_handle && out_handle->i2c_dev) {
        i2c_master_bus_rm_device(out_handle->i2c_dev);
    }
    free(out_handle);
    return ret;
}

esp_err_t ds1307_deinit(ds1307_handle_t ds1307_handle)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_ERROR(i2c_master_bus_rm_device(ds1307_handle->i2c_dev), TAG,
                        "rm i2c device failed");
    free(ds1307_handle);
    return ESP_OK;
}

static uint8_t from_12_hour(uint8_t hour_bcd)
{
    uint8_t hour = bcd2int(hour_bcd & HOUR_12_MASK);
    if (hour == 12) {
        hour = 0; // 12:xx AM = 00:xx, 12:xx PM = 12:xx
    }
    if (hour_bcd & HOUR_PM_BIT) {
        hour += 12;
    }
    return hour;
}

static uint8_t to_12_hour(uint8_t hour)
{
    uint8_t hour_pm = 0;
    if (hour >= 12) {
        hour -= 12;
        hour_pm = HOUR_PM_BIT;
    }
    if (hour == 0) {
        hour = 12; // 00:xx = 12:xx AM, 12:xx = 12:xx PM
    }
    return (int2bcd(hour) & HOUR_12_MASK) | HOUR_12_BIT | hour_pm;
}

esp_err_t ds1307_get_datetime(ds1307_handle_t ds1307_handle, struct tm *tm)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(tm, ESP_ERR_NO_MEM, TAG, "invalid datetime handle");

    uint8_t reg = SEC_REG, buf[BUF_SIZE];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), buf,
                                                    sizeof(buf), -1),
                        TAG, "i2c read failed");
    memset(tm, 0, sizeof(struct tm));
    tm->tm_sec = bcd2int(buf[SEC_OFFSET] & SEC_MASK);
    tm->tm_min = bcd2int(buf[MIN_OFFSET]);
    if (buf[HOUR_OFFSET] & HOUR_12_BIT) { // 12-Hour
        tm->tm_hour = from_12_hour(buf[HOUR_OFFSET]);
    } else { // 24-Hour
        tm->tm_hour = bcd2int(buf[HOUR_OFFSET]);
    }
    tm->tm_wday = bcd2int(buf[DAY_OFFSET]) - 1;
    tm->tm_mday = bcd2int(buf[DATE_OFFSET]);
    tm->tm_mon = bcd2int(buf[MON_OFFSET]);
    tm->tm_year = bcd2int(buf[YEAR_OFFSET]) + ds1307_handle->tm_year_start;
    return ESP_OK;
}

esp_err_t ds1307_set_datetime(ds1307_handle_t ds1307_handle,
                              const struct tm *tm)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(tm, ESP_ERR_NO_MEM, TAG, "invalid datetime handle");

    uint8_t reg = SEC_REG, buf[BUF_SIZE + 1];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), buf,
                                                    BUF_HOUR_SIZE, -1),
                        TAG, "i2c read failed");
    uint8_t ch = buf[SEC_OFFSET] & SEC_CH_BIT;
    uint8_t hour_12 = buf[HOUR_OFFSET] & HOUR_12_BIT;

    buf[0] = reg;
    buf[SEC_OFFSET + 1] = (int2bcd(tm->tm_sec) & SEC_MASK) | ch;
    buf[MIN_OFFSET + 1] = int2bcd(tm->tm_min);
    if (hour_12) { // 12-Hour
        buf[HOUR_OFFSET + 1] = to_12_hour(tm->tm_hour);
    } else { // 24-Hour
        buf[HOUR_OFFSET + 1] = int2bcd(tm->tm_hour);
    }
    buf[DAY_OFFSET + 1] = int2bcd(tm->tm_wday + 1);
    buf[DATE_OFFSET + 1] = int2bcd(tm->tm_mday);
    buf[MON_OFFSET + 1] = int2bcd(tm->tm_mon + 1);
    int year = tm->tm_year % 100;
    if (year < 0) {
        year += 100;
    }
    buf[YEAR_OFFSET + 1] = int2bcd(year);
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(ds1307_handle->i2c_dev, buf, sizeof(buf), -1), TAG,
        "i2c write failed");

    return ESP_OK;
}

esp_err_t ds1307_get_data(ds1307_handle_t ds1307_handle, ds1307_data_t *data)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_NO_MEM, TAG, "invalid data handle");

    uint8_t reg = SEC_REG, buf[BUF_SIZE];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), buf,
                                                    sizeof(buf), -1),
                        TAG, "i2c read failed");
    memset(data, 0, sizeof(ds1307_data_t));
    data->second = buf[SEC_OFFSET] & SEC_MASK;
    data->minute = buf[MIN_OFFSET];
    if (buf[HOUR_OFFSET] & HOUR_12_BIT) { // 12-Hour
        data->hour_12 = 1;
        data->hour_pm = (buf[HOUR_OFFSET] & HOUR_PM_BIT) ? 1 : 0;
        data->hour = buf[HOUR_OFFSET] & HOUR_12_MASK;
    } else { // 24-Hour
        data->hour = buf[HOUR_OFFSET];
    }
    data->day = buf[DAY_OFFSET];
    data->date = buf[DATE_OFFSET];
    data->month = buf[MON_OFFSET];
    data->year = buf[YEAR_OFFSET];
    return ESP_OK;
}

esp_err_t ds1307_set_data(ds1307_handle_t ds1307_handle,
                          const ds1307_data_t *data)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_NO_MEM, TAG, "invalid data handle");

    uint8_t reg = SEC_REG, buf[BUF_SIZE + 1];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), buf, 1,
                                                    -1),
                        TAG, "i2c read failed");
    uint8_t ch = buf[SEC_OFFSET] & SEC_CH_BIT;

    buf[0] = reg;
    buf[SEC_OFFSET + 1] = (data->second & SEC_MASK) | ch;
    buf[MIN_OFFSET + 1] = data->minute & 0x7f;
    if (data->hour_12) { // 12-Hour
        buf[HOUR_OFFSET + 1] = (data->hour & HOUR_12_MASK) | HOUR_12_BIT |
                               (data->hour_pm ? HOUR_PM_BIT : 0);
    } else { // 24-Hour
        buf[HOUR_OFFSET + 1] = data->hour & 0x3f;
    }
    buf[DAY_OFFSET + 1] = data->day & 0x07;
    buf[DATE_OFFSET + 1] = data->date & 0x3f;
    buf[MON_OFFSET + 1] = data->month & 0x1f;
    buf[YEAR_OFFSET + 1] = data->year;
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(ds1307_handle->i2c_dev, buf, sizeof(buf), -1), TAG,
        "i2c write failed");

    return ESP_OK;
}

esp_err_t ds1307_get_12_hour(ds1307_handle_t ds1307_handle, bool *mode)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(mode, ESP_ERR_NO_MEM, TAG, "invalid mode handle");

    uint8_t reg = SEC_REG + HOUR_OFFSET, value;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), &value,
                                                    sizeof(value), -1),
                        TAG, "i2c read failed");
    *mode = (value & HOUR_12_BIT) ? true : false;
    return ESP_OK;
}

esp_err_t ds1307_set_12_hour(ds1307_handle_t ds1307_handle, const bool mode)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");

    uint8_t reg = SEC_REG + HOUR_OFFSET, hour;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), &hour,
                                                    sizeof(hour), -1),
                        TAG, "i2c read failed");
    if ((hour & HOUR_12_BIT) == (mode ? HOUR_12_BIT : 0)) {
        return ESP_OK;
    }
    if (mode) { // 24-Hour ==> 12-Hour
        hour = to_12_hour(bcd2int(hour));
    } else { // 12-Hour -=> 24-Hour
        hour = int2bcd(from_12_hour(hour));
    }
    uint8_t buf[2] = {reg, hour};
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(ds1307_handle->i2c_dev, buf, sizeof(buf), -1), TAG,
        "i2c write failed");
    return ESP_OK;
}

static esp_err_t set_reg(ds1307_handle_t ds1307_handle, uint8_t reg,
                         uint8_t mask, uint8_t value)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");

    uint8_t origin;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), &origin,
                                                    sizeof(origin), -1),
                        TAG, "i2c read failed");
    if ((origin & (~mask)) == value) {
        return ESP_OK;
    }

    uint8_t buf[2] = {reg, (origin & mask) | value};
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(ds1307_handle->i2c_dev, buf, sizeof(buf), -1), TAG,
        "i2c write failed");
    return ESP_OK;
}

esp_err_t ds1307_get_halt(ds1307_handle_t ds1307_handle, bool *halt)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(halt, ESP_ERR_NO_MEM, TAG, "invalid halt handle");

    uint8_t reg = SEC_REG, value;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), &value,
                                                    sizeof(value), -1),
                        TAG, "i2c read failed");
    *halt = (value & SEC_CH_BIT) ? true : false;
    return ESP_OK;
}

esp_err_t ds1307_set_halt(ds1307_handle_t ds1307_handle, const bool halt)
{
    return set_reg(ds1307_handle, SEC_REG, (uint8_t)~SEC_CH_BIT,
                   halt ? SEC_CH_BIT : 0);
}

esp_err_t ds1307_get_output(ds1307_handle_t ds1307_handle, bool *output)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(output, ESP_ERR_NO_MEM, TAG, "invalid output handle");

    uint8_t reg = CTRL_REG, value;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), &value,
                                                    sizeof(value), -1),
                        TAG, "i2c read failed");
    *output = (value & CTRL_OUT_BIT) ? true : false;
    return ESP_OK;
}

esp_err_t ds1307_set_output(ds1307_handle_t ds1307_handle, const bool output)
{
    return set_reg(ds1307_handle, CTRL_REG, (uint8_t)~CTRL_OUT_BIT,
                   output ? CTRL_OUT_BIT : 0);
}

esp_err_t ds1307_get_square_wave_enable(ds1307_handle_t ds1307_handle,
                                        bool *enable)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(enable, ESP_ERR_NO_MEM, TAG, "invalid enable handle");

    uint8_t reg = CTRL_REG, value;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), &value,
                                                    sizeof(value), -1),
                        TAG, "i2c read failed");
    *enable = (value & CTRL_SQWE_BIT) ? true : false;
    return ESP_OK;
}

esp_err_t ds1307_set_square_wave_enable(ds1307_handle_t ds1307_handle,
                                        const bool enable)
{
    return set_reg(ds1307_handle, CTRL_REG, ~CTRL_SQWE_BIT,
                   enable ? CTRL_SQWE_BIT : 0);
}

esp_err_t ds1307_get_rate_select(ds1307_handle_t ds1307_handle,
                                 ds1307_rate_select_t *rs)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(rs, ESP_ERR_NO_MEM, TAG, "invalid rate_select handle");

    uint8_t reg = CTRL_REG, value;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &reg, sizeof(reg), &value,
                                                    sizeof(value), -1),
                        TAG, "i2c read failed");
    *rs = value & CTRL_RS_MASK;
    return ESP_OK;
}

esp_err_t ds1307_set_rate_select(ds1307_handle_t ds1307_handle,
                                 const ds1307_rate_select_t rs)
{
    return set_reg(ds1307_handle, CTRL_REG, ~CTRL_RS_MASK, rs & CTRL_RS_MASK);
}

esp_err_t ds1307_get_ram(ds1307_handle_t ds1307_handle, uint8_t offset,
                         uint8_t *data, uint8_t size)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_NO_MEM, TAG, "invalid data handle");
    ESP_RETURN_ON_FALSE(offset + size <= DS1307_RAM_SIZE, ESP_ERR_INVALID_ARG,
                        TAG, "invalid offset or size");

    offset += RAM_REG;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ds1307_handle->i2c_dev,
                                                    &offset, sizeof(offset),
                                                    data, size, -1),
                        TAG, "i2c read failed");
    return ESP_OK;
}

esp_err_t ds1307_set_ram(ds1307_handle_t ds1307_handle, uint8_t offset,
                         const uint8_t *data, uint8_t size)
{
    ESP_RETURN_ON_FALSE(ds1307_handle, ESP_ERR_NO_MEM, TAG,
                        "invalid ds1307 handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_NO_MEM, TAG, "invalid data handle");
    ESP_RETURN_ON_FALSE(offset + size <= DS1307_RAM_SIZE, ESP_ERR_INVALID_ARG,
                        TAG, "invalid offset or size");

    uint8_t buf[DS1307_RAM_SIZE + 1];
    buf[0] = offset + RAM_REG;
    memcpy(buf + 1, data, size);
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(ds1307_handle->i2c_dev, buf, size + 1, -1), TAG,
        "i2c write failed");
    return ESP_OK;
}
