/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_metal_view.h"

#import <MetalKit/MetalKit.h>

@implementation XeniaMetalView

+ (Class)layerClass {
  return [CAMetalLayer class];
}

@end
