#pragma once
// オンラインプレイ用の暗号プリミティブ (外部ライブラリに依存しない最小実装)。
//   - X25519            : 鍵交換 (RFC 7748)
//   - SHA-256 / HMAC    : 鍵導出 (HKDF, RFC 5869)。外部ツールのダウンロードの照合にも使う
//   - ChaCha20-Poly1305 : 認証付き暗号 (RFC 8439)。改ざんされたデータは復号時に検出して破棄する
//   - OS の乱数生成器    : 秘密鍵・ノンス・ルームの秘密情報
// いずれも RFC のテストベクタで動作を確認している。変更した場合は再確認すること。

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <cstdio>
#endif

namespace fasnet::crypto {

    using Key32 = std::array<uint8_t, 32>;

    // 最適化で消されないゼロクリア (鍵や受信データを使い終えた後に呼ぶ)
    inline void SecureZero(void* data, size_t size) {
        volatile uint8_t* p = static_cast<volatile uint8_t*>(data);
        while (size--) { *p++ = 0; }
    }
    template<class T>
    void SecureZero(std::vector<T>& v) {
        if (!v.empty()) { SecureZero(v.data(), v.size() * sizeof(T)); }
        v.clear();
        v.shrink_to_fit();
    }

    // 定数時間の比較 (認証タグの比較で時間差から情報が漏れないように)
    inline bool ConstantTimeEqual(const uint8_t* a, const uint8_t* b, size_t size) {
        uint8_t diff = 0;
        for (size_t i = 0; i < size; ++i) { diff |= a[i] ^ b[i]; }
        return diff == 0;
    }

    inline bool RandomBytes(uint8_t* out, size_t size) {
#ifdef _WIN32
        return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, out, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
#else
        FILE* f = std::fopen("/dev/urandom", "rb");
        if (f == nullptr) { return false; }
        const bool ok = std::fread(out, 1, size, f) == size;
        std::fclose(f);
        return ok;
#endif
    }

    inline uint32_t LoadLE32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    inline void StoreLE32(uint8_t* p, uint32_t v) {
        p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
    }
    inline void StoreLE64(uint8_t* p, uint64_t v) {
        for (int i = 0; i < 8; ++i) { p[i] = (uint8_t)(v >> (8 * i)); }
    }

    // ------------------------------------------------------------------ SHA-256

    class Sha256 {
    public:
        static constexpr size_t DigestSize = 32;
        static constexpr size_t BlockSize = 64;

