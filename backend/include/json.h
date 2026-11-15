// Minimal JSON value, parser and serializer (no external dependency).
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <cmath>
#include <cstdio>
#include <cstdlib>

class Json {
public:
    enum Type { Null, Bool, Number, String, Array, Object };
private:
    Type t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    static void escape(const std::string& in, std::string& out) {
        out += '"';
        for (unsigned char c : in) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                else out += (char)c;
            }
        }
        out += '"';
    }

    struct Parser {
        const std::string& src; size_t p = 0;
        explicit Parser(const std::string& s_) : src(s_) {}
        [[noreturn]] void fail(const char* m) { throw std::runtime_error(std::string("JSON parse error: ") + m); }
        void ws() { while (p < src.size() && (src[p]==' '||src[p]=='\t'||src[p]=='\n'||src[p]=='\r')) p++; }
        bool lit(const char* w) {
            size_t l = std::char_traits<char>::length(w);
            if (src.compare(p, l, w) == 0) { p += l; return true; }
            return false;
        }
        static void utf8(unsigned cp, std::string& out) {
            if (cp < 0x80) out += (char)cp;
            else if (cp < 0x800) { out += (char)(0xC0|(cp>>6)); out += (char)(0x80|(cp&0x3F)); }
            else if (cp < 0x10000) { out += (char)(0xE0|(cp>>12)); out += (char)(0x80|((cp>>6)&0x3F)); out += (char)(0x80|(cp&0x3F)); }
            else { out += (char)(0xF0|(cp>>18)); out += (char)(0x80|((cp>>12)&0x3F)); out += (char)(0x80|((cp>>6)&0x3F)); out += (char)(0x80|(cp&0x3F)); }
        }
        unsigned hex4() {
            if (p + 4 > src.size()) fail("bad unicode escape");
            unsigned v = 0;
            for (int i = 0; i < 4; i++) {
                char c = src[p++]; v <<= 4;
                if (c>='0'&&c<='9') v |= c-'0';
                else if (c>='a'&&c<='f') v |= c-'a'+10;
                else if (c>='A'&&c<='F') v |= c-'A'+10;
                else fail("bad hex digit");
            }
            return v;
        }
        std::string str() {
            if (src[p] != '"') fail("expected string");
            p++; std::string out;
            while (true) {
                if (p >= src.size()) fail("unterminated string");
                char c = src[p++];
                if (c == '"') break;
                if ((unsigned char)c < 0x20) fail("control character in string");
                if (c != '\\') { out += c; continue; }
                if (p >= src.size()) fail("bad escape");
                char e = src[p++];
                switch (e) {
                case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (src.compare(p, 2, "\\u") != 0) fail("lone surrogate");
                        p += 2; unsigned lo = hex4();
                        if (lo < 0xDC00 || lo > 0xDFFF) fail("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(cp, out); break;
                }
                default: fail("bad escape");
                }
            }
            return out;
        }
        Json value(int depth) {
            if (depth > 64) fail("nesting too deep");
            ws();
            if (p >= src.size()) fail("unexpected end");
            char c = src[p];
            if (c == '{') {
                p++; Json j = Json::object(); ws();
                if (p < src.size() && src[p] == '}') { p++; return j; }
                while (true) {
                    ws(); std::string k = str(); ws();
                    if (p >= src.size() || src[p] != ':') fail("expected ':'");
                    p++; j[k] = value(depth + 1); ws();
                    if (p < src.size() && src[p] == ',') { p++; continue; }
                    if (p < src.size() && src[p] == '}') { p++; break; }
                    fail("expected ',' or '}'");
                }
                return j;
            }
            if (c == '[') {
                p++; Json j = Json::array(); ws();
                if (p < src.size() && src[p] == ']') { p++; return j; }
                while (true) {
                    j.push_back(value(depth + 1)); ws();
                    if (p < src.size() && src[p] == ',') { p++; continue; }
                    if (p < src.size() && src[p] == ']') { p++; break; }
                    fail("expected ',' or ']'");
                }
                return j;
            }
            if (c == '"') return Json(str());
            if (lit("true")) return Json(true);
            if (lit("false")) return Json(false);
            if (lit("null")) return Json();
            const char* start = src.c_str() + p; char* end = nullptr;
            if (!(c == '-' || (c >= '0' && c <= '9'))) fail("unexpected character");
            double d = strtod(start, &end);
            if (end == start) fail("bad number");
            p += (size_t)(end - start);
            return Json(d);
        }
    };

