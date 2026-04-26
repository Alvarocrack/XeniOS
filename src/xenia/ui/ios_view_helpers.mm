/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_view_helpers.h"

#import "xenia/ui/ios_system_utils.h"

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

NSString* XEFormatTitleIDHexUpper(uint32_t title_id) {
  return [NSString stringWithFormat:@"%08X", title_id];
}

NSString* XEFormatTitleIDHexLower(uint32_t title_id) {
  return [NSString stringWithFormat:@"%08x", title_id];
}

void XEPresentOKAlert(UIViewController* presenter, NSString* title, NSString* message) {
  if (!presenter) {
    return;
  }
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:title ?: @"Notice"
                                          message:message ?: @""
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [presenter presentViewController:alert animated:YES completion:nil];
}

@implementation XESheetTableViewController

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskAllButUpsideDown;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return xe_current_interface_orientation(self.view);
}

@end

@implementation XESheetViewController

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskAllButUpsideDown;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return xe_current_interface_orientation(self.view);
}

@end
