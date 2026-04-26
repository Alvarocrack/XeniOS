/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_config_builder.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/config.h"

namespace {

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

std::string TrimAscii(std::string value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

NSTimeInterval GetUnixTimeSeconds() { return [[NSDate date] timeIntervalSince1970]; }

NSString* const kXeniaPendingExternalLaunchPathPreferenceKey = @"ios_pending_external_launch_path";
NSString* const kXeniaPendingExternalLaunchTimestampPreferenceKey =
    @"ios_pending_external_launch_timestamp";

constexpr NSTimeInterval kXeniaPendingExternalLaunchTTLSeconds = 120.0;

}  // namespace

NSString* const kXeniaAutoOpenStikDebugOnLaunchPreferenceKey = @"ios_auto_open_stikdebug_on_launch";
NSString* const kXeniaLastAutoStikDebugAttemptTimestampPreferenceKey =
    @"ios_last_auto_stikdebug_attempt_timestamp";

static cvar::IConfigVar* GetConfigVar(const std::string& key) {
  if (!cvar::ConfigVars) {
    return nullptr;
  }
  auto it = cvar::ConfigVars->find(key);
  if (it == cvar::ConfigVars->end()) {
    return nullptr;
  }
  return it->second;
}

static bool HasConfigVar(const std::string& key) { return GetConfigVar(key) != nullptr; }

static std::string GetConfigVarString(const std::string& key, const std::string& fallback) {
  cvar::IConfigVar* var = GetConfigVar(key);
  if (!var) {
    return fallback;
  }
  return TrimAscii(var->config_value());
}

static bool ParseBoolString(const std::string& text, bool* value_out) {
  if (!value_out) {
    return false;
  }
  std::string lower = text;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (lower == "true" || lower == "1") {
    *value_out = true;
    return true;
  }
  if (lower == "false" || lower == "0") {
    *value_out = false;
    return true;
  }
  return false;
}

static bool ParseInt64String(const std::string& text, int64_t* value_out) {
  if (!value_out) {
    return false;
  }
  char* end = nullptr;
  errno = 0;
  long long parsed = std::strtoll(text.c_str(), &end, 10);
  if (errno != 0 || !end || *end != '\0') {
    return false;
  }
  *value_out = static_cast<int64_t>(parsed);
  return true;
}

static NSUserDefaults* GetUserDefaults() { return [NSUserDefaults standardUserDefaults]; }

bool GetUserDefaultBool(NSString* key, bool fallback) {
  if (!key || key.length == 0) {
    return fallback;
  }
  if (![GetUserDefaults() objectForKey:key]) {
    return fallback;
  }
  return [GetUserDefaults() boolForKey:key];
}

double GetUserDefaultDouble(NSString* key, double fallback) {
  if (!key || key.length == 0) {
    return fallback;
  }
  if (![GetUserDefaults() objectForKey:key]) {
    return fallback;
  }
  return [GetUserDefaults() doubleForKey:key];
}

static NSString* GetUserDefaultString(NSString* key) {
  if (!key || key.length == 0) {
    return nil;
  }
  return [GetUserDefaults() stringForKey:key];
}

static void SetUserDefaultBool(NSString* key, bool value) {
  if (!key || key.length == 0) {
    return;
  }
  [GetUserDefaults() setBool:value forKey:key];
}

void SetUserDefaultDouble(NSString* key, double value) {
  if (!key || key.length == 0) {
    return;
  }
  [GetUserDefaults() setDouble:value forKey:key];
}

static void SetUserDefaultString(NSString* key, NSString* value) {
  if (!key || key.length == 0) {
    return;
  }
  if (value.length > 0) {
    [GetUserDefaults() setObject:value forKey:key];
  } else {
    [GetUserDefaults() removeObjectForKey:key];
  }
}

void ClearPendingExternalLaunchPathPreference() {
  [GetUserDefaults() removeObjectForKey:kXeniaPendingExternalLaunchPathPreferenceKey];
  [GetUserDefaults() removeObjectForKey:kXeniaPendingExternalLaunchTimestampPreferenceKey];
}

void StorePendingExternalLaunchPathPreference(const std::filesystem::path& path) {
  NSString* path_string = ToNSString(path.string());
  if (!path_string || path_string.length == 0) {
    ClearPendingExternalLaunchPathPreference();
    return;
  }
  SetUserDefaultString(kXeniaPendingExternalLaunchPathPreferenceKey, path_string);
  SetUserDefaultDouble(kXeniaPendingExternalLaunchTimestampPreferenceKey, GetUnixTimeSeconds());
}

std::filesystem::path TakePendingExternalLaunchPathPreference() {
  NSString* path_string = GetUserDefaultString(kXeniaPendingExternalLaunchPathPreferenceKey);
  const double stored_at =
      GetUserDefaultDouble(kXeniaPendingExternalLaunchTimestampPreferenceKey, 0.0);
  ClearPendingExternalLaunchPathPreference();
  if (!path_string || path_string.length == 0 || stored_at <= 0.0) {
    return std::filesystem::path();
  }
  if ((GetUnixTimeSeconds() - stored_at) > kXeniaPendingExternalLaunchTTLSeconds) {
    XELOGW("iOS: Discarding stale deferred external launch request");
    return std::filesystem::path();
  }
  return std::filesystem::path([path_string UTF8String]).lexically_normal();
}

static bool SetConfigVarBool(const std::string& key, bool value) {
  cvar::IConfigVar* var = GetConfigVar(key);
  if (!var) {
    XELOGW("iOS settings: missing config var '{}'", key);
    return false;
  }
  toml::value node(value);
  var->LoadConfigValue(&node);
  return true;
}

static bool SetConfigVarInt32(const std::string& key, int32_t value) {
  cvar::IConfigVar* var = GetConfigVar(key);
  if (!var) {
    XELOGW("iOS settings: missing config var '{}'", key);
    return false;
  }
  toml::value node(value);
  var->LoadConfigValue(&node);
  return true;
}

static bool SetConfigVarUInt64(const std::string& key, uint64_t value) {
  cvar::IConfigVar* var = GetConfigVar(key);
  if (!var) {
    XELOGW("iOS settings: missing config var '{}'", key);
    return false;
  }
  toml::value node(value);
  var->LoadConfigValue(&node);
  return true;
}

static bool SetConfigVarString(const std::string& key, const std::string& value) {
  cvar::IConfigVar* var = GetConfigVar(key);
  if (!var) {
    XELOGW("iOS settings: missing config var '{}'", key);
    return false;
  }
  toml::value node(value);
  var->LoadConfigValue(&node);
  return true;
}

static void AddBoolSetting(std::vector<IOSConfigItem>& items, const std::string& key,
                           const std::string& title, const std::string& subtitle, bool fallback) {
  if (!HasConfigVar(key)) {
    return;
  }
  IOSConfigItem item;
  item.key = key;
  item.title = title;
  item.subtitle = subtitle;
  item.control_type = IOSConfigControlType::kToggle;
  item.bool_value = fallback;
  ParseBoolString(GetConfigVarString(key, fallback ? "true" : "false"), &item.bool_value);
  items.push_back(std::move(item));
}

static void AddUserDefaultBoolSetting(std::vector<IOSConfigItem>& items, NSString* key,
                                      const std::string& title, const std::string& subtitle,
                                      bool fallback) {
  if (!key || key.length == 0) {
    return;
  }
  IOSConfigItem item;
  item.key = std::string([key UTF8String]);
  item.title = title;
  item.subtitle = subtitle;
  item.control_type = IOSConfigControlType::kToggle;
  item.storage = IOSConfigStorage::kUserDefaults;
  item.bool_value = GetUserDefaultBool(key, fallback);
  items.push_back(std::move(item));
}

static void AddChoiceSetting(std::vector<IOSConfigItem>& items, IOSConfigControlType control_type,
                             const std::string& key, const std::string& title,
                             const std::string& subtitle, int64_t fallback,
                             std::vector<IOSConfigChoice> choices) {
  if (!HasConfigVar(key) || choices.empty()) {
    return;
  }
  IOSConfigItem item;
  item.key = key;
  item.title = title;
  item.subtitle = subtitle;
  item.control_type = control_type;
  item.choice_value = fallback;
  ParseInt64String(GetConfigVarString(key, std::to_string(fallback)), &item.choice_value);
  item.choices = std::move(choices);
  bool found = false;
  for (const IOSConfigChoice& choice : item.choices) {
    if (choice.value == item.choice_value) {
      found = true;
      break;
    }
  }
  if (!found) {
    item.choice_value = item.choices.front().value;
  }
  items.push_back(std::move(item));
}

static void AddStringChoiceSetting(std::vector<IOSConfigItem>& items, const std::string& key,
                                   const std::string& title, const std::string& subtitle,
                                   const std::string& fallback,
                                   std::vector<std::pair<std::string, std::string>> choices) {
  if (!HasConfigVar(key) || choices.empty()) {
    return;
  }
  IOSConfigItem item;
  item.key = key;
  item.title = title;
  item.subtitle = subtitle;
  item.control_type = IOSConfigControlType::kChoiceString;
  item.string_value = GetConfigVarString(key, fallback);

  for (size_t i = 0; i < choices.size(); ++i) {
    item.choices.push_back({choices[i].first, static_cast<int64_t>(i)});
    item.choice_string_values.push_back(choices[i].second);
  }

  bool found = false;
  for (size_t i = 0; i < item.choice_string_values.size(); ++i) {
    if (item.choice_string_values[i] == item.string_value) {
      item.choice_value = static_cast<int64_t>(i);
      found = true;
      break;
    }
  }
  if (!found) {
    item.string_value = fallback;
    for (size_t i = 0; i < item.choice_string_values.size(); ++i) {
      if (item.choice_string_values[i] == fallback) {
        item.choice_value = static_cast<int64_t>(i);
        found = true;
        break;
      }
    }
  }
  if (!found) {
    item.choice_value = 0;
    item.string_value = item.choice_string_values.front();
  }

  items.push_back(std::move(item));
}

static void AddActionSetting(std::vector<IOSConfigItem>& items, IOSConfigAction action,
                             const std::string& title, const std::string& subtitle) {
  IOSConfigItem item;
  item.title = title;
  item.subtitle = subtitle;
  item.control_type = IOSConfigControlType::kAction;
  item.action = action;
  items.push_back(std::move(item));
}

std::string ChoiceTitleForItem(const IOSConfigItem& item) {
  for (const IOSConfigChoice& choice : item.choices) {
    if (choice.value == item.choice_value) {
      return choice.title;
    }
  }
  return item.choices.empty() ? std::string() : item.choices.front().title;
}

std::vector<IOSConfigSection> BuildIOSConfigSections() {
  std::vector<IOSConfigSection> sections;

  IOSConfigSection display;
  display.title = "Display";
  display.footer = "These settings affect frame pacing and the Metal presenter output.";
  AddBoolSetting(display.items, "metal_presenter_force_10bpc", "Force 10bpc Presenter Output",
                 "Metal-only. Uses RGB10A2 output, which is the default path and usually "
                 "reduces gamma-conversion cost on Apple GPUs. Disable only if colors, "
                 "captures, or display compatibility look wrong.",
                 true);
  AddChoiceSetting(display.items, IOSConfigControlType::kChoiceUInt64, "framerate_limit",
                   "Frame Rate Limit",
                   "Caps host presentation only; guest timing is separate. Use this to "
                   "reduce heat and battery drain. 120 FPS only matters on high-refresh "
                   "displays.",
                   0,
                   {{"Unlimited", 0},
                    {"30 FPS", 30},
                    {"45 FPS", 45},
                    {"60 FPS", 60},
                    {"90 FPS", 90},
                    {"120 FPS", 120}});
  AddBoolSetting(display.items, "guest_display_refresh_cap", "Cap Guest Display Refresh",
                 "Keeps guest vblank at console timing instead of running as fast as "
                 "possible. Turn this off only for troubleshooting speed or timing-"
                 "sensitive boot issues.",
                 true);
  AddBoolSetting(display.items, "use_50Hz_mode", "Use 50Hz PAL Timing",
                 "Only matters when guest refresh cap is enabled. Required by some PAL "
                 "titles; leave this off for most games to keep normal 60 Hz timing.",
                 false);
  if (!display.items.empty()) {
    sections.push_back(std::move(display));
  }

  IOSConfigSection performance;
  performance.title = "Performance";
  performance.footer = "These settings trade stutter, battery usage, and cache size.";
  AddBoolSetting(performance.items, "store_shaders", "Persistent Shader Cache",
                 "Keeps translated shaders and pipelines on disk so later boots stutter "
                 "less. Disable only if you suspect cache corruption after an update.",
                 true);
  AddBoolSetting(performance.items, "async_shader_compilation", "Async Shader Compilation",
                 "Compiles Metal shaders and pipelines in background threads to reduce "
                 "stutter. New effects may appear a moment late; turn this off if you "
                 "prefer blocking correctness over smoother frame pacing.",
                 false);
  if (!performance.items.empty()) {
    sections.push_back(std::move(performance));
  }

  IOSConfigSection audio;
  audio.title = "Audio";
  audio.footer = "These settings control mute state and XMA decoding behavior.";
  AddBoolSetting(audio.items, "mute", "Mute Audio",
                 "Immediately silences all emulator audio. Useful for background testing "
                 "or silent repro runs.",
                 false);
  AddStringChoiceSetting(audio.items, "xma_decoder", "XMA Decoder",
                         "Select the XMA decoder implementation. New is the current general "
                         "default; Old and Master are fallback paths for regressions, and Fake "
                         "disables XMA decode entirely.",
                         "new",
                         {{"New (Recommended)", "new"},
                          {"Old", "old"},
                          {"Master", "master"},
                          {"Fake (No XMA Audio)", "fake"}});
  AddBoolSetting(audio.items, "use_dedicated_xma_thread", "Dedicated XMA Thread",
                 "Runs XMA decode work on a separate thread. On arm64 this is off by "
                 "default; enable only if audio stutters or decode work blocks the title, "
                 "since timing can change.",
                 false);
  if (!audio.items.empty()) {
    sections.push_back(std::move(audio));
  }

  IOSConfigSection compatibility;
  compatibility.title = "Compatibility";
  compatibility.footer = "Leave these at their defaults unless a specific title needs them.";
  AddBoolSetting(compatibility.items, "half_pixel_offset", "Half-Pixel Offset",
                 "D3D9-style sampling behavior. Keep this on for correct post-processing "
                 "and UI in most games; disable only if a specific title shows blurred UI "
                 "or edge artifacts.",
                 true);
  AddBoolSetting(compatibility.items, "gpu_3d_to_2d_texture", "Treat 3D Textures as 2D",
                 "Compatibility workaround for titles that incorrectly sample 3D textures "
                 "as 2D. Keep this on unless it causes a specific regression.",
                 true);
  AddBoolSetting(compatibility.items, "gpu_allow_invalid_fetch_constants",
                 "Allow Invalid Fetch Constants",
                 "Unsafe workaround for titles with broken texture or vertex fetch "
                 "metadata. This can help a game boot or draw, but it may also introduce "
                 "corruption or hide a deeper bug.",
                 true);
  AddBoolSetting(compatibility.items, "mount_cache", "Mount Cache",
                 "Mounts the Xbox cache partition for titles that expect it. Keep "
                 "this on for normal behavior, but disabling it may fix cutscene "
                 "loop issues in games like Halo 3, ODST, Reach, etc.",
                 true);
  AddBoolSetting(compatibility.items, "a64_enable_host_guest_stack_synchronization",
                 "A64 Stack Synchronization",
                 "ARM64-only compatibility path that keeps host and guest stacks "
                 "synchronized across calls. Leave this off unless a game specifically "
                 "needs it to boot or unwind correctly.",
                 false);
  AddBoolSetting(compatibility.items, "ios_jit_brk_prepare_fallback",
                 "External JIT Prepare Fallback",
                 "iOS ARM64 only. If iOS denies JIT page protection changes, ask an "
                 "external broker or helper to prepare the region and retry. This is only "
                 "useful on TXM or broker setups; otherwise it is unnecessary.",
                 true);
  AddBoolSetting(compatibility.items, "ios_jit_brk_use_universal_0xf00d",
                 "Universal 0xF00D JIT Breakpoint",
                 "Use the modern universal BRK command for the external JIT broker. Keep "
                 "this on for current broker scripts; disable it only if you are using an "
                 "older legacy 0x69-only setup. This only matters when External JIT "
                 "Prepare Fallback is enabled.",
                 true);
  if (!compatibility.items.empty()) {
    sections.push_back(std::move(compatibility));
  }

  IOSConfigSection automation;
  automation.title = "Automation";
  automation.footer =
      "These options are stored locally in the iOS frontend rather than xenios.config.toml.";
  AddUserDefaultBoolSetting(
      automation.items, kXeniaAutoOpenStikDebugOnLaunchPreferenceKey,
      "Auto-Enable JIT via StikDebug",
      "On app open, jump into StikDebug with XeniOS's bundle ID so it can enable JIT and "
      "relaunch XeniOS. Requires StikDebug, a valid pairing file, and your normal VPN / loopback "
      "setup.",
      false);
  if (!automation.items.empty()) {
    sections.push_back(std::move(automation));
  }

  IOSConfigSection diagnostics;
  diagnostics.title = "Diagnostics";
  diagnostics.footer = "";
  AddChoiceSetting(diagnostics.items, IOSConfigControlType::kChoiceInt32, "log_level",
                   "Log Verbosity",
                   "Controls how much goes into xenia.log. Higher levels help debug issues "
                   "but increase log size and background overhead.",
                   2, {{"Errors Only", 0}, {"Warnings", 1}, {"Info", 2}, {"Debug", 3}});
  AddActionSetting(diagnostics.items, IOSConfigAction::kViewRecentLog, "View Live Log",
                   "Open a live-updating xenia.log viewer so you can capture boot failures "
                   "without Xcode.");
  if (!diagnostics.items.empty()) {
    sections.push_back(std::move(diagnostics));
  }

  return sections;
}

bool ApplyIOSConfigSections(const std::vector<IOSConfigSection>& sections) {
  bool ok = true;
  for (const IOSConfigSection& section : sections) {
    for (const IOSConfigItem& item : section.items) {
      switch (item.control_type) {
        case IOSConfigControlType::kToggle:
          if (item.storage == IOSConfigStorage::kUserDefaults) {
            SetUserDefaultBool(ToNSString(item.key), item.bool_value);
          } else {
            ok &= SetConfigVarBool(item.key, item.bool_value);
          }
          break;
        case IOSConfigControlType::kChoiceInt32:
          if (item.storage != IOSConfigStorage::kConfigVar) {
            XELOGW("iOS settings: unsupported integer storage for '{}'", item.key);
            ok = false;
            break;
          }
          ok &= SetConfigVarInt32(item.key, static_cast<int32_t>(item.choice_value));
          break;
        case IOSConfigControlType::kChoiceUInt64:
          if (item.storage != IOSConfigStorage::kConfigVar) {
            XELOGW("iOS settings: unsupported uint64 storage for '{}'", item.key);
            ok = false;
            break;
          }
          ok &= SetConfigVarUInt64(item.key, static_cast<uint64_t>(item.choice_value));
          break;
        case IOSConfigControlType::kChoiceString:
          if (item.storage != IOSConfigStorage::kConfigVar) {
            XELOGW("iOS settings: unsupported string storage for '{}'", item.key);
            ok = false;
            break;
          }
          if (item.choice_value < 0 ||
              item.choice_value >= static_cast<int64_t>(item.choice_string_values.size())) {
            XELOGW("iOS settings: invalid string choice index {} for '{}'", item.choice_value,
                   item.key);
            ok = false;
            break;
          }
          ok &= SetConfigVarString(
              item.key, item.choice_string_values[static_cast<size_t>(item.choice_value)]);
          break;
        case IOSConfigControlType::kAction:
          break;
      }
    }
  }
  config::SaveConfig();
  return ok;
}
