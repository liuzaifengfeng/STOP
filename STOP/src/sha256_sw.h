#ifndef SHA256_SW_H
#define SHA256_SW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_length;
    uint8_t buffer[64];
    size_t buffer_length;
} sha256_sw_context_t;

void sha256_sw_init(sha256_sw_context_t *context);
void sha256_sw_update(sha256_sw_context_t *context, const uint8_t *data, size_t length);
void sha256_sw_finish(sha256_sw_context_t *context, uint8_t digest[32]);
bool sha256_sw_self_test(void);

#endif