        Sha256() { Reset(); }
        void Reset() {
            static constexpr uint32_t init[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
            std::memcpy(m_State, init, sizeof(m_State));
            m_Length = 0;
            m_BufferSize = 0;
        }
        void Update(const uint8_t* data, size_t size) {
            m_Length += size;
            while (size > 0) {
                const size_t n = std::min(size, BlockSize - m_BufferSize);
                std::memcpy(m_Buffer + m_BufferSize, data, n);
                m_BufferSize += n;
                data += n;
                size -= n;
                if (m_BufferSize == BlockSize) {
                    Transform(m_Buffer);
                    m_BufferSize = 0;
                }
            }
        }
        void Update(std::span<const uint8_t> data) { Update(data.data(), data.size()); }
        void Update(std::string_view data) { Update(reinterpret_cast<const uint8_t*>(data.data()), data.size()); }

        std::array<uint8_t, DigestSize> Final() {
            const uint64_t bits = m_Length * 8;
            const uint8_t pad = 0x80;
            Update(&pad, 1);
            const uint8_t zero = 0;
            while (m_BufferSize != 56) { Update(&zero, 1); }
            uint8_t len[8];
            for (int i = 0; i < 8; ++i) { len[i] = (uint8_t)(bits >> (56 - 8 * i)); }
            Update(len, 8);
            std::array<uint8_t, DigestSize> out{};
            for (int i = 0; i < 8; ++i) {
                out[4 * i] = (uint8_t)(m_State[i] >> 24);
                out[4 * i + 1] = (uint8_t)(m_State[i] >> 16);
                out[4 * i + 2] = (uint8_t)(m_State[i] >> 8);
                out[4 * i + 3] = (uint8_t)m_State[i];
            }
            SecureZero(m_Buffer, sizeof(m_Buffer));
            return out;
        }

        static std::array<uint8_t, DigestSize> Hash(std::span<const uint8_t> data) {
            Sha256 h;
            h.Update(data);
            return h.Final();
        }

    private:
        static uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

        void Transform(const uint8_t* block) {
            static constexpr uint32_t k[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
            };
            uint32_t w[64];
            for (int i = 0; i < 16; ++i) {
                w[i] = ((uint32_t)block[4 * i] << 24) | ((uint32_t)block[4 * i + 1] << 16) | ((uint32_t)block[4 * i + 2] << 8) | block[4 * i + 3];
            }
            for (int i = 16; i < 64; ++i) {
                const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = m_State[0], b = m_State[1], c = m_State[2], d = m_State[3];
            uint32_t e = m_State[4], f = m_State[5], g = m_State[6], h = m_State[7];
            for (int i = 0; i < 64; ++i) {
                const uint32_t t1 = h + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
                const uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
                h = g; g = f; f = e; e = d + t1;
                d = c; c = b; b = a; a = t1 + t2;
            }
            m_State[0] += a; m_State[1] += b; m_State[2] += c; m_State[3] += d;
            m_State[4] += e; m_State[5] += f; m_State[6] += g; m_State[7] += h;
        }

        uint32_t m_State[8]{};
        uint64_t m_Length = 0;
        uint8_t m_Buffer[BlockSize]{};
        size_t m_BufferSize = 0;
    };

    inline std::array<uint8_t, 32> HmacSha256(std::span<const uint8_t> key, std::span<const uint8_t> data) {
        uint8_t k[Sha256::BlockSize]{};
        if (key.size() > Sha256::BlockSize) {
            const auto hk = Sha256::Hash(key);
            std::memcpy(k, hk.data(), hk.size());
        }
        else {
            std::memcpy(k, key.data(), key.size());
        }
        uint8_t ipad[Sha256::BlockSize], opad[Sha256::BlockSize];
        for (size_t i = 0; i < Sha256::BlockSize; ++i) {
            ipad[i] = k[i] ^ 0x36;
            opad[i] = k[i] ^ 0x5c;
        }
        Sha256 inner;
        inner.Update(ipad, sizeof(ipad));
        inner.Update(data);
        const auto ih = inner.Final();
        Sha256 outer;
        outer.Update(opad, sizeof(opad));
        outer.Update(ih);
        SecureZero(k, sizeof(k));
        SecureZero(ipad, sizeof(ipad));
        SecureZero(opad, sizeof(opad));
        return outer.Final();
    }

    // HKDF-SHA256 (RFC 5869)
    inline std::vector<uint8_t> Hkdf(std::span<const uint8_t> salt, std::span<const uint8_t> ikm, std::span<const uint8_t> info, size_t length) {
        const auto prk = HmacSha256(salt, ikm);
        std::vector<uint8_t> okm;
        std::vector<uint8_t> block;
        std::array<uint8_t, 32> t{};
        size_t tsize = 0;
        for (uint8_t counter = 1; okm.size() < length; ++counter) {
            block.assign(t.begin(), t.begin() + tsize);
            block.insert(block.end(), info.begin(), info.end());
            block.push_back(counter);
            t = HmacSha256(prk, block);
            tsize = t.size();
            okm.insert(okm.end(), t.begin(), t.begin() + std::min(t.size(), length - okm.size()));
        }
        SecureZero(block);
        SecureZero(t.data(), t.size());
        return okm;
    }

    // ------------------------------------------------------------------ X25519 (RFC 7748)

    namespace detail {
        using gf = int64_t[16];

        inline void car25519(int64_t* o) {
            for (int i = 0; i < 16; ++i) {
                o[i] += (int64_t)1 << 16;
                const int64_t c = o[i] >> 16;
                o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
                o[i] -= c * ((int64_t)1 << 16);
            }
        }
        inline void sel25519(int64_t* p, int64_t* q, int64_t b) {
            const int64_t c = ~(b - 1);
            for (int i = 0; i < 16; ++i) {
                const int64_t t = c & (p[i] ^ q[i]);
                p[i] ^= t;
                q[i] ^= t;
            }
        }
        inline void pack25519(uint8_t* o, const int64_t* n) {
            int64_t m[16], t[16];
            for (int i = 0; i < 16; ++i) { t[i] = n[i]; }
            car25519(t); car25519(t); car25519(t);
            for (int j = 0; j < 2; ++j) {
                m[0] = t[0] - 0xffed;
                for (int i = 1; i < 15; ++i) {
                    m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
                    m[i - 1] &= 0xffff;
                }
                m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
                const int64_t b = (m[15] >> 16) & 1;
                m[14] &= 0xffff;
                sel25519(t, m, 1 - b);
            }
            for (int i = 0; i < 16; ++i) {
                o[2 * i] = (uint8_t)(t[i] & 0xff);
                o[2 * i + 1] = (uint8_t)(t[i] >> 8);
            }
        }
        inline void unpack25519(int64_t* o, const uint8_t* n) {
            for (int i = 0; i < 16; ++i) { o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8); }
            o[15] &= 0x7fff;
        }
        inline void A(int64_t* o, const int64_t* a, const int64_t* b) { for (int i = 0; i < 16; ++i) { o[i] = a[i] + b[i]; } }
        inline void Z(int64_t* o, const int64_t* a, const int64_t* b) { for (int i = 0; i < 16; ++i) { o[i] = a[i] - b[i]; } }
        inline void M(int64_t* o, const int64_t* a, const int64_t* b) {
            int64_t t[31] = {};
            for (int i = 0; i < 16; ++i) {
                for (int j = 0; j < 16; ++j) { t[i + j] += a[i] * b[j]; }
            }
            for (int i = 0; i < 15; ++i) { t[i] += 38 * t[i + 16]; }
            for (int i = 0; i < 16; ++i) { o[i] = t[i]; }
            car25519(o);
            car25519(o);
        }
        inline void S(int64_t* o, const int64_t* a) { M(o, a, a); }
        inline void inv25519(int64_t* o, const int64_t* in) {
            int64_t c[16];
            for (int i = 0; i < 16; ++i) { c[i] = in[i]; }
            for (int a = 253; a >= 0; --a) {
                S(c, c);
                if (a != 2 && a != 4) { M(c, c, in); }
            }
            for (int i = 0; i < 16; ++i) { o[i] = c[i]; }
        }
    }

