// =====================================================================================
//  sha256.h - self-contained SHA-256 (FIPS 180-4)
//
//  HyperSense hashes a file for the optional "Search on VirusTotal" action. Using the
//  advapi32 crypto providers for that (CryptAcquireContextW / CryptCreateHash /
//  CryptHashData) put the algorithm names and the provider calls into the import table
//  next to ReadProcessMemory - the exact combination machine-learning engines label as a
//  credential stealer. This implementation keeps the binary free of those imports; it is
//  ordinary FIPS 180-4 SHA-256, not obfuscation.
// =====================================================================================
#pragma once

#include <cstdint>
#include <cstring>

namespace hs {

struct Sha256 {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t used;
};

inline uint32_t Sha256Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

inline void Sha256Init(Sha256& c) {
    static const uint32_t iv[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };
    std::memcpy(c.h, iv, sizeof(iv));
    c.len = 0;
    c.used = 0;
}

inline void Sha256Block(Sha256& c, const unsigned char* p) {
    static const uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = Sha256Rotr(w[i - 15], 7) ^ Sha256Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = Sha256Rotr(w[i - 2], 17) ^ Sha256Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3];
    uint32_t e = c.h[4], f = c.h[5], g = c.h[6], hh = c.h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = Sha256Rotr(e, 6) ^ Sha256Rotr(e, 11) ^ Sha256Rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + s1 + ch + k[i] + w[i];
        uint32_t s0 = Sha256Rotr(a, 2) ^ Sha256Rotr(a, 13) ^ Sha256Rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = s0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }

    c.h[0] += a; c.h[1] += b; c.h[2] += cc; c.h[3] += d;
    c.h[4] += e; c.h[5] += f; c.h[6] += g; c.h[7] += hh;
}

inline void Sha256Update(Sha256& c, const void* data, size_t n) {
    const unsigned char* p = (const unsigned char*)data;
    c.len += n;
    while (n > 0) {
        size_t take = 64 - c.used;
        if (take > n) take = n;
        std::memcpy(c.buf + c.used, p, take);
        c.used += take;
        p += take;
        n -= take;
        if (c.used == 64) {
            Sha256Block(c, c.buf);
            c.used = 0;
        }
    }
}

inline void Sha256Final(Sha256& c, unsigned char out[32]) {
    uint64_t bits = c.len * 8;   // captured before padding is fed through Update
    const unsigned char one = 0x80;
    const unsigned char zero = 0x00;
    Sha256Update(c, &one, 1);
    while (c.used != 56) Sha256Update(c, &zero, 1);
    unsigned char lenBytes[8];
    for (int i = 0; i < 8; ++i) lenBytes[i] = (unsigned char)(bits >> (56 - 8 * i));
    Sha256Update(c, lenBytes, 8);
    for (int i = 0; i < 8; ++i) {
        out[i * 4] = (unsigned char)(c.h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c.h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c.h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)c.h[i];
    }
}

} // namespace hs
