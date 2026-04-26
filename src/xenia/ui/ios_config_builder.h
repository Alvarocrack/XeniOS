/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IOS_CONFIG_BUILDER_H_
#define XENIA_UI_IOS_CONFIG_BUILDER_H_

#import <UIKit/UIKit.h>

#include <filesystem>
#include <string>
#include <vector>

#include "xenia/ui/ios_config_models.h"

// Bridge between Xenia's cvar / NSUserDefaults state and the iOS settings
// sheet. BuildIOSConfigSections constructs the rows the sheet shows;
// ApplyIOSConfigSections writes them back into the cvar registry / user
// defaults and persists the config on disk.

extern NSString* const kXeniaAutoOpenStikDebugOnLaunchPreferenceKey;
extern NSString* const kXeniaLastAutoStikDebugAttemptTimestampPreferenceKey;

std::vector<IOSConfigSection> BuildIOSConfigSections(void);
bool ApplyIOSConfigSections(const std::vector<IOSConfigSection>& sections);
std::string ChoiceTitleForItem(const IOSConfigItem& item);

// NSUserDefaults helpers re-used by the launcher main view controller.
bool GetUserDefaultBool(NSString* key, bool fallback);
double GetUserDefaultDouble(NSString* key, double fallback);
void SetUserDefaultDouble(NSString* key, double value);

// Pending external-launch path persistence: when the launcher hands off to
// StikDebug to enable JIT, the path of the game that triggered the launch is
// persisted here so that on the way back in we can resume it.
void ClearPendingExternalLaunchPathPreference(void);
void StorePendingExternalLaunchPathPreference(const std::filesystem::path& path);
std::filesystem::path TakePendingExternalLaunchPathPreference(void);

#endif  // XENIA_UI_IOS_CONFIG_BUILDER_H_