public:
    Json() {}
    Json(std::nullptr_t) {}
    Json(bool v) : t(Bool), b(v) {}
    Json(int v) : t(Number), n(v) {}
    Json(long v) : t(Number), n((double)v) {}
    Json(long long v) : t(Number), n((double)v) {}
    Json(unsigned long v) : t(Number), n((double)v) {}
    Json(double v) : t(Number), n(v) {}
    Json(const char* v) : t(String), s(v ? v : "") {}
    Json(const std::string& v) : t(String), s(v) {}

    static Json array() { Json j; j.t = Array; return j; }
    static Json object() { Json j; j.t = Object; return j; }
    static Json parse(const std::string& text) {
        Parser ps(text); Json j = ps.value(0); ps.ws();
        if (ps.p != text.size()) ps.fail("trailing characters");
        return j;
    }

    Type type() const { return t; }
    bool isNull() const { return t == Null; }
    bool isObject() const { return t == Object; }
    bool isArray() const { return t == Array; }
    bool isString() const { return t == String; }
    bool isNumber() const { return t == Number; }
    bool isBool() const { return t == Bool; }

    Json& operator[](const std::string& k) {
        if (t == Null) t = Object;
        for (auto& kv : o) if (kv.first == k) return kv.second;
        o.emplace_back(k, Json());
        return o.back().second;
    }
    const Json& at(const std::string& k) const {
        static const Json nul;
        for (auto& kv : o) if (kv.first == k) return kv.second;
        return nul;
    }
    bool has(const std::string& k) const {
        for (auto& kv : o) if (kv.first == k) return !kv.second.isNull();
        return false;
    }
    void push_back(const Json& v) { if (t == Null) t = Array; a.push_back(v); }
    size_t size() const { return t == Array ? a.size() : o.size(); }
    const std::vector<Json>& items() const { return a; }
    std::vector<Json>& items() { return a; }
    const std::vector<std::pair<std::string, Json>>& members() const { return o; }

    std::string str(const std::string& def = "") const { return t == String ? s : def; }
    double num(double def = 0) const { return t == Number ? n : def; }
    long long integer(long long def = 0) const { return t == Number ? (long long)std::llround(n) : def; }
    bool boolean(bool def = false) const { return t == Bool ? b : def; }
    bool isInteger() const { return t == Number && std::floor(n) == n && std::fabs(n) < 9e15; }

    void dumpTo(std::string& out) const {
        switch (t) {
        case Null: out += "null"; break;
        case Bool: out += b ? "true" : "false"; break;
        case Number:
            if (!std::isfinite(n)) out += "null";
            else if (isInteger()) out += std::to_string((long long)n);
            else { char buf[40]; snprintf(buf, sizeof buf, "%.10g", n); out += buf; }
            break;
        case String: escape(s, out); break;
        case Array:
            out += '[';
            for (size_t i = 0; i < a.size(); i++) { if (i) out += ','; a[i].dumpTo(out); }
            out += ']'; break;
        case Object:
            out += '{';
            for (size_t i = 0; i < o.size(); i++) { if (i) out += ','; escape(o[i].first, out); out += ':'; o[i].second.dumpTo(out); }
            out += '}'; break;
        }
    }
    std::string dump() const { std::string out; dumpTo(out); return out; }
};