    inline Key32 X25519(const Key32& scalar, const Key32& point) {
        using namespace detail;
        static constexpr int64_t k121665[16] = { 0xDB41, 1 };
        uint8_t z[32];
        int64_t x[80], a[16], b[16], c[16], d[16], e[16], f[16];
        for (int i = 0; i < 31; ++i) { z[i] = scalar[i]; }
        z[31] = (scalar[31] & 127) | 64;
        z[0] &= 248;
        unpack25519(x, point.data());
        for (int i = 0; i < 16; ++i) {
            b[i] = x[i];
            d[i] = a[i] = c[i] = 0;
        }
        a[0] = d[0] = 1;
        for (int i = 254; i >= 0; --i) {
            const int64_t r = (z[i >> 3] >> (i & 7)) & 1;
            sel25519(a, b, r);
            sel25519(c, d, r);
            A(e, a, c); Z(a, a, c); A(c, b, d); Z(b, b, d);
            S(d, e); S(f, a); M(a, c, a); M(c, b, e);
            A(e, a, c); Z(a, a, c); S(b, a); Z(c, d, f);
            M(a, c, k121665); A(a, a, d); M(c, c, a); M(a, d, f);
            M(d, b, x); S(b, e);
            sel25519(a, b, r);
            sel25519(c, d, r);
        }
        for (int i = 0; i < 16; ++i) {
            x[i + 16] = a[i];
            x[i + 32] = c[i];
            x[i + 48] = b[i];
            x[i + 64] = d[i];
        }
        inv25519(x + 32, x + 32);
        M(x + 16, x + 16, x + 32);
        Key32 out{};
        pack25519(out.data(), x + 16);
        SecureZero(z, sizeof(z));
        return out;
    }
    inline Key32 X25519Base(const Key32& scalar) {
        Key32 base{};
        base[0] = 9;
        return X25519(scalar, base);
    }

    // ------------------------------------------------------------------ ChaCha20-Poly1305 (RFC 8439)

    namespace detail {
        inline uint32_t Rotl(uint32_t v, int c) { return (v << c) | (v >> (32 - c)); }

