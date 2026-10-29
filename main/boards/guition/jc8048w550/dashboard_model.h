#ifndef JC8048W550_DASHBOARD_MODEL_H
#define JC8048W550_DASHBOARD_MODEL_H

#include "device_state.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Data behind the idle screen: types, pure helpers and JSON parsers. No LVGL
// and no network in here, so everything can be checked without the display.
namespace dashboard {

enum class WeatherIcon : uint8_t {
    kSunny,
    kPartlyCloudy,
    kCloudy,
    kFog,
    kRain,
    kThunder,
    kSnow,
};

// WMO weather interpretation codes, as returned by Open-Meteo.
constexpr WeatherIcon WeatherIconFromWmo(int wmo_code) {
    switch (wmo_code) {
        case 0:
        case 1:
            return WeatherIcon::kSunny;
        case 2:
            return WeatherIcon::kPartlyCloudy;
        case 45:
        case 48:
            return WeatherIcon::kFog;
        case 51 ... 57:  // drizzle
        case 61 ... 67:  // rain
        case 80 ... 82:  // rain showers
            return WeatherIcon::kRain;
        case 71 ... 77:  // snow
        case 85:
        case 86:  // snow showers
            return WeatherIcon::kSnow;
        case 95 ... 99:
            return WeatherIcon::kThunder;
        default:  // 3 (overcast) and anything unknown
            return WeatherIcon::kCloudy;
    }
}

constexpr const char* WeatherTextFromWmo(int wmo_code) {
    switch (wmo_code) {
        case 0:
            return "Clear";
        case 1:
            return "Mostly clear";
        case 2:
            return "Partly cloudy";
        case 45:
        case 48:
            return "Fog";
        case 51 ... 57:
            return "Drizzle";
        case 61 ... 67:
            return "Rain";
        case 80 ... 82:
            return "Showers";
        case 71 ... 77:
            return "Snow";
        case 85:
        case 86:
            return "Snow showers";
        case 95 ... 99:
            return "Thunderstorm";
        default:
            return "Cloudy";
    }
}

// 3-letter English weekday of a civil date ("Mon".."Sun"), Sakamoto's method.
constexpr const char* Weekday3(int year, int month, int day) {
    constexpr const char* kNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    constexpr int kOffsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 1 || month > 12) {
        return kNames[0];
    }
    if (month < 3) {
        year -= 1;
    }
    int index = (year + year / 4 - year / 100 + year / 400 + kOffsets[month - 1] + day) % 7;
    return kNames[index];
}

// Fetching is skipped unless the device is idle, the clock is set (the GitHub
// query needs today's date) and enough internal RAM is free for a TLS session.
constexpr bool FetchAllowed(DeviceState state, bool time_synced, size_t free_internal_bytes,
                            size_t min_free_internal_bytes) {
    return state == kDeviceStateIdle && time_synced &&
           free_internal_bytes >= min_free_internal_bytes;
}

namespace detail {
constexpr std::string UIntToString(unsigned value) {
    if (value == 0) {
        return "0";
    }
    std::string digits;
    while (value > 0) {
        digits.insert(digits.begin(), static_cast<char>('0' + value % 10));
        value /= 10;
    }
    return digits;
}
}  // namespace detail

// 950 -> "950", 12400 -> "12.4k".
constexpr std::string FormatStars(int stars) {
    if (stars < 1000) {
        return detail::UIntToString(stars < 0 ? 0u : static_cast<unsigned>(stars));
    }
    unsigned tenths = static_cast<unsigned>(stars) / 100;
    return detail::UIntToString(tenths / 10) + "." + detail::UIntToString(tenths % 10) + "k";
}

// Every byte >= 0x80 becomes '?', control characters become a space, and the
// result is cut to max_chars.
constexpr std::string SanitizeAscii(std::string_view text, size_t max_chars) {
    std::string out;
    for (char raw : text) {
        if (out.size() >= max_chars) {
            break;
        }
        unsigned char c = static_cast<unsigned char>(raw);
        if (c >= 0x80) {
            out += '?';
        } else if (c < 0x20 || c == 0x7f) {
            out += ' ';
        } else {
            out += raw;
        }
    }
    return out;
}

struct ForecastDay {
    std::string weekday;
    WeatherIcon icon = WeatherIcon::kCloudy;
    int high_c = 0;
    int low_c = 0;
};

struct Weather {
    std::string city;
    int temp_c = 0;
    int high_c = 0;
    int low_c = 0;
    int humidity = 0;
    WeatherIcon icon = WeatherIcon::kCloudy;
    std::string condition;
    std::array<ForecastDay, 7> days;
};

struct Repo {
    std::string full_name;
    std::string description;
    std::string language;
    std::string updated;  // YYYY-MM-DD
    std::vector<std::string> topics;
    int stars = 0;
    int forks = 0;
};

struct GeoLocation {
    double latitude = 0;
    double longitude = 0;
    std::string city;
};

template <class T>
struct Source {
    T data{};
    bool valid = false;        // data holds a successful fetch
    bool last_failed = false;  // the most recent attempt failed
    int64_t fetched_at_s = 0;  // time() of the last success
};

struct Snapshot {
    Source<Weather> weather;
    Source<std::vector<Repo>> repos;
};

// Parsers return false, leaving `out` untouched, when a required field is
// missing or has the wrong type. ParseRepos accepts an empty result and keeps
// at most 5 repos; text fields are run through SanitizeAscii.
bool ParseGeoIp(const std::string& json, GeoLocation& out);
// Today is daily[0]; days[0..6] are the seven days after it.
bool ParseWeather(const std::string& json, const std::string& city, Weather& out);
bool ParseRepos(const std::string& json, std::vector<Repo>& out);

// Checks the parsers against captured responses; logs the result. Development
// aid only: nothing calls it in production.
bool RunModelSelfTest();

}  // namespace dashboard

#endif  // JC8048W550_DASHBOARD_MODEL_H
