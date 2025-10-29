if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(REQ "")
    set(PRIV_REQ esp_driver_i2c)
else()
    set(REQ "")
    set(PRIV_REQ driver)
endif()

idf_component_register(SRCS "src/ds1307.c"
                    INCLUDE_DIRS "include"
                    REQUIRES ${REQ}
                    PRIV_REQUIRES ${PRIV_REQ})