        inline void ChaChaBlock(const uint32_t key[8], uint32_t counter, const uint32_t nonce[3], uint8_t out[64]) {
            uint32_t s[16] = {
                0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
                key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7],
                counter, nonce[0], nonce[1], nonce[2],
            };
            uint32_t x[16];
            std::memcpy(x, s, sizeof(x));
            auto QR = [&](int a, int b, int c, int d) {
                x[a] += x[b]; x[d] ^= x[a]; x[d] = Rotl(x[d], 16);
                x[c] += x[d]; x[b] ^= x[c]; x[b] = Rotl(x[b], 12);
                x[a] += x[b]; x[d] ^= x[a]; x[d] = Rotl(x[d], 8);
                x[c] += x[d]; x[b] ^= x[c]; x[b] = Rotl(x[b], 7);
            };
            for (int i = 0; i < 10; ++i) {
                QR(0, 4, 8, 12); QR(1, 5, 9, 13); QR(2, 6, 10, 14); QR(3, 7, 11, 15);
                QR(0, 5, 10, 15); QR(1, 6, 11, 12); QR(2, 7, 8, 13); QR(3, 4, 9, 14);
            }
            for (int i = 0; i < 16; ++i) { StoreLE32(out + 4 * i, x[i] + s[i]); }
        }

        class Poly1305 {
        public:
            explicit Poly1305(const uint8_t key[32]) {
                r[0] = LoadLE32(key + 0) & 0x3ffffff;
                r[1] = (LoadLE32(key + 3) >> 2) & 0x3ffff03;
                r[2] = (LoadLE32(key + 6) >> 4) & 0x3ffc0ff;
                r[3] = (LoadLE32(key + 9) >> 6) & 0x3f03fff;
                r[4] = (LoadLE32(key + 12) >> 8) & 0x00fffff;
                for (int i = 0; i < 4; ++i) { pad[i] = LoadLE32(key + 16 + 4 * i); }
            }
            ~Poly1305() {
                SecureZero(r, sizeof(r));
                SecureZero(pad, sizeof(pad));
                SecureZero(h, sizeof(h));
                SecureZero(buffer, sizeof(buffer));
            }
            void Update(const uint8_t* m, size_t bytes) {
                if (leftover) {
                    const size_t want = std::min<size_t>(16 - leftover, bytes);
                    std::memcpy(buffer + leftover, m, want);
                    bytes -= want;
                    m += want;
                    leftover += want;
                    if (leftover < 16) { return; }
                    Blocks(buffer, 16, false);
                    leftover = 0;
                }
                if (bytes >= 16) {
                    const size_t want = bytes & ~(size_t)15;
                    Blocks(m, want, false);
                    m += want;
                    bytes -= want;
                }
                if (bytes) {
                    std::memcpy(buffer + leftover, m, bytes);
                    leftover += bytes;
                }
            }
            void Final(uint8_t mac[16]) {
                if (leftover) {
                    buffer[leftover] = 1;
                    for (size_t i = leftover + 1; i < 16; ++i) { buffer[i] = 0; }
                    Blocks(buffer, 16, true);
                }
                uint32_t h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4];
                uint32_t c = h1 >> 26; h1 &= 0x3ffffff;
                h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
                h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
                h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
                h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
                h1 += c;

                uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
                uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
                uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
                uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
                uint32_t g4 = h4 + c - (1u << 26);

                uint32_t mask = (g4 >> 31) - 1;
                g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
                mask = ~mask;
                h0 = (h0 & mask) | g0;
                h1 = (h1 & mask) | g1;
                h2 = (h2 & mask) | g2;
                h3 = (h3 & mask) | g3;
                h4 = (h4 & mask) | g4;

                h0 = (h0 | (h1 << 26));
                h1 = ((h1 >> 6) | (h2 << 20));
                h2 = ((h2 >> 12) | (h3 << 14));
                h3 = ((h3 >> 18) | (h4 << 8));

                uint64_t f = (uint64_t)h0 + pad[0]; h0 = (uint32_t)f;
                f = (uint64_t)h1 + pad[1] + (f >> 32); h1 = (uint32_t)f;
                f = (uint64_t)h2 + pad[2] + (f >> 32); h2 = (uint32_t)f;
                f = (uint64_t)h3 + pad[3] + (f >> 32); h3 = (uint32_t)f;

                StoreLE32(mac + 0, h0);
                StoreLE32(mac + 4, h1);
                StoreLE32(mac + 8, h2);
                StoreLE32(mac + 12, h3);
            }

