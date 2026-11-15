// SHA-256, HMAC-SHA256, PBKDF2-HMAC-SHA256 and secure random helpers.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <cstdio>

namespace crypto {

struct Sha256 {
    uint32_t h[8]; uint64_t len = 0; uint8_t buf[64]; size_t blen = 0;
    Sha256() { reset(); }
    void reset() {
        static const uint32_t i[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
        memcpy(h, i, sizeof h); len = 0; blen = 0;
    }
    static uint32_t rr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void block(const uint8_t* d) {
        static const uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)d[i*4]<<24 | (uint32_t)d[i*4+1]<<16 | (uint32_t)d[i*4+2]<<8 | d[i*4+3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rr(w[i-15],7) ^ rr(w[i-15],18) ^ (w[i-15]>>3);
            uint32_t s1 = rr(w[i-2],17) ^ rr(w[i-2],19) ^ (w[i-2]>>10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],dd=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rr(e,6) ^ rr(e,11) ^ rr(e,25), ch = (e&f) ^ (~e&g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rr(a,2) ^ rr(a,13) ^ rr(a,22), mj = (a&b) ^ (a&c) ^ (b&c);
            uint32_t t2 = S0 + mj;
            hh=g; g=f; f=e; e=dd+t1; dd=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=dd; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    void update(const uint8_t* d, size_t n) {
        len += n;
        while (n) {
            size_t take = 64 - blen; if (take > n) take = n;
            memcpy(buf + blen, d, take); blen += take; d += take; n -= take;
            if (blen == 64) { block(buf); blen = 0; }
        }
    }
    std::vector<uint8_t> finish() {
        uint64_t bits = len * 8; uint8_t pad = 0x80;
        update(&pad, 1); pad = 0;
        while (blen != 56) update(&pad, 1);
        uint8_t lb[8]; for (int i = 0; i < 8; i++) lb[i] = (uint8_t)(bits >> (56 - 8*i));
        update(lb, 8);
        std::vector<uint8_t> out(32);
        for (int i = 0; i < 8; i++) { out[i*4]=h[i]>>24; out[i*4+1]=h[i]>>16; out[i*4+2]=h[i]>>8; out[i*4+3]=h[i]; }
        return out;
    }
};

inline std::vector<uint8_t> sha256(const std::vector<uint8_t>& d) {
    Sha256 s; s.update(d.data(), d.size()); return s.finish();
}
inline std::vector<uint8_t> bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

inline std::vector<uint8_t> hmac(const std::vector<uint8_t>& key, const std::vector<uint8_t>& msg) {
    std::vector<uint8_t> k = key;
    if (k.size() > 64) k = sha256(k);
    k.resize(64, 0);
    std::vector<uint8_t> ip(64), op(64);
    for (int i = 0; i < 64; i++) { ip[i] = k[i] ^ 0x36; op[i] = k[i] ^ 0x5c; }
    Sha256 in; in.update(ip.data(), 64); in.update(msg.data(), msg.size());
    auto ih = in.finish();
    Sha256 out; out.update(op.data(), 64); out.update(ih.data(), ih.size());
    return out.finish();
}

inline std::vector<uint8_t> pbkdf2(const std::string& password, const std::vector<uint8_t>& salt, int iterations, size_t dkLen = 32) {
    auto pw = bytes(password);
    std::vector<uint8_t> result;
    for (uint32_t block = 1; result.size() < dkLen; block++) {
        std::vector<uint8_t> s = salt;
        s.push_back(block >> 24); s.push_back(block >> 16); s.push_back(block >> 8); s.push_back(block);
        auto u = hmac(pw, s); auto t = u;
        for (int i = 1; i < iterations; i++) {
            u = hmac(pw, u);
            for (size_t j = 0; j < t.size(); j++) t[j] ^= u[j];
        }
        result.insert(result.end(), t.begin(), t.end());
    }
    result.resize(dkLen);
    return result;
}

inline std::string hex(const std::vector<uint8_t>& d) {
    static const char* dg = "0123456789abcdef"; std::string s;
    for (uint8_t c : d) { s += dg[c >> 4]; s += dg[c & 15]; }
    return s;
}
inline std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    if (s.size() % 2) return out;
    for (size_t i = 0; i < s.size(); i += 2) out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
inline std::vector<uint8_t> randomBytes(size_t n) {
    std::random_device rd; std::vector<uint8_t> v(n);
    for (auto& b : v) b = (uint8_t)(rd() & 0xFF);
    return v;
}
inline bool constantTimeEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char d = 0;
    for (size_t i = 0; i < a.size(); i++) d |= (unsigned char)(a[i] ^ b[i]);
    return d == 0;
}

// Stored format: pbkdf2$<iterations>$<salt hex>$<hash hex>
inline std::string hashPassword(const std::string& password, int iterations = 50000) {
    auto salt = randomBytes(16);
    return "pbkdf2$" + std::to_string(iterations) + "$" + hex(salt) + "$" + hex(pbkdf2(password, salt, iterations));
}
inline bool verifyPassword(const std::string& password, const std::string& stored) {
    size_t p1 = stored.find('$'), p2 = stored.find('$', p1 + 1), p3 = stored.find('$', p2 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos) return false;
    if (stored.substr(0, p1) != "pbkdf2") return false;
    int it = std::atoi(stored.substr(p1 + 1, p2 - p1 - 1).c_str());
    if (it < 1000 || it > 5000000) return false;
    auto salt = unhex(stored.substr(p2 + 1, p3 - p2 - 1));
    return constantTimeEq(hex(pbkdf2(password, salt, it)), stored.substr(p3 + 1));
}
} // namespace crypto
