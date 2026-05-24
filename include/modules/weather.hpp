#pragma once

#include <fmt/core.h>
#include <json/json.h>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Weather : public ALabel {
 public:
  Weather(const std::string&, const Json::Value&);
  virtual ~Weather() = default;
  auto update() -> void override;

 private:
  auto fetchWeather() -> bool;
  auto parseWeather(const std::string&) -> void;

  util::SleeperThread thread_;
  std::string icon_;
  int temperature_;
  int humidity_;
};

}  // namespace waybar::modules