// Small shared helpers: time, strings, validation, geo distance.
#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace util {
inline std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); }); return s; }
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b-1])) b--;
    return s.substr(a, b - a);
}
inline std::string urlDecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') o += ' ';
        else if (s[i] == '%' && i + 2 < s.size() + 0 && std::isxdigit((unsigned char)s[i+1]) && std::isxdigit((unsigned char)s[i+2])) {
            o += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16); i += 2;
        } else o += s[i];
    }
    return o;
}
inline std::string utcNow() {
    time_t t = time(nullptr); tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char buf[32]; strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &g); return buf;
}
inline bool validDate(const std::string& d) {   // YYYY-MM-DD
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (int i : {0,1,2,3,5,6,8,9}) if (!std::isdigit((unsigned char)d[i])) return false;
    int y = std::stoi(d.substr(0,4)), m = std::stoi(d.substr(5,2)), day = std::stoi(d.substr(8,2));
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12 || y < 1900 || y > 2200) return false;
    int lim = dm[m-1] + ((m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) ? 1 : 0);
    return day >= 1 && day <= lim;
}
inline bool validEmail(const std::string& e) {
    size_t at = e.find('@');
    if (at == std::string::npos || at == 0 || e.find('@', at + 1) != std::string::npos) return false;
    size_t dot = e.find('.', at);
    return dot != std::string::npos && dot > at + 1 && dot + 1 < e.size() && e.find(' ') == std::string::npos;
}
inline bool validPhone(const std::string& p) {
    int digits = 0;
    for (char c : p) { if (std::isdigit((unsigned char)c)) digits++; else if (!(c=='+'||c=='-'||c==' '||c=='('||c==')')) return false; }
    return digits >= 7 && digits <= 15;
}
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0, rad = 3.14159265358979323846 / 180.0;
    double dl = (lat2 - lat1) * rad, dn = (lon2 - lon1) * rad;
    double a = std::sin(dl/2)*std::sin(dl/2) + std::cos(lat1*rad)*std::cos(lat2*rad)*std::sin(dn/2)*std::sin(dn/2);
    return 2 * R * std::asin(std::sqrt(a));
}
} // namespace util

// Calendar helpers independent of platform time functions (Howard Hinnant's civil-date algorithms).
namespace util {
inline long long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
inline std::string civilFromDays(long long z) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = (long long)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);
    char buf[40]; snprintf(buf, sizeof buf, "%04lld-%02u-%02u", y, m, d); return buf;
}
inline std::string addDays(const std::string& date, int days) {
    return civilFromDays(daysFromCivil(std::stoi(date.substr(0,4)), (unsigned)std::stoi(date.substr(5,2)), (unsigned)std::stoi(date.substr(8,2))) + days);
}
inline std::string today() { return utcNow().substr(0, 10); }
} // namespace util
