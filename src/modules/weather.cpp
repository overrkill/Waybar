#include "modules/weather.hpp"

#include <cstdlib>
#include <json/json.h>
#include <spdlog/spdlog.h>

#include <array>
#include <memory>

namespace waybar::modules {

namespace {

auto getWeatherIcon(int code) -> std::string {
  if (code == 0) return "clear";
  if (code >= 1 && code <= 3) return "partly-cloudy";
  if (code >= 45 && code <= 48) return "fog";
  if (code >= 51 && code <= 55) return "drizzle";
  if (code >= 56 && code <= 57) return "freezing-drizzle";
  if (code >= 61 && code <= 65) return "rain";
  if (code >= 66 && code <= 67) return "freezing-rain";
  if (code >= 71 && code <= 77) return "snow";
  if (code >= 80 && code <= 82) return "showers";
  if (code >= 85 && code <= 86) return "snow-showers";
  if (code >= 95 && code <= 99) return "thunderstorm";
  return "unknown";
}

}  // namespace

Weather::Weather(const std::string& id, const Json::Value& config)
    : ALabel(config, "weather", id, "{icon} {temperature}°C {humidity}%", 30) {
  if (!config["latitude"].isNumeric() || !config["longitude"].isNumeric()) {
    throw std::runtime_error("weather: latitude and longitude are required");
  }

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto Weather::fetchWeather() -> bool {
  auto lat = config_["latitude"].asFloat();
  auto lon = config_["longitude"].asFloat();

  std::string url = fmt::format(
      "curl -s 'https://api.open-meteo.com/v1/forecast?"
      "latitude={}&longitude={}&current=temperature_2m,relative_humidity_2m,weather_code'",
      lat, lon);

  std::array<char, 128> buffer;
  std::string result;

  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(url.c_str(), "r"), pclose);
  if (!pipe) {
    spdlog::error("weather: failed to fetch weather data");
    return false;
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  parseWeather(result);
  return true;
}

auto Weather::parseWeather(const std::string& json_str) -> void {
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::istringstream stream(json_str);
  std::string errs;

  if (!Json::parseFromStream(builder, stream, &root, &errs)) {
    spdlog::error("weather: failed to parse JSON: {}", errs);
    return;
  }

  auto& current = root["current"];
  if (current.isObject()) {
    if (current["temperature_2m"].isNumeric()) {
      temperature_ = std::round(current["temperature_2m"].asFloat());
    }
    if (current["relative_humidity_2m"].isNumeric()) {
      humidity_ = std::round(current["relative_humidity_2m"].asFloat());
    }
    if (current["weather_code"].isNumeric()) {
      icon_ = getWeatherIcon(current["weather_code"].asInt());
    }
  }
}

auto Weather::update() -> void {
  if (!fetchWeather()) {
    temperature_ = 0;
    humidity_ = 0;
    icon_ = "unknown";
  }

  auto format = format_;
  if (format.empty()) {
    event_box_.hide();
    return;
  }

  event_box_.show();
  label_.set_markup(fmt::format(fmt::runtime(format), fmt::arg("icon", getIcon(0, icon_)),
                                fmt::arg("temperature", temperature_),
                                fmt::arg("humidity", humidity_)));

  if (tooltipEnabled()) {
    label_.set_tooltip_markup(fmt::format("Temperature: {}°C\nHumidity: {}%", temperature_,
                                          humidity_));
  }

  ALabel::update();
}

}  // namespace waybar::modules