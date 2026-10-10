#pragma once

#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/***
 * The ESP8266 RTOS SDK (and other IDF versions prior to 5.x) does not ship
 * esp_check.h, so the convenience ESP_RETURN_ON_* / ESP_GOTO_ON_* macros are
 * provided here for compatibility. Each macro is guarded so it is a no-op when
 * the upstream SDK already defines it.
 */
#ifndef ESP_RETURN_ON_ERROR
#define ESP_RETURN_ON_ERROR(x, log_tag, format, ...)                          \
    do {                                                                     \
        esp_err_t __err_rc = (x);                                            \
        if (__err_rc != ESP_OK) {                                            \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                         \
            return __err_rc;                                                 \
        }                                                                    \
    } while (0)
#endif

#ifndef ESP_GOTO_ON_ERROR
#define ESP_GOTO_ON_ERROR(x, goto_tag, log_tag, format, ...)                  \
    do {                                                                     \
        esp_err_t __err_rc = (x);                                            \
        if (__err_rc != ESP_OK) {                                            \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                         \
            ret = __err_rc;                                                  \
            goto goto_tag;                                                   \
        }                                                                    \
    } while (0)
#endif

#ifndef ESP_RETURN_ON_FALSE
#define ESP_RETURN_ON_FALSE(x, err_code, log_tag, format, ...)                \
    do {                                                                     \
        if (!(x)) {                                                          \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                         \
            return err_code;                                                 \
        }                                                                    \
    } while (0)
#endif

#ifndef ESP_GOTO_ON_FALSE
#define ESP_GOTO_ON_FALSE(x, err_code, goto_tag, log_tag, format, ...)        \
    do {                                                                     \
        if (!(x)) {                                                          \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__,      \
                     ##__VA_ARGS__);                                         \
            ret = err_code;                                                  \
            goto goto_tag;                                                   \
        }                                                                    \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif
