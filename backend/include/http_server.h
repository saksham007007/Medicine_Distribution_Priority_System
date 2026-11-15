// Small embedded HTTP/1.1 server + router (POSIX sockets, Winsock on Windows).
// Used instead of a third-party framework so the backend has no dependency
// besides SQLite. Routes are registered on a Router, so a framework such as
// Crow could replace this layer without touching the business logic.
#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <sstream>
#include <cstring>
#include <csignal>
#include <fstream>
#include "json.h"
#include "util.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define CLOSESOCK closesocket
  #define INVALID_SOCK INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <sys/time.h>
  #include <unistd.h>
  typedef int sock_t;
  #define CLOSESOCK close
  #define INVALID_SOCK (-1)
#endif

struct ApiError : std::runtime_error {
    int status;
    ApiError(int s, const std::string& m) : std::runtime_error(m), status(s) {}
};

struct HttpRequest {
    std::string method, path, body, ip;
    std::map<std::string, std::string> query, headers, params;
    std::string header(const std::string& k) const { auto it = headers.find(util::lower(k)); return it == headers.end() ? "" : it->second; }
    std::string q(const std::string& k, const std::string& def = "") const { auto it = query.find(k); return it == query.end() ? def : it->second; }
    std::string param(const std::string& k) const { auto it = params.find(k); return it == params.end() ? "" : it->second; }
    long long paramInt(const std::string& k) const {
        std::string v = param(k);
        if (v.empty() || v.size() > 12) throw ApiError(400, "Invalid id");
        for (char c : v) if (!isdigit((unsigned char)c)) throw ApiError(400, "Invalid id");
        return std::stoll(v);
    }
    Json json() const {
        if (body.empty()) throw ApiError(400, "Request body is required");
        try { Json j = Json::parse(body); if (!j.isObject()) throw ApiError(400, "JSON object expected"); return j; }
        catch (const ApiError&) { throw; }
        catch (const std::exception& e) { throw ApiError(400, e.what()); }
    }
};

struct HttpResponse {
    int status = 200;
    std::string body, contentType = "application/json";
    void send(int s, const Json& j) { status = s; body = j.dump(); contentType = "application/json"; }
};

using Handler = std::function<void(HttpRequest&, HttpResponse&)>;

class Router {
    struct Route { std::string method; std::vector<std::string> segs; Handler h; };
    std::vector<Route> routes;
public:
    Handler fallback;                // optional: serves static files for non-API GET requests
private:
    static std::vector<std::string> split(const std::string& p) {
        std::vector<std::string> v; std::string cur;
        for (char c : p) { if (c == '/') { if (!cur.empty()) v.push_back(cur); cur.clear(); } else cur += c; }
        if (!cur.empty()) v.push_back(cur);
        return v;
    }
public:
    void add(const std::string& method, const std::string& pattern, Handler h) { routes.push_back({method, split(pattern), std::move(h)}); }
    void get(const std::string& p, Handler h) { add("GET", p, std::move(h)); }
    void post(const std::string& p, Handler h) { add("POST", p, std::move(h)); }
    void put(const std::string& p, Handler h) { add("PUT", p, std::move(h)); }
    void del(const std::string& p, Handler h) { add("DELETE", p, std::move(h)); }
    size_t count() const { return routes.size(); }

    void dispatch(HttpRequest& req, HttpResponse& res) {
        auto segs = split(req.path); bool pathMatched = false;
        for (auto& r : routes) {
            if (r.segs.size() != segs.size()) continue;
            std::map<std::string, std::string> params; bool ok = true;
            for (size_t i = 0; i < segs.size() && ok; i++) {
                if (!r.segs[i].empty() && r.segs[i][0] == ':') params[r.segs[i].substr(1)] = util::urlDecode(segs[i]);
                else if (r.segs[i] != segs[i]) ok = false;
            }
            if (!ok) continue;
            pathMatched = true;
            if (r.method != req.method) continue;
            req.params = params;
            r.h(req, res);
            return;
        }
        if (!pathMatched && fallback && req.method == "GET" && req.path.compare(0, 5, "/api/") != 0) { fallback(req, res); return; }
        Json e = Json::object(); e["error"] = pathMatched ? "Method not allowed" : "Route not found";
        res.send(pathMatched ? 405 : 404, e);
    }
};

