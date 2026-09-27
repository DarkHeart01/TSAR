#pragma once

// Per-build XOR key derived from compile timestamp.
// __TIME__ expands to "HH:MM:SS" — each compilation second produces a
// different key, so the encoded byte arrays in .rdata change every build.
#define JKY_XOR_KEY ((unsigned char)(__TIME__[0] ^ __TIME__[3] ^ __TIME__[6]))

// BASE_KEY is the fixed key used when the byte literals in USE_STR were
// originally generated (python encoder used 0x5A).
#define JKY_BASE_KEY ((unsigned char)(0x5A))

// XB(b): re-encode a pre-encoded byte with the per-build key.
// Stored in .rdata as: (char ^ BASE_KEY) ^ XOR_KEY
// Decoded at runtime as: stored ^ XOR_KEY ^ BASE_KEY == char
#define XB(b) ((unsigned char)((unsigned char)(b) ^ JKY_XOR_KEY))

// Decode XOR-encoded bytes onto a stack buffer.
// Decode key = JKY_XOR_KEY ^ JKY_BASE_KEY, which undoes both layers.
static inline void jky_decode(char* out, const unsigned char* enc, int len) {
    const unsigned char dk = (unsigned char)(JKY_XOR_KEY ^ JKY_BASE_KEY);
    for (int i = 0; i < len; i++)
        out[i] = (char)(enc[i] ^ dk);
    out[len] = '\0';
}

// USE_STR(varname, XB(b0), XB(b1), ...)
//   Static encoded array lives in .rdata as per-build XOR garbage.
//   char varname[] is decoded onto the stack at runtime.
#define USE_STR(varname, ...) \
    static const unsigned char varname##_enc_[] = { __VA_ARGS__, 0 }; \
    char varname[sizeof(varname##_enc_)]; \
    jky_decode(varname, varname##_enc_, (int)(sizeof(varname##_enc_) - 1));

// ZERO_STR(varname): volatile-wipe the stack buffer after use.
#define ZERO_STR(varname) \
    do { \
        volatile char* _z = (volatile char*)(varname); \
        for (int _i = 0; _i < (int)sizeof(varname); _i++) _z[_i] = 0; \
    } while (0);
