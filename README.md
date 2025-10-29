# DS1307 real-time clock(RTC) driver component for esp-idf

## Installation

    idf.py add-dependency "larryli/ds1307"

## Getting Started

### New i2c master

```c
#include "driver/i2c_master.h"

i2c_master_bus_config_t i2c_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = PORT_NUMBER,
    .scl_io_num = SCL_IO_PIN,
    .sda_io_num = SDA_IO_PIN,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t bus_handle;

i2c_new_master_bus(&i2c_bus_config, &bus_handle);
```

### Init DS1307 device

```c
#include "ds1307.h"

ds1307_config_t ds1307_config = {
    .ds1307_device.scl_speed_hz = MASTER_FREQUENCY,
    .ds1307_device.device_address = DS1307_ADDRESS,
};

ds1307_handle_t ds1307_handle;
ds1307_init(bus_handle, &ds1307_config, &ds1307_handle);
```

### Get datetime or data

```c
struct tm tm;

ds1307_get_datetime(ds1307_handle, &tm);

ds1307_data_t data; // BCD data
ds1307_get_data(ds1307_handle, &data);
```
