/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IOS_THEME_H_
#define XENIA_UI_IOS_THEME_H_

#import <UIKit/UIKit.h>

// Centralized iOS theme primitives. These mirror the Xenia website tokens
// (see assets/website / apple_theme_tokens) so the UIKit and web surfaces
// stay visually aligned. Keep additions here so view controllers don't grow
// their own private color/font palettes.

// Border radii matching the website's Tailwind scale.
static constexpr CGFloat XeniaRadiusMd = 8.0;
static constexpr CGFloat XeniaRadiusLg = 12.0;
static constexpr CGFloat XeniaRadiusXl = 16.0;

@interface XeniaTheme : NSObject

// Surfaces.
+ (UIColor*)bgPrimary;   // #09090b
+ (UIColor*)bgSurface;   // #18181b
+ (UIColor*)bgSurface2;  // #27272a
+ (UIColor*)bgSurface3;  // #3f3f46

// Text.
+ (UIColor*)textPrimary;    // #fafafa
+ (UIColor*)textSecondary;  // #a1a1aa
+ (UIColor*)textMuted;      // #71717a

// Accent.
+ (UIColor*)accent;       // #34d399
+ (UIColor*)accentHover;  // #6ee7b7
+ (UIColor*)accentFg;     // #09090b

// Status.
+ (UIColor*)statusError;    // #f87171
+ (UIColor*)statusWarning;  // #fbbf24

// Strokes / overlays.
+ (UIColor*)border;        // white 6%
+ (UIColor*)borderHover;   // white 10%
+ (UIColor*)overlay;       // black 85%
+ (UIColor*)overlayLight;  // black 58%

@end

// UILabel that adds an inset around its text. Useful for pill / chip styling.
@interface XeniaPaddedLabel : UILabel
@property(nonatomic) UIEdgeInsets padding;
@end

// Hero artwork → glow color extraction (used by the compatibility hero card).
typedef struct {
  UIColor* primary;
  UIColor* secondary;
} XEHeroGlowPalette;

// Dynamic-type aware font helpers.
UIFont* xe_scaled_system_font(UIFontTextStyle text_style, CGFloat point_size, UIFontWeight weight);
UIFont* xe_scaled_monospaced_font(UIFontTextStyle text_style, CGFloat point_size,
                                  UIFontWeight weight);
void xe_apply_label_font(UILabel* label, UIFontTextStyle text_style, CGFloat point_size,
                         UIFontWeight weight);
void xe_apply_monospaced_label_font(UILabel* label, UIFontTextStyle text_style, CGFloat point_size,
                                    UIFontWeight weight);
void xe_apply_button_title_font(UIButton* button, UIFontTextStyle text_style, CGFloat point_size,
                                UIFontWeight weight);
void xe_apply_text_view_font(UITextView* text_view, UIFontTextStyle text_style, CGFloat point_size,
                             UIFontWeight weight, BOOL monospaced);

// Reusable controls.
XeniaPaddedLabel* xe_make_tag_pill(NSString* text, UIColor* text_color);
UIButton* xe_make_ios_sheet_close_button(id target, SEL action);
UIImage* xe_settings_footer_image(NSString* asset_name, NSString* fallback_symbol_name,
                                  BOOL tintable);
UIButton* xe_make_settings_footer_button(NSString* asset_name, NSString* fallback_symbol_name,
                                         NSString* accessibility_label, NSInteger tag,
                                         BOOL tintable, id target, SEL action);

// Color math.
BOOL xe_color_to_rgb_components(UIColor* color, CGFloat* r, CGFloat* g, CGFloat* b, CGFloat* a);
UIColor* xe_blend_rgb_colors(UIColor* a, UIColor* b, CGFloat amount);
CGFloat xe_color_luma(UIColor* color);

// Hero glow palette.
XEHeroGlowPalette xe_extract_hero_glow_palette(UIImage* image);

#endif  // XENIA_UI_IOS_THEME_H_
