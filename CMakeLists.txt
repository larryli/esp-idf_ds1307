idf_component_register(SRCS "src/ds1307.c"
                    PRIV_REQUIRES esp_driver_i2c
                    INCLUDE_DIRS "include")
