#include "sha256_sw.h"

#include <string.h>

static const uint32_t s_round_constants[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static uint32_t rotate_right(uint32_t value, unsigned count)
{
    return (value >> count) | (value << (32U - count));
}

static void transform(sha256_sw_context_t *context, const uint8_t block[64])
{
    uint32_t words[64];
    for (size_t i = 0; i < 16U; ++i) {
        size_t offset = i * 4U;
        words[i] = ((uint32_t)block[offset] << 24) |
                   ((uint32_t)block[offset + 1U] << 16) |
                   ((uint32_t)block[offset + 2U] << 8) |
                   block[offset + 3U];
    }
    for (size_t i = 16U; i < 64U; ++i) {
        uint32_t s0 = rotate_right(words[i - 15U], 7) ^
                      rotate_right(words[i - 15U], 18) ^
                      (words[i - 15U] >> 3);
        uint32_t s1 = rotate_right(words[i - 2U], 17) ^
                      rotate_right(words[i - 2U], 19) ^
                      (words[i - 2U] >> 10);
        words[i] = words[i - 16U] + s0 + words[i - 7U] + s1;
    }

    uint32_t a = context->state[0];
    uint32_t b = context->state[1];
    uint32_t c = context->state[2];
    uint32_t d = context->state[3];
    uint32_t e = context->state[4];
    uint32_t f = context->state[5];
    uint32_t g = context->state[6];
    uint32_t h = context->state[7];
    for (size_t i = 0; i < 64U; ++i) {
        uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t temp1 = h + sum1 + choose + s_round_constants[i] + words[i];
        uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

void sha256_sw_init(sha256_sw_context_t *context)
{
    memset(context, 0, sizeof(*context));
    context->state[0] = 0x6a09e667U;
    context->state[1] = 0xbb67ae85U;
    context->state[2] = 0x3c6ef372U;
    context->state[3] = 0xa54ff53aU;
    context->state[4] = 0x510e527fU;
    context->state[5] = 0x9b05688cU;
    context->state[6] = 0x1f83d9abU;
    context->state[7] = 0x5be0cd19U;
}

void sha256_sw_update(sha256_sw_context_t *context, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        context->buffer[context->buffer_length++] = data[i];
        if (context->buffer_length == sizeof(context->buffer)) {
            transform(context, context->buffer);
            context->bit_length += 512U;
            context->buffer_length = 0U;
        }
    }
}

void sha256_sw_finish(sha256_sw_context_t *context, uint8_t digest[32])
{
    size_t index = context->buffer_length;
    context->buffer[index++] = 0x80U;
    if (index > 56U) {
        memset(context->buffer + index, 0, 64U - index);
        transform(context, context->buffer);
        index = 0U;
    }
    memset(context->buffer + index, 0, 56U - index);
    context->bit_length += context->buffer_length * 8U;
    for (size_t i = 0; i < 8U; ++i) {
        context->buffer[63U - i] = (uint8_t)(context->bit_length >> (i * 8U));
    }
    transform(context, context->buffer);

    for (size_t i = 0; i < 8U; ++i) {
        digest[i * 4U] = (uint8_t)(context->state[i] >> 24);
        digest[i * 4U + 1U] = (uint8_t)(context->state[i] >> 16);
        digest[i * 4U + 2U] = (uint8_t)(context->state[i] >> 8);
        digest[i * 4U + 3U] = (uint8_t)context->state[i];
    }
    memset(context, 0, sizeof(*context));
}

bool sha256_sw_self_test(void)
{
    static const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    static const uint8_t input[] = {'a', 'b', 'c'};
    sha256_sw_context_t context;
    uint8_t digest[32];
    sha256_sw_init(&context);
    sha256_sw_update(&context, input, sizeof(input));
    sha256_sw_finish(&context, digest);
    return memcmp(digest, expected, sizeof(expected)) == 0;
}
