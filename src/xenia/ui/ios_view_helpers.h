/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IOS_VIEW_HELPERS_H_
#define XENIA_UI_IOS_VIEW_HELPERS_H_

#import <UIKit/UIKit.h>

#include <cstdint>
#include <string>

// Shared ObjC-side helpers for the iOS UI module: bridges from std::string
// to NSString, the standard "OK" alert presenter, and sheet base classes
// that own the orientation overrides every Xenia modal sheet shares.

NSString* ToNSString(const std::string& value);

// Title ID → 8-digit hex NSString (zero-padded), uppercase or lowercase.
NSString* XEFormatTitleIDHexUpper(uint32_t title_id);
NSString* XEFormatTitleIDHexLower(uint32_t title_id);

// Presents a one-button "OK" alert. No-ops when `presenter` is nil.
void XEPresentOKAlert(UIViewController* presenter, NSString* title, NSString* message);

// UITableViewController base class with the orientation overrides every
// Xenia sheet shares: rotates freely (no upside-down) and starts in
// whatever orientation the host launcher is currently using.
@interface XESheetTableViewController : UITableViewController
@end

// UIViewController base class with the same orientation overrides as
// XESheetTableViewController.
@interface XESheetViewController : UIViewController
@end

#endif  // XENIA_UI_IOS_VIEW_HELPERS_H_
