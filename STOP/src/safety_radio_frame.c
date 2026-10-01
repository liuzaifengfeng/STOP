#include "safety_radio_frame.h"

#include <string.h>
#include "sha256_sw.h"

static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8U * i));
}

bool safety_radio_stop_ack_matches(const safety_radio_frame_t *frame,
                                   const uint8_t peer[6], const uint8_t self[6],
                                   uint32_t boot_id, uint32_t sequence)
{
    return frame && peer && self && frame->type == SAFETY_RADIO_STOP_ACK &&
           frame->state <= SAFETY_RADIO_ACK_VERIFIED_OFF &&
           memcmp(frame->source, peer, 6) == 0 &&
           memcmp(frame->destination, self, 6) == 0 &&
           frame->boot_id == boot_id && frame->sequence == sequence;
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void mac8(const uint8_t key[SAFETY_RADIO_KEY_LEN],
                 const uint8_t *data, size_t len, uint8_t out[8])
{
    uint8_t pad[64], digest[32];
    sha256_sw_context_t hash;
    memset(pad, 0x36, sizeof(pad));
    for (unsigned i = 0; i < SAFETY_RADIO_KEY_LEN; ++i) pad[i] ^= key[i];
    sha256_sw_init(&hash);
    sha256_sw_update(&hash, pad, sizeof(pad));
    sha256_sw_update(&hash, data, len);
    sha256_sw_finish(&hash, digest);
    memset(pad, 0x5c, sizeof(pad));
    for (unsigned i = 0; i < SAFETY_RADIO_KEY_LEN; ++i) pad[i] ^= key[i];
    sha256_sw_init(&hash);
    sha256_sw_update(&hash, pad, sizeof(pad));
    sha256_sw_update(&hash, digest, sizeof(digest));
    sha256_sw_finish(&hash, digest);
    memcpy(out, digest, 8);
    memset(digest, 0, sizeof(digest));
}

size_t safety_radio_encode(const safety_radio_frame_t *frame,
                           const uint8_t key[SAFETY_RADIO_KEY_LEN], uint8_t *out,
                           size_t capacity)
{
    if (!frame || !out) return 0;
    size_t len = frame->type == SAFETY_RADIO_PAIR_ACCEPT ? SAFETY_RADIO_PAIR_LEN
                                                          : SAFETY_RADIO_BASE_LEN;
    bool authenticated = frame->type >= SAFETY_RADIO_HEARTBEAT;
    if (capacity < len || (authenticated && !key) ||
        frame->type < SAFETY_RADIO_PAIR_REQUEST ||
        frame->type > SAFETY_RADIO_STOP_ACK ||
        (frame->type == SAFETY_RADIO_STOP_REQUEST && frame->state != 0) ||
        (frame->type == SAFETY_RADIO_STOP_ACK && frame->state > SAFETY_RADIO_ACK_VERIFIED_OFF))
        return 0;
    out[0] = 'S'; out[1] = 'R'; out[2] = SAFETY_RADIO_VERSION;
    out[3] = frame->type;
    memcpy(out + 4, frame->source, 6);
    memcpy(out + 10, frame->destination, 6);
    put32(out + 16, frame->boot_id);
    put32(out + 20, frame->sequence);
    out[24] = frame->state;
    if (frame->type == SAFETY_RADIO_PAIR_ACCEPT) memcpy(out + 25, frame->key, 16);
    if (authenticated) mac8(key, out, len - 8, out + len - 8);
    else memset(out + len - 8, 0, 8);
    return len;
}

bool safety_radio_decode(const uint8_t *data, size_t len,
                         const uint8_t key[SAFETY_RADIO_KEY_LEN],
                         safety_radio_frame_t *frame)
{
    if (!data || !frame || len < SAFETY_RADIO_BASE_LEN ||
        data[0] != 'S' || data[1] != 'R' || data[2] != SAFETY_RADIO_VERSION ||
        data[3] < SAFETY_RADIO_PAIR_REQUEST || data[3] > SAFETY_RADIO_STOP_ACK ||
        (data[3] == SAFETY_RADIO_STOP_REQUEST && data[24] != 0) ||
        (data[3] == SAFETY_RADIO_STOP_ACK && data[24] > SAFETY_RADIO_ACK_VERIFIED_OFF) ||
        len != (data[3] == SAFETY_RADIO_PAIR_ACCEPT ? SAFETY_RADIO_PAIR_LEN
                                                   : SAFETY_RADIO_BASE_LEN)) return false;
    if (data[3] >= SAFETY_RADIO_HEARTBEAT) {
        if (!key) return false;
        uint8_t expected[8];
        mac8(key, data, len - 8, expected);
        uint8_t mismatch = 0;
        for (unsigned i = 0; i < 8; ++i) mismatch |= expected[i] ^ data[len - 8 + i];
        if (mismatch) return false;
    } else {
        for (unsigned i = 0; i < 8; ++i) if (data[len - 8 + i] != 0) return false;
    }
    memset(frame, 0, sizeof(*frame));
    frame->type = data[3];
    memcpy(frame->source, data + 4, 6);
    memcpy(frame->destination, data + 10, 6);
    frame->boot_id = get32(data + 16);
    frame->sequence = get32(data + 20);
    frame->state = data[24];
    if (frame->type == SAFETY_RADIO_PAIR_ACCEPT) memcpy(frame->key, data + 25, 16);
    return true;
}
