/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IOS_CONFIG_VIEW_CONTROLLER_H_
#define XENIA_UI_IOS_CONFIG_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "xenia/ui/ios_view_helpers.h"

// Settings sheet shown from the launcher and from the in-game overlay. Each
// row maps to either a Xenia cvar or an iOS NSUserDefaults key; rows are
// constructed by ios_config_builder, and Save persists every dirty row back
// through ApplyIOSConfigSections.
@interface XeniaConfigViewController : XESheetTableViewController
@end

#endif  // XENIA_UI_IOS_CONFIG_VIEW_CONTROLLER_H_
