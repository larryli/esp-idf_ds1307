# DS1307 real-time clock(RTC) driver component for ESP8266-RTOS-SDK

## Installation

For ESP8266-RTOS-SDK projects, add this component as a git submodule under your
project's `components/` directory:

```bash
cd <your-project>
git submodule add https://github.com/larryli/esp-idf_ds1307 components/ds1307
git submodule update --init --recursive
```

ESP8266-RTOS-SDK automatically discovers any component placed in `components/`,
so no further configuration is needed. Then simply `#include "ds1307.h"` in your
application.

## Getting Started

### New i2c master

```c
#include "driver/i2c.h"

i2c_config_t i2c_config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = SDA_IO_PIN,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = SCL_IO_PIN,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .clk_stretch_tick = 300,
};
i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER);
i2c_param_config(I2C_NUM_0, &i2c_config);
```

### Init DS1307 device

```c
#include "ds1307.h"

ds1307_config_t ds1307_config = {
    .i2c_port = I2C_NUM_0,
    .device_address = DS1307_ADDRESS,
};

ds1307_handle_t ds1307_handle;
ds1307_init(&ds1307_config, &ds1307_handle);
```

### Get datetime or data

```c
struct tm tm;

ds1307_get_datetime(ds1307_handle, &tm);

ds1307_data_t data; // BCD data
ds1307_get_data(ds1307_handle, &data);
```