class HttpServer {
    std::mutex lock;                 // all handlers run one at a time (SQLite + in-memory state)
    std::atomic<bool> running{true};
    static const char* reason(int s) {
        switch (s) { case 200: return "OK"; case 201: return "Created"; case 204: return "No Content"; case 400: return "Bad Request";
            case 401: return "Unauthorized"; case 403: return "Forbidden"; case 404: return "Not Found"; case 405: return "Method Not Allowed";
            case 409: return "Conflict"; case 413: return "Payload Too Large"; case 422: return "Unprocessable Entity"; case 429: return "Too Many Requests"; default: return "Internal Server Error"; }
    }
    static bool sendAll(sock_t s, const std::string& d) {
        size_t off = 0;
        while (off < d.size()) {
#ifdef MSG_NOSIGNAL
            long n = (long)send(s, d.data() + off, d.size() - off, MSG_NOSIGNAL);
#else
            long n = (long)send(s, d.data() + off, (int)(d.size() - off), 0);
#endif
            if (n <= 0) return false;
            off += (size_t)n;
        }
        return true;
    }
    void serve(sock_t c, const std::string& ip, Router& router) {
        std::string data; char buf[8192]; size_t headerEnd = std::string::npos; size_t need = 0;
        const size_t MAXBODY = 2 * 1024 * 1024;
        HttpResponse res; HttpRequest req; req.ip = ip; bool parsed = false;
        while (true) {
            long n = (long)recv(c, buf, sizeof buf, 0);
            if (n <= 0) break;
            data.append(buf, (size_t)n);
            if (headerEnd == std::string::npos) {
                headerEnd = data.find("\r\n\r\n");
                if (headerEnd == std::string::npos) { if (data.size() > 65536) break; continue; }
                std::istringstream hs(data.substr(0, headerEnd)); std::string line;
                std::getline(hs, line);
                std::istringstream rl(line); std::string target, ver; rl >> req.method >> target >> ver;
                size_t qm = target.find('?');
                req.path = qm == std::string::npos ? target : target.substr(0, qm);
                if (qm != std::string::npos) {
                    std::string qs = target.substr(qm + 1); size_t pos = 0;
                    while (pos <= qs.size()) {
                        size_t amp = qs.find('&', pos); if (amp == std::string::npos) amp = qs.size();
                        std::string kv = qs.substr(pos, amp - pos); size_t eq = kv.find('=');
                        if (!kv.empty()) req.query[util::urlDecode(kv.substr(0, eq))] = eq == std::string::npos ? "" : util::urlDecode(kv.substr(eq + 1));
                        pos = amp + 1;
                    }
                }
                while (std::getline(hs, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    size_t col = line.find(':');
                    if (col != std::string::npos) req.headers[util::lower(line.substr(0, col))] = util::trim(line.substr(col + 1));
                }
                try { need = req.header("content-length").empty() ? 0 : (size_t)std::stoull(req.header("content-length")); } catch (...) { need = 0; }
                if (need > MAXBODY) { Json e = Json::object(); e["error"] = "Payload too large"; res.send(413, e); parsed = true; break; }
                parsed = true;
            }
            if (headerEnd != std::string::npos && data.size() >= headerEnd + 4 + need) break;
        }
        if (parsed && res.body.empty()) {
            req.body = data.substr(headerEnd + 4, need);
            if (req.method == "OPTIONS") { res.status = 204; res.body.clear(); }
            else {
                std::lock_guard<std::mutex> g(lock);
                try { router.dispatch(req, res); }
                catch (const ApiError& e) { Json j = Json::object(); j["error"] = e.what(); res.send(e.status, j); }
                catch (const std::exception& e) {
                    std::cerr << "[error] " << req.method << " " << req.path << ": " << e.what() << std::endl;
                    Json j = Json::object(); j["error"] = "Internal server error"; res.send(500, j);
                }
            }
        } else if (!parsed) { CLOSESOCK(c); return; }
        std::string out = "HTTP/1.1 " + std::to_string(res.status) + " " + reason(res.status) + "\r\n"
            "Content-Type: " + res.contentType + ((res.contentType.compare(0, 5, "text/") == 0 || res.contentType == "application/json" || res.contentType == "application/javascript") ? "; charset=utf-8" : "") + "\r\n"
            "Content-Length: " + std::to_string(res.body.size()) + "\r\n"
            "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Authorization, Content-Type\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "X-Content-Type-Options: nosniff\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + res.body;
        sendAll(c, out);
        CLOSESOCK(c);
    }
public:
    void stop() { running = false; }
    bool listen(const std::string& host, int port, Router& router) {
#ifdef _WIN32
        WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
#else
        signal(SIGPIPE, SIG_IGN);
#endif
        sock_t s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCK) return false;
        int yes = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port);
        a.sin_addr.s_addr = inet_addr(host.c_str());
        if (bind(s, (sockaddr*)&a, sizeof a) < 0 || ::listen(s, 64) < 0) { CLOSESOCK(s); return false; }
        while (running) {
            sockaddr_in ca{}; socklen_t cl = sizeof ca;
            sock_t c = accept(s, (sockaddr*)&ca, &cl);
            if (c == INVALID_SOCK) continue;
#ifdef _WIN32
            DWORD tv = 15000; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
#else
            timeval tv{15, 0}; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
            std::string ip = inet_ntoa(ca.sin_addr);
            std::thread([this, c, ip, &router]() { serve(c, ip, router); }).detach();
        }
        CLOSESOCK(s);
        return true;
    }
};