        private:
            void Blocks(const uint8_t* m, size_t bytes, bool final) {
                const uint32_t hibit = final ? 0 : (1u << 24);
                const uint32_t r0 = r[0], r1 = r[1], r2 = r[2], r3 = r[3], r4 = r[4];
                const uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
                uint32_t h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4];
                while (bytes >= 16) {
                    h0 += LoadLE32(m + 0) & 0x3ffffff;
                    h1 += (LoadLE32(m + 3) >> 2) & 0x3ffffff;
                    h2 += (LoadLE32(m + 6) >> 4) & 0x3ffffff;
                    h3 += (LoadLE32(m + 9) >> 6) & 0x3ffffff;
                    h4 += (LoadLE32(m + 12) >> 8) | hibit;

                    const uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
                    uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
                    uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
                    uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
                    uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

                    uint32_t c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
                    d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
                    d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
                    d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
                    d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
                    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
                    h1 += c;

                    m += 16;
                    bytes -= 16;
                }
                h[0] = h0; h[1] = h1; h[2] = h2; h[3] = h3; h[4] = h4;
            }

            uint32_t r[5]{};
            uint32_t h[5]{};
            uint32_t pad[4]{};
            uint8_t buffer[16]{};
            size_t leftover = 0;
        };
    }

    inline constexpr size_t AeadTagSize = 16;

    // 認証付き暗号の鍵 (送信・受信それぞれに持つ)
    class Aead {
    public:
        Aead() = default;
        explicit Aead(const Key32& key) { SetKey(key); }
        ~Aead() { SecureZero(m_Key, sizeof(m_Key)); }

        void SetKey(const Key32& key) {
            for (int i = 0; i < 8; ++i) { m_Key[i] = LoadLE32(key.data() + 4 * i); }
        }

        // data をその場で暗号化し、認証タグを tag に書き込む
        void Seal(const uint8_t nonce[12], std::span<const uint8_t> aad, std::span<uint8_t> data, uint8_t tag[AeadTagSize]) const {
            uint32_t n[3];
            LoadNonce(nonce, n);
            Xor(n, data);
            ComputeTag(n, aad, data, tag);
        }
        // タグを検証してから復号する。改ざんされていれば false を返し、data は変更しない
        bool Open(const uint8_t nonce[12], std::span<const uint8_t> aad, std::span<uint8_t> data, const uint8_t tag[AeadTagSize]) const {
            uint32_t n[3];
            LoadNonce(nonce, n);
            uint8_t expected[AeadTagSize];
            ComputeTag(n, aad, data, expected);
            if (!ConstantTimeEqual(expected, tag, AeadTagSize)) {
                return false;
            }
            Xor(n, data);
            return true;
        }

    private:
        static void LoadNonce(const uint8_t nonce[12], uint32_t out[3]) {
            for (int i = 0; i < 3; ++i) { out[i] = LoadLE32(nonce + 4 * i); }
        }
        void Xor(const uint32_t nonce[3], std::span<uint8_t> data) const {
            uint8_t block[64];
            uint32_t counter = 1;
            for (size_t offset = 0; offset < data.size(); offset += 64, ++counter) {
                detail::ChaChaBlock(m_Key, counter, nonce, block);
                const size_t n = std::min<size_t>(64, data.size() - offset);
                for (size_t i = 0; i < n; ++i) { data[offset + i] ^= block[i]; }
            }
            SecureZero(block, sizeof(block));
        }
        void ComputeTag(const uint32_t nonce[3], std::span<const uint8_t> aad, std::span<const uint8_t> ciphertext, uint8_t tag[AeadTagSize]) const {
            uint8_t otk[64];
            detail::ChaChaBlock(m_Key, 0, nonce, otk);
            detail::Poly1305 mac(otk);
            static constexpr uint8_t zeros[16] = {};
            mac.Update(aad.data(), aad.size());
            if (aad.size() % 16) { mac.Update(zeros, 16 - aad.size() % 16); }
            mac.Update(ciphertext.data(), ciphertext.size());
            if (ciphertext.size() % 16) { mac.Update(zeros, 16 - ciphertext.size() % 16); }
            uint8_t lengths[16];
            StoreLE64(lengths, aad.size());
            StoreLE64(lengths + 8, ciphertext.size());
            mac.Update(lengths, 16);
            mac.Final(tag);
            SecureZero(otk, sizeof(otk));
        }

        uint32_t m_Key[8]{};
    };

}
