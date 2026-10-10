set(REQ "")
set(PRIV_REQ esp8266 log)

idf_component_register(SRCS "src/ds1307.c"
                    INCLUDE_DIRS "include"
                    REQUIRES ${REQ}
                    PRIV_REQUIRES ${PRIV_REQ})
