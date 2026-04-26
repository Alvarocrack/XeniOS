/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IOS_CONFIG_MODELS_H_
#define XENIA_UI_IOS_CONFIG_MODELS_H_

#include <cstdint>
#include <string>
#include <vector>

// Plain-data view models that the iOS settings sheet
// (XeniaConfigViewController) builds at runtime from xenia cvars and
// NSUserDefaults, and that the choice list sheet
// (XeniaChoiceListViewController) consumes when picking a value.
//
// Kept here in one header so the two sheets and any future config-related
// view controllers reference the same type definitions.

enum class IOSConfigControlType {
  kToggle,
  kChoiceInt32,
  kChoiceUInt64,
  kChoiceString,
  kAction,
};

enum class IOSConfigStorage {
  kConfigVar,
  kUserDefaults,
};

enum class IOSConfigAction {
  kNone,
  kViewRecentLog,
};

struct IOSConfigChoice {
  std::string title;
  int64_t value = 0;
};

struct IOSConfigItem {
  std::string key;
  std::string title;
  std::string subtitle;
  IOSConfigControlType control_type = IOSConfigControlType::kToggle;
  IOSConfigStorage storage = IOSConfigStorage::kConfigVar;
  bool bool_value = false;
  int64_t choice_value = 0;
  std::string string_value;
  IOSConfigAction action = IOSConfigAction::kNone;
  std::vector<IOSConfigChoice> choices;
  std::vector<std::string> choice_string_values;
};

struct IOSConfigSection {
  std::string title;
  std::string footer;
  std::vector<IOSConfigItem> items;
};

#endif  // XENIA_UI_IOS_CONFIG_MODELS_H_
