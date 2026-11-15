// Serves the built React app (frontend/dist) from the same port as the API, with SPA fallback to index.html.
#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include "http_server.h"

inline Handler makeStaticHandler(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root = fs::weakly_canonical(fs::path(dir), ec);
    return [root](HttpRequest& req, HttpResponse& res) {
        static const std::map<std::string, std::string> mime = {
            {".html", "text/html"}, {".js", "application/javascript"}, {".mjs", "application/javascript"}, {".css", "text/css"},
            {".json", "application/json"}, {".svg", "image/svg+xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".ico", "image/x-icon"},
            {".woff2", "font/woff2"}, {".woff", "font/woff"}, {".txt", "text/plain"}, {".map", "application/json"}};
        std::string rel = req.path;
        if (rel.find('\0') != std::string::npos || rel.find('\\') != std::string::npos) { res.status = 400; res.body = "Bad request"; res.contentType = "text/plain"; return; }
        if (rel == "/" || rel.empty()) rel = "/index.html";
        std::error_code e;
        fs::path p = fs::weakly_canonical(root / fs::path(rel.substr(1)), e);
        auto rs = root.string(), ps = p.string();
        bool inside = !e && ps.compare(0, rs.size(), rs) == 0 && (ps.size() == rs.size() || ps[rs.size()] == fs::path::preferred_separator);
        if (!inside || !fs::is_regular_file(p, e)) {               // unknown path or traversal attempt -> SPA entry point
            if (rel.find('.') != std::string::npos && rel.rfind("/index.html", 0) != 0) { res.status = 404; res.body = "Not found"; res.contentType = "text/plain"; return; }
            p = root / "index.html";
        }
        std::ifstream in(p, std::ios::binary);
        if (!in) { res.status = 404; res.body = "Frontend not built. Run: cd frontend && npm install && npm run build"; res.contentType = "text/plain"; return; }
        std::ostringstream ss; ss << in.rdbuf(); res.body = ss.str(); res.status = 200;
        auto it = mime.find(p.extension().string()); res.contentType = it == mime.end() ? "application/octet-stream" : it->second;
    };
}
