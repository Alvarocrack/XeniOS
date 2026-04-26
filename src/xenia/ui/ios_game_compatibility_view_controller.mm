/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_game_compatibility_view_controller.h"

#import <SafariServices/SafariServices.h>

#include <cstdint>
#include <string>

#import "xenia/base/logging.h"
#import "xenia/ui/ios_compat_data.h"
#import "xenia/ui/ios_compat_report_view_controller.h"
#import "xenia/ui/ios_game_art.h"
#import "xenia/ui/ios_landscape_navigation_controller.h"
#import "xenia/ui/ios_system_utils.h"
#import "xenia/ui/ios_theme.h"

namespace {

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

constexpr NSInteger kXeniaDiscussionPreviewCount = 3;

}  // namespace

@implementation XeniaGameCompatibilityViewController {
  uint32_t title_id_;
  NSString* game_title_;
  NSDictionary* compat_info_;
  UIImage* hero_artwork_;
  UIImage* hero_background_artwork_;
  CAGradientLayer* hero_background_gradient_layer_;
  CAGradientLayer* hero_wave_layer_a_;
  CAGradientLayer* hero_wave_layer_b_;
  CAGradientLayer* hero_wave_layer_c_;
  UIColor* hero_glow_color_;
  UIColor* hero_glow_secondary_color_;
  UIView* hero_handle_view_;
  UILabel* hero_sheet_title_label_;
  UIButton* hero_close_button_;
  UIStackView* hero_content_stack_;
  UIView* hero_header_view_;
  UIView* hero_background_view_;
  UIView* hero_header_card_view_;
  UIImageView* hero_header_backdrop_view_;
  UIVisualEffectView* hero_header_blur_view_;
  CAGradientLayer* hero_header_scrim_layer_;
  UILabel* hero_title_label_;
  UILabel* hero_tid_label_;
  UIStackView* hero_pills_stack_;
  XeniaPaddedLabel* hero_status_pill_;
  XeniaPaddedLabel* hero_perf_pill_;
  UILabel* hero_updated_label_;
  NSMutableArray<NSDictionary*>* discussion_reports_;
  NSMutableSet<NSNumber*>* discussion_expanded_report_indexes_;
  NSString* discussion_issue_url_;
  NSInteger discussion_issue_number_;
  BOOL discussion_loading_;
  BOOL discussion_show_all_;
  BOOL hero_scroll_layout_initialized_;
}

- (instancetype)initWithTitleID:(uint32_t)title_id
                          title:(NSString*)title
                     compatData:(NSDictionary*)compat_data {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    title_id_ = title_id;
    game_title_ = [title copy];
    hero_glow_color_ = [[XeniaTheme accent] retain];
    hero_glow_secondary_color_ = [[XeniaTheme accentHover] retain];
    compat_info_ = [compat_data retain];
    discussion_reports_ = [[NSMutableArray alloc] init];
    discussion_expanded_report_indexes_ = [[NSMutableSet alloc] init];
    discussion_loading_ = YES;
    discussion_show_all_ = NO;
    discussion_issue_number_ = 0;
    self.title = @"Compatibility";
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [hero_background_view_ removeFromSuperview];
  [game_title_ release];
  [hero_handle_view_ release];
  [hero_sheet_title_label_ release];
  [hero_close_button_ release];
  [hero_content_stack_ release];
  [compat_info_ release];
  [hero_artwork_ release];
  [hero_background_artwork_ release];
  [hero_background_gradient_layer_ release];
  [hero_wave_layer_a_ release];
  [hero_wave_layer_b_ release];
  [hero_wave_layer_c_ release];
  [hero_glow_color_ release];
  [hero_glow_secondary_color_ release];
  [hero_header_view_ release];
  [hero_background_view_ release];
  [hero_header_card_view_ release];
  [hero_header_backdrop_view_ release];
  [hero_header_blur_view_ release];
  [hero_header_scrim_layer_ release];
  [hero_title_label_ release];
  [hero_tid_label_ release];
  [hero_pills_stack_ release];
  [hero_status_pill_ release];
  [hero_perf_pill_ release];
  [hero_updated_label_ release];
  [discussion_reports_ release];
  [discussion_expanded_report_indexes_ release];
  [discussion_issue_url_ release];
  [super dealloc];
}

- (void)setHeroArtwork:(UIImage*)image {
  [hero_artwork_ release];
  // Re-layout so the hero height adapts to the image's aspect ratio.
  if ([self isViewLoaded]) {
    [self layoutHeroHeaderIfNeeded];
  }
  hero_artwork_ = [image retain];
}

- (void)setHeroBackgroundArtwork:(UIImage*)image {
  [hero_background_artwork_ release];
  hero_background_artwork_ = [image retain];
}

- (NSDictionary*)bestResultSource {
  NSDictionary* preferred_summary = xe_preferred_summary_from_compat_info(compat_info_);
  return preferred_summary ?: [self latestDiscussionReport];
}

- (void)updateHeroHeaderArtwork {
  if (!hero_header_backdrop_view_) {
    return;
  }
  UIImage* display = hero_background_artwork_ ?: hero_artwork_;
  hero_header_backdrop_view_.image = display;
  hero_header_backdrop_view_.hidden = (display == nil);
  hero_header_backdrop_view_.layer.contentsRect =
      hero_background_artwork_ ? CGRectMake(0.0, 0.0, 1.0, 1.0) : CGRectMake(0.0, 0.18, 1.0, 0.82);
  hero_header_card_view_.backgroundColor =
      display ? [XeniaTheme bgSurface] : [XeniaTheme bgSurface2];
  [self applyHeroGlowColors];
}

- (void)updateHeroGlowColorFromImage:(UIImage*)image {
  if (!image) return;
  XEHeroGlowPalette palette = xe_extract_hero_glow_palette(image);
  UIColor* primary = palette.primary ?: [XeniaTheme accent];
  UIColor* secondary = palette.secondary ?: [XeniaTheme accentHover];
  [hero_glow_color_ release];
  hero_glow_color_ = [primary retain];
  [hero_glow_secondary_color_ release];
  hero_glow_secondary_color_ = [secondary retain];
  [self applyHeroGlowColors];
}

- (void)applyHeroGlowColors {
  if (!hero_background_gradient_layer_) return;

  UIColor* primary = hero_glow_color_ ?: [XeniaTheme accent];
  UIColor* secondary =
      hero_glow_secondary_color_ ?: xe_blend_rgb_colors(primary, [XeniaTheme accentHover], 0.26);
  UIColor* tertiary = xe_blend_rgb_colors(primary, secondary, 0.34);
  CGFloat avg_luma = xe_color_luma(primary) * 0.70 + xe_color_luma(secondary) * 0.30;
  BOOL has_background_art = (hero_background_artwork_ != nil);
  if (hero_header_backdrop_view_) {
    if (has_background_art) {
      hero_header_backdrop_view_.alpha = MIN(0.96, MAX(0.78, 0.94 - avg_luma * 0.20));
    } else {
      hero_header_backdrop_view_.alpha = MIN(0.88, MAX(0.62, 0.82 - avg_luma * 0.24));
    }
  }
  if (hero_header_blur_view_) {
    if (has_background_art) {
      hero_header_blur_view_.alpha = MIN(0.36, MAX(0.14, 0.14 + avg_luma * 0.14));
    } else {
      hero_header_blur_view_.alpha = MIN(0.60, MAX(0.30, 0.36 + avg_luma * 0.18));
    }
  }
  if (hero_header_scrim_layer_) {
    CGFloat top_alpha = MIN(0.30, MAX(0.10, 0.10 + avg_luma * 0.16));
    CGFloat mid_alpha = MIN(0.60, MAX(0.38, 0.38 + avg_luma * 0.20));
    CGFloat bottom_alpha = MIN(0.92, MAX(0.80, 0.80 + avg_luma * 0.12));
    hero_header_scrim_layer_.colors = @[
      (id)[UIColor colorWithWhite:0.0 alpha:top_alpha].CGColor,
      (id)[UIColor colorWithWhite:0.0 alpha:mid_alpha].CGColor,
      (id)[UIColor colorWithWhite:0.0 alpha:bottom_alpha].CGColor,
    ];
    hero_header_scrim_layer_.locations = @[ @0.0, @0.58, @1.0 ];
  }

  hero_background_gradient_layer_.colors = @[
    (id)[primary colorWithAlphaComponent:0.28].CGColor,
    (id)[secondary colorWithAlphaComponent:0.16].CGColor,
    (id)[tertiary colorWithAlphaComponent:0.08].CGColor,
    (id)[UIColor clearColor].CGColor,
  ];
  hero_background_gradient_layer_.locations = @[ @0.0, @0.30, @0.66, @1.0 ];

  if (hero_wave_layer_a_) {
    hero_wave_layer_a_.colors = @[
      (id)[primary colorWithAlphaComponent:0.24].CGColor,
      (id)[primary colorWithAlphaComponent:0.14].CGColor,
      (id)[primary colorWithAlphaComponent:0.06].CGColor,
      (id)[UIColor clearColor].CGColor,
    ];
  }
  if (hero_wave_layer_b_) {
    hero_wave_layer_b_.colors = @[
      (id)[secondary colorWithAlphaComponent:0.21].CGColor,
      (id)[secondary colorWithAlphaComponent:0.12].CGColor,
      (id)[secondary colorWithAlphaComponent:0.05].CGColor,
      (id)[UIColor clearColor].CGColor,
    ];
  }
  if (hero_wave_layer_c_) {
    UIColor* cloud = xe_blend_rgb_colors(secondary, primary, 0.24);
    hero_wave_layer_c_.colors = @[
      (id)[cloud colorWithAlphaComponent:0.18].CGColor,
      (id)[cloud colorWithAlphaComponent:0.10].CGColor,
      (id)[cloud colorWithAlphaComponent:0.04].CGColor,
      (id)[UIColor clearColor].CGColor,
    ];
  }
}

- (void)layoutHeroHeaderOverlayFrames {
  if (!hero_header_card_view_) return;
  CGRect bounds = hero_header_card_view_.bounds;
  if (CGRectIsEmpty(bounds)) return;
  CGFloat card_w = bounds.size.width;

  // Handle: centered, 60x6, 8pt from top.
  CGFloat handle_w = 60.0, handle_h = 6.0;
  hero_handle_view_.frame = CGRectMake(floor((card_w - handle_w) / 2.0), 8.0, handle_w, handle_h);

  // Sheet title: centered, below handle.
  CGFloat title_y = CGRectGetMaxY(hero_handle_view_.frame) + 16.0;
  CGFloat title_max_w = card_w - 72.0 - 60.0;  // left margin + close button area
  CGSize title_size = [hero_sheet_title_label_ sizeThatFits:CGSizeMake(title_max_w, CGFLOAT_MAX)];
  hero_sheet_title_label_.frame = CGRectMake(floor((card_w - title_size.width) / 2.0), title_y,
                                             ceil(title_size.width), ceil(title_size.height));

  // Close button: 48x48, right-aligned, vertically centered with title.
  CGFloat btn_size = 48.0;
  CGFloat title_center_y = CGRectGetMidY(hero_sheet_title_label_.frame);
  hero_close_button_.frame = CGRectMake(card_w - 16.0 - btn_size,
                                        floor(title_center_y - btn_size / 2.0), btn_size, btn_size);
}

- (void)updateHeroGradientFrames {
  if (!hero_header_card_view_) return;
  CGRect bounds = hero_header_card_view_.bounds;
  if (CGRectIsEmpty(bounds)) return;
  hero_header_scrim_layer_.frame = bounds;
  hero_background_gradient_layer_.frame = bounds;
  CGRect wave_frame = CGRectInset(bounds, -bounds.size.width * 0.44, -bounds.size.height * 0.86);
  wave_frame.origin.y -= bounds.size.height * 0.60;
  hero_wave_layer_a_.frame = wave_frame;
  hero_wave_layer_b_.frame = wave_frame;
  hero_wave_layer_c_.frame = wave_frame;
}

- (void)ensureHeroTopGlowAnimation {
  if (!hero_wave_layer_a_ || !hero_wave_layer_b_ || !hero_wave_layer_c_ ||
      UIAccessibilityIsReduceMotionEnabled()) {
    [hero_wave_layer_a_ removeAllAnimations];
    [hero_wave_layer_b_ removeAllAnimations];
    [hero_wave_layer_c_ removeAllAnimations];
    hero_wave_layer_a_.opacity = 0.08;
    hero_wave_layer_b_.opacity = 0.06;
    hero_wave_layer_c_.opacity = 0.05;
    return;
  }

  NSArray<CAGradientLayer*>* waves =
      @[ hero_wave_layer_a_, hero_wave_layer_b_, hero_wave_layer_c_ ];
  NSArray<NSNumber*>* durations = @[ @19.8, @24.2, @28.4 ];
  NSArray<NSNumber*>* peaks = @[ @0.13, @0.10, @0.08 ];
  NSArray<NSNumber*>* scale_y_to = @[ @1.08, @1.10, @1.07 ];
  NSArray<NSNumber*>* scale_x_to = @[ @1.06, @1.08, @1.05 ];

  for (NSUInteger i = 0; i < waves.count; ++i) {
    CAGradientLayer* wave = waves[i];
    NSString* key = [NSString stringWithFormat:@"xenia.hero.wave.%lu.group", (unsigned long)i];
    if ([wave animationForKey:key]) continue;

    double peak = [peaks[i] doubleValue];
    CAKeyframeAnimation* opacity = [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
    opacity.values =
        @[ @0.02, @(peak * 0.70), @(peak * 0.95), @(peak * 0.50), @0.03, @(peak * 0.82), @0.02 ];
    opacity.keyTimes = @[ @0.0, @0.15, @0.33, @0.51, @0.68, @0.86, @1.0 ];
    opacity.calculationMode = kCAAnimationLinear;

    CAKeyframeAnimation* scale_y = [CAKeyframeAnimation animationWithKeyPath:@"transform.scale.y"];
    scale_y.values = @[ @0.97, @1.01, scale_y_to[i], @1.02, @0.99, @1.03, @0.97 ];
    scale_y.keyTimes = @[ @0.0, @0.18, @0.36, @0.55, @0.71, @0.88, @1.0 ];
    scale_y.calculationMode = kCAAnimationLinear;

    CAKeyframeAnimation* scale_x = [CAKeyframeAnimation animationWithKeyPath:@"transform.scale.x"];
    scale_x.values = @[ @0.98, @1.01, scale_x_to[i], @1.01, @0.99, @1.02, @0.98 ];
    scale_x.keyTimes = @[ @0.0, @0.16, @0.34, @0.52, @0.69, @0.87, @1.0 ];
    scale_x.calculationMode = kCAAnimationLinear;

    CGFloat x_span = wave.bounds.size.width * (0.004 + 0.002 * (CGFloat)i);
    CGFloat y_span = wave.bounds.size.height * (0.003 + 0.002 * (CGFloat)i);
    CGFloat x_jitter = (((CGFloat)arc4random_uniform(180) / 100.0) - 0.90) * x_span;
    CGFloat y_jitter = (((CGFloat)arc4random_uniform(180) / 100.0) - 0.90) * y_span;
    CAKeyframeAnimation* drift_x =
        [CAKeyframeAnimation animationWithKeyPath:@"transform.translation.x"];
    drift_x.values = @[
      @(-x_span * 0.26), @(x_span * 0.18 + x_jitter), @(x_span * 0.34), @(x_span * -0.08),
      @(-x_span * 0.22)
    ];
    drift_x.keyTimes = @[ @0.0, @0.27, @0.52, @0.78, @1.0 ];
    drift_x.calculationMode = kCAAnimationLinear;
    CAKeyframeAnimation* drift_y =
        [CAKeyframeAnimation animationWithKeyPath:@"transform.translation.y"];
    drift_y.values = @[
      @(-y_span * 0.22), @(y_span * 0.20), @(y_span * 0.34 + y_jitter), @(y_span * -0.04),
      @(-y_span * 0.22)
    ];
    drift_y.keyTimes = @[ @0.0, @0.29, @0.58, @0.81, @1.0 ];
    drift_y.calculationMode = kCAAnimationLinear;

    CAAnimationGroup* group = [CAAnimationGroup animation];
    group.animations = @[ opacity, scale_y, scale_x, drift_x, drift_y ];
    group.duration = [durations[i] doubleValue] + ((double)arc4random_uniform(520) / 100.0);
    group.beginTime = CACurrentMediaTime() + ((double)arc4random_uniform(420) / 100.0);
    group.repeatCount = HUGE_VALF;
    group.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
    group.removedOnCompletion = NO;
    [wave addAnimation:group forKey:key];
  }
}

- (void)updateHeroHeaderContent {
  if (!hero_header_view_) {
    return;
  }

  hero_title_label_.text = game_title_.length > 0 ? game_title_ : @"Unknown Title";
  hero_tid_label_.text = title_id_ ? [NSString stringWithFormat:@"Title ID: %08X", title_id_]
                                   : @"No title ID available";

  NSDictionary* summary_source = [self bestResultSource];
  NSString* status = xe_string_from_object(summary_source[@"status"]);
  if (status.length > 0) {
    UIColor* status_color = xe_compat_status_color(status);
    hero_status_pill_.text = xe_compat_status_label(status);
    hero_status_pill_.textColor = status_color;
    hero_status_pill_.backgroundColor = [status_color colorWithAlphaComponent:0.1];
    hero_status_pill_.hidden = NO;
  } else {
    hero_status_pill_.hidden = YES;
  }

  NSString* perf = xe_string_from_object(summary_source[@"perf"]);
  if (perf.length > 0) {
    UIColor* perf_color = xe_compat_perf_color(perf);
    hero_perf_pill_.text = xe_compat_perf_label(perf);
    hero_perf_pill_.textColor = perf_color;
    hero_perf_pill_.backgroundColor = [perf_color colorWithAlphaComponent:0.1];
    hero_perf_pill_.hidden = NO;
  } else {
    hero_perf_pill_.hidden = YES;
  }
  hero_pills_stack_.hidden = hero_status_pill_.hidden && hero_perf_pill_.hidden;

  NSString* updated_at =
      [compat_info_[@"updatedAt"] isKindOfClass:[NSString class]]
          ? compat_info_[@"updatedAt"]
          : ([summary_source[@"date"] isKindOfClass:[NSString class]] ? summary_source[@"date"]
                                                                      : nil);
  if (updated_at.length > 0) {
    hero_updated_label_.text =
        [NSString stringWithFormat:@"Last updated: %@", xe_format_iso_date(updated_at)];
    hero_updated_label_.hidden = NO;
  } else {
    hero_updated_label_.hidden = YES;
  }

  [self updateHeroHeaderArtwork];
}

- (void)layoutHeroHeaderIfNeeded {
  if (!hero_header_view_) {
    return;
  }
  CGFloat width = CGRectGetWidth(self.tableView.bounds);
  if (width <= 0.0) {
    width = CGRectGetWidth(self.view.bounds);
  }
  if (width <= 0.0) {
    return;
  }

  CGFloat height = xe_compat_hero_height_for_width(width, hero_background_artwork_);
  if (!hero_background_view_) {
    hero_background_view_ = [[UIView alloc] initWithFrame:CGRectZero];
    hero_background_view_.backgroundColor = [XeniaTheme bgPrimary];
    hero_background_view_.clipsToBounds = NO;
    [hero_background_view_ addSubview:hero_header_view_];
  }
  UIView* host_view = self.navigationController.view ?: self.view.superview ?: self.view;
  // Push the hero below the dynamic island / status bar safe area.
  // Use the window's safe area rather than the host view's, because the
  // host view's insets can be stale during navigation push/pop transitions.
  CGFloat safe_top = 0.0;
  if (@available(iOS 11.0, *)) {
    UIWindow* window = host_view.window ?: self.view.window;
    safe_top = window ? window.safeAreaInsets.top : host_view.safeAreaInsets.top;
  }
  // Start the hero background at the top of the screen (y=0) so it covers
  // the status bar / dynamic island area, preventing bleed-through during
  // the sheet slide-up animation. The hero_header_view_ inside is offset
  // downward by safe_top so content sits below the dynamic island.
  if (host_view && hero_background_view_.superview != host_view) {
    [hero_background_view_ removeFromSuperview];
    [host_view addSubview:hero_background_view_];
  }
  CGRect table_frame =
      host_view ? [self.view convertRect:self.view.bounds toView:host_view] : self.view.bounds;
  CGFloat hero_top = CGRectGetMinY(table_frame);
  hero_background_view_.frame =
      CGRectMake(CGRectGetMinX(table_frame), hero_top, width, height + safe_top);
  hero_header_view_.frame = CGRectMake(0.0, safe_top, width, height);
  CGFloat desired_top_inset =
      CGRectGetMaxY(hero_background_view_.frame) - CGRectGetMinY(table_frame) + 12.0;
  CGFloat relative_offset = self.tableView.contentOffset.y + self.tableView.contentInset.top;
  if (fabs(self.tableView.contentInset.top - desired_top_inset) > 0.5) {
    UIEdgeInsets content_inset = self.tableView.contentInset;
    content_inset.top = desired_top_inset;
    self.tableView.contentInset = content_inset;
    if (@available(iOS 13.0, *)) {
      UIEdgeInsets vertical_insets = self.tableView.verticalScrollIndicatorInsets;
      vertical_insets.top = desired_top_inset;
      self.tableView.verticalScrollIndicatorInsets = vertical_insets;
    } else {
      UIEdgeInsets indicator_insets = content_inset;
      indicator_insets.top = desired_top_inset;
      self.tableView.scrollIndicatorInsets = indicator_insets;
    }
    self.tableView.contentOffset =
        CGPointMake(self.tableView.contentOffset.x, relative_offset - desired_top_inset);
  }
  if (!hero_scroll_layout_initialized_) {
    self.tableView.contentOffset = CGPointMake(self.tableView.contentOffset.x, -desired_top_inset);
    hero_scroll_layout_initialized_ = YES;
  }
  [hero_header_view_ setNeedsLayout];
  [hero_header_view_ layoutIfNeeded];
  hero_header_scrim_layer_.frame = hero_header_card_view_.bounds;
  [self updateHeroHeaderArtwork];
}

- (void)buildHeroHeaderIfNeeded {
  if (hero_header_view_) {
    [self updateHeroHeaderContent];
    [self layoutHeroHeaderIfNeeded];
    return;
  }

  CGFloat width = CGRectGetWidth(self.tableView.bounds);
  if (width <= 0.0) {
    width = CGRectGetWidth(self.view.bounds);
  }
  if (width <= 0.0) {
    width = UIScreen.mainScreen.bounds.size.width;
  }

  CGFloat height = xe_compat_hero_height_for_width(width, hero_background_artwork_);
  hero_header_view_ = [[UIView alloc] initWithFrame:CGRectMake(0.0, 0.0, width, height)];
  hero_header_view_.backgroundColor = [UIColor clearColor];
  hero_background_view_ = [[UIView alloc] initWithFrame:hero_header_view_.frame];
  hero_background_view_.backgroundColor = [XeniaTheme bgPrimary];
  hero_background_view_.clipsToBounds = NO;
  [hero_background_view_ addSubview:hero_header_view_];

  hero_header_card_view_ = [[UIView alloc] init];
  hero_header_card_view_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_header_card_view_.backgroundColor = [XeniaTheme bgSurface];
  hero_header_card_view_.layer.cornerRadius = 28.0;
  if (@available(iOS 11.0, *)) {
    hero_header_card_view_.layer.maskedCorners = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  }
  hero_header_card_view_.layer.borderWidth = 0.5;
  hero_header_card_view_.layer.borderColor = [XeniaTheme border].CGColor;
  hero_header_card_view_.clipsToBounds = YES;
  [hero_header_view_ addSubview:hero_header_card_view_];

  hero_header_backdrop_view_ = [[UIImageView alloc] init];
  hero_header_backdrop_view_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_header_backdrop_view_.contentMode = UIViewContentModeScaleAspectFill;
  hero_header_backdrop_view_.clipsToBounds = YES;
  [hero_header_card_view_ addSubview:hero_header_backdrop_view_];

  hero_header_blur_view_ = [[UIVisualEffectView alloc]
      initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterialDark]];
  hero_header_blur_view_.translatesAutoresizingMaskIntoConstraints = NO;
  [hero_header_card_view_ addSubview:hero_header_blur_view_];

  hero_header_scrim_layer_ = [[CAGradientLayer layer] retain];
  [hero_header_card_view_.layer addSublayer:hero_header_scrim_layer_];

  // Base glow gradient from the top.
  hero_background_gradient_layer_ = [[CAGradientLayer layer] retain];
  [hero_header_card_view_.layer insertSublayer:hero_background_gradient_layer_
                                         above:hero_header_scrim_layer_];

  // Three layered radial-glow waves.
  hero_wave_layer_a_ = [[CAGradientLayer layer] retain];
  hero_wave_layer_a_.type = kCAGradientLayerRadial;
  hero_wave_layer_a_.locations = @[ @0.00, @0.34, @0.68, @1.00 ];
  hero_wave_layer_a_.startPoint = CGPointMake(0.52, -0.44);
  hero_wave_layer_a_.endPoint = CGPointMake(0.52, 1.00);
  hero_wave_layer_a_.opacity = 0.06;

  hero_wave_layer_b_ = [[CAGradientLayer layer] retain];
  hero_wave_layer_b_.type = kCAGradientLayerRadial;
  hero_wave_layer_b_.locations = @[ @0.00, @0.36, @0.70, @1.00 ];
  hero_wave_layer_b_.startPoint = CGPointMake(0.34, -0.52);
  hero_wave_layer_b_.endPoint = CGPointMake(0.36, 1.00);
  hero_wave_layer_b_.opacity = 0.05;

  hero_wave_layer_c_ = [[CAGradientLayer layer] retain];
  hero_wave_layer_c_.type = kCAGradientLayerRadial;
  hero_wave_layer_c_.locations = @[ @0.00, @0.40, @0.72, @1.00 ];
  hero_wave_layer_c_.startPoint = CGPointMake(0.70, -0.50);
  hero_wave_layer_c_.endPoint = CGPointMake(0.68, 1.00);
  hero_wave_layer_c_.opacity = 0.04;

  [hero_header_card_view_.layer insertSublayer:hero_wave_layer_a_
                                         above:hero_background_gradient_layer_];
  [hero_header_card_view_.layer insertSublayer:hero_wave_layer_b_ above:hero_wave_layer_a_];
  [hero_header_card_view_.layer insertSublayer:hero_wave_layer_c_ above:hero_wave_layer_b_];

  [self applyHeroGlowColors];

  // Handle, sheet title, and close button use manual frames (not auto
  // layout) so they survive hero_background_view_ remove/re-add cycles
  // across navigation push/pop transitions without constraint breakage.
  hero_handle_view_ = [[UIView alloc] init];
  hero_handle_view_.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.34];
  hero_handle_view_.layer.cornerRadius = 3.0;
  [hero_header_card_view_ addSubview:hero_handle_view_];

  hero_sheet_title_label_ = [[UILabel alloc] init];
  hero_sheet_title_label_.text = @"Compatibility";
  hero_sheet_title_label_.textColor = [XeniaTheme textPrimary];
  hero_sheet_title_label_.textAlignment = NSTextAlignmentCenter;
  xe_apply_label_font(hero_sheet_title_label_, UIFontTextStyleTitle2, 18.0, UIFontWeightSemibold);
  [hero_header_card_view_ addSubview:hero_sheet_title_label_];

  hero_close_button_ = [xe_make_ios_sheet_close_button(self, @selector(doneTapped:)) retain];
  // Convert close button from auto layout to manual frame positioning.
  for (NSLayoutConstraint* c in [hero_close_button_.constraints copy]) {
    c.active = NO;
  }
  hero_close_button_.translatesAutoresizingMaskIntoConstraints = YES;
  [hero_header_card_view_ addSubview:hero_close_button_];

  hero_content_stack_ = [[UIStackView alloc] init];
  hero_content_stack_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_content_stack_.axis = UILayoutConstraintAxisVertical;
  hero_content_stack_.spacing = 8.0;
  [hero_header_card_view_ addSubview:hero_content_stack_];

  hero_title_label_ = [[UILabel alloc] init];
  hero_title_label_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_title_label_.textColor = [XeniaTheme textPrimary];
  hero_title_label_.numberOfLines = 2;
  hero_title_label_.lineBreakMode = NSLineBreakByTruncatingTail;
  xe_apply_label_font(hero_title_label_, UIFontTextStyleLargeTitle, 26.0, UIFontWeightBold);
  [hero_content_stack_ addArrangedSubview:hero_title_label_];

  hero_tid_label_ = [[UILabel alloc] init];
  hero_tid_label_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_tid_label_.textColor = [XeniaTheme textSecondary];
  xe_apply_monospaced_label_font(hero_tid_label_, UIFontTextStyleBody, 13.0, UIFontWeightRegular);
  [hero_content_stack_ addArrangedSubview:hero_tid_label_];

  hero_pills_stack_ = [[UIStackView alloc] init];
  hero_pills_stack_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_pills_stack_.axis = UILayoutConstraintAxisHorizontal;
  hero_pills_stack_.spacing = 10.0;
  hero_pills_stack_.alignment = UIStackViewAlignmentCenter;
  [hero_pills_stack_ setContentHuggingPriority:UILayoutPriorityRequired
                                       forAxis:UILayoutConstraintAxisHorizontal];
  [hero_pills_stack_ setContentCompressionResistancePriority:UILayoutPriorityRequired
                                                     forAxis:UILayoutConstraintAxisHorizontal];

  hero_status_pill_ = [[XeniaPaddedLabel alloc] init];
  hero_status_pill_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_status_pill_.padding = UIEdgeInsetsMake(2, 7, 2, 7);
  hero_status_pill_.textAlignment = NSTextAlignmentCenter;
  hero_status_pill_.layer.cornerRadius = 7.0;
  hero_status_pill_.clipsToBounds = YES;
  xe_apply_label_font(hero_status_pill_, UIFontTextStyleCaption1, 11.0, UIFontWeightMedium);
  [hero_status_pill_ setContentHuggingPriority:UILayoutPriorityRequired
                                       forAxis:UILayoutConstraintAxisHorizontal];
  [hero_status_pill_ setContentCompressionResistancePriority:UILayoutPriorityRequired
                                                     forAxis:UILayoutConstraintAxisHorizontal];
  hero_status_pill_.hidden = YES;

  hero_perf_pill_ = [[XeniaPaddedLabel alloc] init];
  hero_perf_pill_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_perf_pill_.padding = UIEdgeInsetsMake(2, 7, 2, 7);
  hero_perf_pill_.textAlignment = NSTextAlignmentCenter;
  hero_perf_pill_.layer.cornerRadius = 7.0;
  hero_perf_pill_.clipsToBounds = YES;
  xe_apply_label_font(hero_perf_pill_, UIFontTextStyleCaption1, 11.0, UIFontWeightMedium);
  [hero_perf_pill_ setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisHorizontal];
  [hero_perf_pill_ setContentCompressionResistancePriority:UILayoutPriorityRequired
                                                   forAxis:UILayoutConstraintAxisHorizontal];
  hero_perf_pill_.hidden = YES;

  [hero_pills_stack_ addArrangedSubview:hero_status_pill_];
  [hero_pills_stack_ addArrangedSubview:hero_perf_pill_];

  UIView* pills_row = [[[UIView alloc] init] autorelease];
  pills_row.translatesAutoresizingMaskIntoConstraints = NO;
  [pills_row addSubview:hero_pills_stack_];
  [NSLayoutConstraint activateConstraints:@[
    [hero_pills_stack_.topAnchor constraintEqualToAnchor:pills_row.topAnchor],
    [hero_pills_stack_.leadingAnchor constraintEqualToAnchor:pills_row.leadingAnchor],
    [hero_pills_stack_.bottomAnchor constraintEqualToAnchor:pills_row.bottomAnchor],
    [hero_pills_stack_.trailingAnchor constraintLessThanOrEqualToAnchor:pills_row.trailingAnchor],
  ]];
  [hero_content_stack_ addArrangedSubview:pills_row];

  hero_updated_label_ = [[UILabel alloc] init];
  hero_updated_label_.translatesAutoresizingMaskIntoConstraints = NO;
  hero_updated_label_.textColor = [XeniaTheme textMuted];
  hero_updated_label_.numberOfLines = 1;
  xe_apply_label_font(hero_updated_label_, UIFontTextStyleFootnote, 12.0, UIFontWeightRegular);
  [hero_content_stack_ addArrangedSubview:hero_updated_label_];

  [NSLayoutConstraint activateConstraints:@[
    [hero_header_card_view_.topAnchor constraintEqualToAnchor:hero_header_view_.topAnchor],
    [hero_header_card_view_.leadingAnchor constraintEqualToAnchor:hero_header_view_.leadingAnchor],
    [hero_header_card_view_.trailingAnchor
        constraintEqualToAnchor:hero_header_view_.trailingAnchor],
    [hero_header_card_view_.bottomAnchor constraintEqualToAnchor:hero_header_view_.bottomAnchor],
    [hero_header_backdrop_view_.topAnchor constraintEqualToAnchor:hero_header_card_view_.topAnchor],
    [hero_header_backdrop_view_.leadingAnchor
        constraintEqualToAnchor:hero_header_card_view_.leadingAnchor],
    [hero_header_backdrop_view_.trailingAnchor
        constraintEqualToAnchor:hero_header_card_view_.trailingAnchor],
    [hero_header_backdrop_view_.bottomAnchor
        constraintEqualToAnchor:hero_header_card_view_.bottomAnchor],
    [hero_header_blur_view_.topAnchor constraintEqualToAnchor:hero_header_card_view_.topAnchor],
    [hero_header_blur_view_.leadingAnchor
        constraintEqualToAnchor:hero_header_card_view_.leadingAnchor],
    [hero_header_blur_view_.trailingAnchor
        constraintEqualToAnchor:hero_header_card_view_.trailingAnchor],
    [hero_header_blur_view_.bottomAnchor
        constraintEqualToAnchor:hero_header_card_view_.bottomAnchor],
    // content_stack uses auto layout only for leading/trailing/bottom.
    // No top constraint — its top position is determined by its content height
    // and the bottom anchor, avoiding any conflict with the manual-frame views above.
    [hero_content_stack_.leadingAnchor constraintEqualToAnchor:hero_header_card_view_.leadingAnchor
                                                      constant:28.0],
    [hero_content_stack_.trailingAnchor
        constraintEqualToAnchor:hero_header_card_view_.trailingAnchor
                       constant:-28.0],
    [hero_content_stack_.bottomAnchor constraintEqualToAnchor:hero_header_card_view_.bottomAnchor
                                                     constant:-26.0],
  ]];

  [hero_header_view_ setNeedsLayout];
  [hero_header_view_ layoutIfNeeded];
  [self layoutHeroHeaderOverlayFrames];
  [self updateHeroHeaderContent];
  [self layoutHeroHeaderIfNeeded];
}

- (void)loadHeroArtwork {
  if (!title_id_) {
    return;
  }

  UIImage* cached_background = xe_cached_game_background_art(title_id_);
  if (cached_background) {
    [self setHeroBackgroundArtwork:cached_background];
    [self updateHeroGlowColorFromImage:cached_background];
  }

  if (!hero_artwork_) {
    UIImage* cached_cover = xe_cached_game_art(title_id_);
    if (cached_cover) {
      [self setHeroArtwork:cached_cover];
    }
  }

  // If we have artwork but no background, use cover art for the glow.
  if (!cached_background && hero_artwork_) {
    [self updateHeroGlowColorFromImage:hero_artwork_];
  }

  if ((cached_background || hero_artwork_) && [self isViewLoaded]) {
    [self updateHeroHeaderContent];
  }

  const uint32_t expected_title_id = title_id_;
  if (!cached_background) {
    xe_fetch_game_background_art(expected_title_id, ^(UIImage* image) {
      if (!image || self->title_id_ != expected_title_id) {
        return;
      }
      [self setHeroBackgroundArtwork:image];
      [self updateHeroGlowColorFromImage:image];
      if ([self isViewLoaded]) {
        [self updateHeroHeaderContent];
      }
    });
  }

  if (!hero_artwork_) {
    xe_fetch_game_art(expected_title_id, ^(UIImage* image) {
      if (!image || self->title_id_ != expected_title_id) {
        return;
      }
      [self setHeroArtwork:image];
      if (!self->hero_background_artwork_) {
        [self updateHeroGlowColorFromImage:image];
      }
      if ([self isViewLoaded]) {
        [self updateHeroHeaderContent];
      }
    });
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [XeniaTheme bgPrimary];
  self.tableView.backgroundColor = [UIColor clearColor];
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 360.0;
  self.tableView.alwaysBounceVertical = YES;
  if (@available(iOS 11.0, *)) {
    self.tableView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
  }
  if (@available(iOS 15.0, *)) {
    self.tableView.sectionHeaderTopPadding = 0;
  }
  self.title = @"";
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(onDiscussionDidUpdate:)
                                               name:kXeniaDiscussionDidUpdateNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(onCompatDataDidUpdate:)
                                               name:kXeniaCompatDataDidUpdateNotification
                                             object:nil];
  [self buildHeroHeaderIfNeeded];
  [self loadHeroArtwork];
  [self loadDiscussionFromCompatibilityData];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  // Add bottom safe area inset so the Submit Report card isn't clipped
  // behind the home indicator.
  UIEdgeInsets insets = self.tableView.contentInset;
  CGFloat safe_bottom = 0.0;
  if (@available(iOS 11.0, *)) {
    safe_bottom = self.view.safeAreaInsets.bottom;
  }
  if (fabs(insets.bottom - safe_bottom) > 0.5) {
    insets.bottom = safe_bottom;
    self.tableView.contentInset = insets;
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self layoutHeroHeaderIfNeeded];

  id<UIViewControllerTransitionCoordinator> coordinator = self.transitionCoordinator;
  if (coordinator && coordinator.interactive) {
    // Interactive swipe-back: don't show hero yet — wait for completion.
    hero_background_view_.hidden = YES;
    [coordinator notifyWhenInteractionChangesUsingBlock:^(
                     id<UIViewControllerTransitionCoordinatorContext> context) {
      if (context.isCancelled) {
        // Swipe was cancelled — keep hidden, viewWillDisappear will handle.
        return;
      }
      // Swipe committed — show hero now.
      [self layoutHeroHeaderIfNeeded];
      [self layoutHeroHeaderOverlayFrames];
      self->hero_background_view_.hidden = NO;
    }];
  } else {
    // Non-interactive transition (back button, programmatic pop):
    // show immediately.
    hero_background_view_.hidden = NO;
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self layoutHeroHeaderIfNeeded];
  [self layoutHeroHeaderOverlayFrames];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  hero_background_view_.hidden = YES;
  [hero_background_view_ removeFromSuperview];
  [self layoutHeroHeaderOverlayFrames];
  [self updateHeroGradientFrames];
  [self ensureHeroTopGlowAnimation];
  [self.navigationController setNavigationBarHidden:NO animated:NO];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self layoutHeroHeaderIfNeeded];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskAllButUpsideDown;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return xe_current_interface_orientation(self.view);
}

- (void)doneTapped:(id)__unused sender {
  hero_background_view_.hidden = YES;
  [hero_background_view_ removeFromSuperview];
  if (self.navigationController.presentingViewController &&
      self.navigationController.viewControllers.firstObject == self) {
    [self.navigationController dismissViewControllerAnimated:YES completion:nil];
    return;
  }
  [self.navigationController setNavigationBarHidden:NO animated:NO];
  [self.navigationController popViewControllerAnimated:YES];
}

- (void)submitReportTapped:(id)__unused sender {
  hero_background_view_.hidden = YES;
  [hero_background_view_ removeFromSuperview];
  XeniaCompatReportViewController* report_controller =
      [[XeniaCompatReportViewController alloc] initWithTitleID:title_id_ title:game_title_];
  [self.navigationController pushViewController:report_controller animated:YES];
  [report_controller release];
}

- (void)onCompatDataDidUpdate:(NSNotification*)notification {
  NSNumber* updated_title_id = notification.userInfo[@"titleId"];
  if (![updated_title_id isKindOfClass:[NSNumber class]] ||
      [updated_title_id unsignedIntValue] != title_id_) {
    return;
  }

  NSDictionary* next_info = xe_dictionary_from_object(notification.userInfo[@"compatInfo"]);
  if (!next_info) {
    NSDictionary* cached_by_title_id = xe_load_cached_compat_data();
    NSString* title_id_string = [NSString stringWithFormat:@"%08X", title_id_];
    next_info = xe_dictionary_from_object(cached_by_title_id[title_id_string]);
  }
  if (!next_info) {
    return;
  }

  [compat_info_ release];
  compat_info_ = [next_info retain];
  [self updateHeroHeaderContent];
  [self.tableView reloadData];
}

- (void)onDiscussionDidUpdate:(NSNotification*)notification {
  NSNumber* updated_title_id = notification.userInfo[@"titleId"];
  if (![updated_title_id isKindOfClass:[NSNumber class]] ||
      [updated_title_id unsignedIntValue] != title_id_) {
    return;
  }

  NSDictionary* snapshot = xe_dictionary_from_object(notification.userInfo[@"discussion"]);
  if (snapshot) {
    [self applyDiscussionJSON:snapshot];
    discussion_loading_ = NO;
    [self.tableView reloadData];
    return;
  }

  discussion_loading_ = YES;
  [self fetchDiscussion];
}

- (NSDictionary*)latestDiscussionReport {
  if (discussion_reports_.count == 0) {
    return nil;
  }
  id report = discussion_reports_.firstObject;
  return [report isKindOfClass:[NSDictionary class]] ? report : nil;
}

- (NSDictionary*)primaryCompatibilitySource {
  if (compat_info_) {
    return compat_info_;
  }
  return [self latestDiscussionReport];
}

- (NSDictionary*)discussionSnapshotFromCompatibilityData {
  if (!compat_info_) {
    return nil;
  }

  NSMutableDictionary* snapshot = [NSMutableDictionary dictionary];
  id reports = compat_info_[@"reports"];
  if ([reports isKindOfClass:[NSArray class]]) {
    snapshot[@"reports"] = reports;
  }

  id issue_url = compat_info_[@"issueUrl"];
  if ([issue_url isKindOfClass:[NSString class]] && [issue_url length] > 0) {
    snapshot[@"issueUrl"] = issue_url;
  }

  id issue_number = compat_info_[@"issueNumber"];
  if ([issue_number isKindOfClass:[NSNumber class]]) {
    snapshot[@"issueNumber"] = issue_number;
  }

  return snapshot.count > 0 ? snapshot : nil;
}

- (BOOL)needsDiscussionNetworkFallback {
  if (!compat_info_) {
    return YES;
  }

  NSArray* reports =
      [compat_info_[@"reports"] isKindOfClass:[NSArray class]] ? compat_info_[@"reports"] : nil;
  BOOL has_reports = reports.count > 0;
  BOOL has_issue_url = [compat_info_[@"issueUrl"] isKindOfClass:[NSString class]] &&
                       [compat_info_[@"issueUrl"] length] > 0;
  BOOL has_issue_number = [compat_info_[@"issueNumber"] isKindOfClass:[NSNumber class]];

  return !has_reports || (!has_issue_url && !has_issue_number);
}

- (void)loadDiscussionFromCompatibilityData {
  NSDictionary* snapshot = [self discussionSnapshotFromCompatibilityData];
  if (snapshot) {
    [self applyDiscussionJSON:snapshot];
    discussion_loading_ = NO;
    [self.tableView reloadData];
  }

  if ([self needsDiscussionNetworkFallback]) {
    discussion_loading_ = YES;
    [self fetchDiscussion];
  } else if (!snapshot) {
    discussion_loading_ = NO;
  }
}

- (void)applyDiscussionJSON:(NSDictionary*)json {
  [discussion_reports_ removeAllObjects];
  [discussion_expanded_report_indexes_ removeAllObjects];

  NSArray* raw_reports = json[@"reports"];
  if ([raw_reports isKindOfClass:[NSArray class]]) {
    for (id item in raw_reports) {
      if ([item isKindOfClass:[NSDictionary class]]) {
        [discussion_reports_ addObject:item];
      }
    }
  }

  [discussion_issue_url_ release];
  discussion_issue_url_ = nil;
  id issue_url = json[@"issueUrl"];
  if ([issue_url isKindOfClass:[NSString class]] && [issue_url length] > 0) {
    discussion_issue_url_ = [issue_url copy];
  }

  discussion_issue_number_ = 0;
  id issue_number = json[@"issueNumber"];
  if ([issue_number isKindOfClass:[NSNumber class]]) {
    discussion_issue_number_ = [issue_number integerValue];
  }

  if ((NSInteger)discussion_reports_.count <= kXeniaDiscussionPreviewCount) {
    discussion_show_all_ = NO;
  }
  if (discussion_reports_.count > 0) {
    [discussion_expanded_report_indexes_ addObject:@0];
  }
  [self updateHeroHeaderContent];
}

- (void)fetchDiscussion {
  NSString* cache_path = xe_discussion_cache_path(title_id_);
  if (discussion_reports_.count == 0) {
    NSData* cached_data = [NSData dataWithContentsOfFile:cache_path];
    if (cached_data.length > 0) {
      NSError* cache_error = nil;
      id cached_json = [NSJSONSerialization JSONObjectWithData:cached_data
                                                       options:0
                                                         error:&cache_error];
      if (!cache_error && [cached_json isKindOfClass:[NSDictionary class]]) {
        [self applyDiscussionJSON:(NSDictionary*)cached_json];
        discussion_loading_ = NO;
        [self.tableView reloadData];
      }
    }

    NSDictionary* cache_attributes =
        [[NSFileManager defaultManager] attributesOfItemAtPath:cache_path error:nil];
    NSDate* cache_modified_date = cache_attributes[NSFileModificationDate];
    if (cache_modified_date && [[NSDate date] timeIntervalSinceDate:cache_modified_date] < 300.0 &&
        !discussion_loading_) {
      return;
    }
  }

  NSString* url_string = [NSString
      stringWithFormat:@"https://xenios-compat-api.xenios.workers.dev/games/%08X/discussion",
                       title_id_];
  NSURL* url = [NSURL URLWithString:url_string];
  if (!url) {
    discussion_loading_ = NO;
    [self.tableView reloadData];
    return;
  }

  NSURLSessionDataTask* task = [[NSURLSession sharedSession]
        dataTaskWithURL:url
      completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
        if (error || data.length == 0) {
          dispatch_async(dispatch_get_main_queue(), ^{
            self->discussion_loading_ = NO;
            [self.tableView reloadData];
          });
          return;
        }

        NSHTTPURLResponse* http_response = (NSHTTPURLResponse*)response;
        if (![http_response isKindOfClass:[NSHTTPURLResponse class]] ||
            http_response.statusCode != 200) {
          dispatch_async(dispatch_get_main_queue(), ^{
            self->discussion_loading_ = NO;
            [self.tableView reloadData];
          });
          return;
        }

        NSError* json_error = nil;
        id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:&json_error];
        if (json_error || ![json isKindOfClass:[NSDictionary class]]) {
          dispatch_async(dispatch_get_main_queue(), ^{
            self->discussion_loading_ = NO;
            [self.tableView reloadData];
          });
          return;
        }

        [data writeToFile:cache_path atomically:YES];
        dispatch_async(dispatch_get_main_queue(), ^{
          self->discussion_loading_ = NO;
          [self applyDiscussionJSON:(NSDictionary*)json];
          [self.tableView reloadData];
        });
      }];
  [task resume];
}

- (void)viewIssueTapped:(id)__unused sender {
  if (!discussion_issue_url_) {
    return;
  }
  NSURL* url = [NSURL URLWithString:discussion_issue_url_];
  if (!url) {
    return;
  }
  [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
}

- (void)toggleDiscussionExpansionTapped:(id)__unused sender {
  discussion_show_all_ = !discussion_show_all_;
  [self.tableView reloadData];
}

- (void)toggleDiscussionReportExpansionTapped:(UIButton*)sender {
  if (!sender) {
    return;
  }
  NSNumber* report_index = [NSNumber numberWithInteger:sender.tag];
  if ([discussion_expanded_report_indexes_ containsObject:report_index]) {
    [discussion_expanded_report_indexes_ removeObject:report_index];
  } else {
    [discussion_expanded_report_indexes_ addObject:report_index];
  }
  NSIndexPath* discussion_path = [NSIndexPath indexPathForRow:0 inSection:0];
  [self.tableView reloadRowsAtIndexPaths:@[ discussion_path ]
                        withRowAnimation:UITableViewRowAnimationFade];
}

- (UIView*)cardViewForCell:(UITableViewCell*)cell {
  UIView* card = [[[UIView alloc] init] autorelease];
  card.translatesAutoresizingMaskIntoConstraints = NO;
  card.backgroundColor = [XeniaTheme bgSurface];
  card.layer.cornerRadius = XeniaRadiusXl;
  card.layer.borderWidth = 0.5;
  card.layer.borderColor = [XeniaTheme border].CGColor;
  [cell.contentView addSubview:card];
  [NSLayoutConstraint activateConstraints:@[
    [card.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor constant:6],
    [card.leadingAnchor constraintEqualToAnchor:cell.contentView.leadingAnchor constant:16],
    [card.trailingAnchor constraintEqualToAnchor:cell.contentView.trailingAnchor constant:-16],
    [card.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor constant:-6],
  ]];
  return card;
}

- (UIView*)buildMetadataPillRowForEntry:(NSDictionary*)entry {
  NSDictionary* build_info = xe_compat_build_info_from_entry(entry);
  if (!build_info) {
    return nil;
  }

  UIStackView* stack = [[[UIStackView alloc] init] autorelease];
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.axis = UILayoutConstraintAxisHorizontal;
  stack.spacing = 8.0;
  stack.alignment = UIStackViewAlignmentLeading;
  stack.distribution = UIStackViewDistributionFillProportionally;

  NSString* channel = xe_string_from_object(build_info[@"channel"]);
  if (channel.length > 0) {
    [stack addArrangedSubview:xe_make_tag_pill(xe_compat_channel_label(channel),
                                               xe_compat_channel_color(channel))];
  }

  NSString* build_label = xe_compat_build_label(build_info);
  if (build_label.length > 0) {
    [stack addArrangedSubview:xe_make_tag_pill(build_label, [XeniaTheme textSecondary])];
  }

  if (stack.arrangedSubviews.count == 0) {
    return nil;
  }

  UIView* row = [[[UIView alloc] init] autorelease];
  row.translatesAutoresizingMaskIntoConstraints = NO;
  [row addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
    [stack.topAnchor constraintEqualToAnchor:row.topAnchor],
    [stack.leadingAnchor constraintEqualToAnchor:row.leadingAnchor],
    [stack.trailingAnchor constraintLessThanOrEqualToAnchor:row.trailingAnchor],
    [stack.bottomAnchor constraintEqualToAnchor:row.bottomAnchor],
  ]];
  return row;
}

- (UIView*)detailsMetricTileWithLabel:(NSString*)label
                                value:(NSString*)value
                           valueColor:(UIColor*)value_color
                          valueIsPill:(BOOL)value_is_pill {
  UIView* tile = [[[UIView alloc] init] autorelease];
  tile.translatesAutoresizingMaskIntoConstraints = NO;
  tile.backgroundColor = [XeniaTheme bgPrimary];
  tile.layer.cornerRadius = XeniaRadiusMd;
  tile.layer.borderWidth = 0.5;
  tile.layer.borderColor = [XeniaTheme border].CGColor;

  UILabel* title = [[[UILabel alloc] init] autorelease];
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.text = label;
  title.font = [UIFont systemFontOfSize:11 weight:UIFontWeightSemibold];
  title.textColor = [XeniaTheme textMuted];
  [tile addSubview:title];

  UIView* value_view = nil;
  if (value_is_pill) {
    value_view = xe_make_tag_pill(value ?: @"Unknown", value_color ?: [XeniaTheme textMuted]);
  } else {
    UILabel* value_label = [[[UILabel alloc] init] autorelease];
    value_label.translatesAutoresizingMaskIntoConstraints = NO;
    value_label.text = value ?: @"Unknown";
    value_label.font = [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
    value_label.textColor = value_color ?: [XeniaTheme textPrimary];
    value_label.numberOfLines = 1;
    value_view = value_label;
  }
  [tile addSubview:value_view];

  [NSLayoutConstraint activateConstraints:@[
    [tile.heightAnchor constraintGreaterThanOrEqualToConstant:86.0],
    [title.topAnchor constraintEqualToAnchor:tile.topAnchor constant:12.0],
    [title.leadingAnchor constraintEqualToAnchor:tile.leadingAnchor constant:12.0],
    [title.trailingAnchor constraintLessThanOrEqualToAnchor:tile.trailingAnchor constant:-12.0],
    [value_view.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:10.0],
    [value_view.leadingAnchor constraintEqualToAnchor:tile.leadingAnchor constant:12.0],
    [value_view.trailingAnchor constraintLessThanOrEqualToAnchor:tile.trailingAnchor
                                                        constant:-12.0],
    [value_view.bottomAnchor constraintLessThanOrEqualToAnchor:tile.bottomAnchor constant:-12.0],
  ]];

  return tile;
}

- (UITableViewCell*)detailsCellForTableView:(UITableView*)__unused tableView {
  UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                  reuseIdentifier:nil] autorelease];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor clearColor];
  cell.contentView.backgroundColor = [UIColor clearColor];

  NSDictionary* summary_source = xe_preferred_summary_from_compat_info(compat_info_);
  NSDictionary* release_summary = xe_release_summary_from_compat_info(compat_info_);
  BOOL using_release_summary = summary_source && summary_source == release_summary;
  NSDictionary* details_source = summary_source ?: [self latestDiscussionReport];

  NSString* status = xe_string_from_object(details_source[@"status"]);
  NSString* status_label = status.length > 0 ? xe_compat_status_label(status) : @"Unknown";
  UIColor* status_color =
      status.length > 0 ? xe_compat_status_color(status) : [XeniaTheme textMuted];

  NSString* report_device = [details_source[@"deviceMachine"] isKindOfClass:[NSString class]]
                                ? details_source[@"deviceMachine"]
                                : details_source[@"device"];
  NSString* device =
      report_device.length > 0 ? xe_device_display_name_for_machine(report_device) : @"Unknown";

  NSString* platform_display =
      xe_platform_display_text(details_source[@"platform"], details_source[@"osVersion"]);
  if (platform_display.length == 0) {
    platform_display = @"Unknown";
  }

  NSString* gpu = [details_source[@"gpuBackend"] isKindOfClass:[NSString class]]
                      ? details_source[@"gpuBackend"]
                      : nil;
  if (gpu.length == 0) {
    gpu = @"Unknown";
  } else {
    gpu = [gpu uppercaseString];
  }

  NSString* based_on_date =
      [details_source[@"date"] isKindOfClass:[NSString class]] ? details_source[@"date"] : nil;
  if (based_on_date.length == 0 && [compat_info_[@"updatedAt"] isKindOfClass:[NSString class]]) {
    based_on_date = compat_info_[@"updatedAt"];
  }
  NSString* footnote = @"Based on available compatibility data.";
  if (using_release_summary && [status isEqualToString:@"untested"]) {
    footnote =
        @"No official release reports yet. Preview or self-built reports may still appear below.";
  } else if (using_release_summary && based_on_date.length > 0) {
    footnote = [NSString stringWithFormat:@"Based on the latest official release summary from %@.",
                                          xe_format_iso_date(based_on_date)];
  } else if (using_release_summary) {
    footnote = @"Based on the current official release summary.";
  } else if (based_on_date.length > 0) {
    footnote = [NSString
        stringWithFormat:@"Based on the latest report from %@.", xe_format_iso_date(based_on_date)];
  }

  UIView* card = [self cardViewForCell:cell];

  UILabel* heading = [[[UILabel alloc] init] autorelease];
  heading.translatesAutoresizingMaskIntoConstraints = NO;
  heading.text = @"Details";
  heading.font = [UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
  heading.textColor = [XeniaTheme textPrimary];
  [card addSubview:heading];

  UILabel* subheading = [[[UILabel alloc] init] autorelease];
  subheading.translatesAutoresizingMaskIntoConstraints = NO;
  subheading.text = using_release_summary
                        ? @"RELEASE SUMMARY"
                        : (summary_source ? @"CURRENT SUMMARY" : @"LATEST REPORT");
  subheading.font = [UIFont systemFontOfSize:12 weight:UIFontWeightSemibold];
  subheading.textColor = [XeniaTheme textMuted];
  [card addSubview:subheading];

  NSDictionary* build_entry = details_source;
  if (!xe_compat_build_info_from_entry(build_entry) && compat_info_ &&
      build_entry != compat_info_) {
    build_entry = compat_info_;
  }
  UIView* build_row = [self buildMetadataPillRowForEntry:build_entry];
  if (build_row) {
    [card addSubview:build_row];
  }

  UIStackView* grid = [[[UIStackView alloc] init] autorelease];
  grid.translatesAutoresizingMaskIntoConstraints = NO;
  grid.axis = UILayoutConstraintAxisVertical;
  grid.spacing = 10.0;
  [card addSubview:grid];

  UIStackView* row_one = [[[UIStackView alloc] init] autorelease];
  row_one.axis = UILayoutConstraintAxisHorizontal;
  row_one.spacing = 10.0;
  row_one.distribution = UIStackViewDistributionFillEqually;
  [grid addArrangedSubview:row_one];

  UIStackView* row_two = [[[UIStackView alloc] init] autorelease];
  row_two.axis = UILayoutConstraintAxisHorizontal;
  row_two.spacing = 10.0;
  row_two.distribution = UIStackViewDistributionFillEqually;
  [grid addArrangedSubview:row_two];

  [row_one addArrangedSubview:[self detailsMetricTileWithLabel:@"STATUS"
                                                         value:status_label
                                                    valueColor:status_color
                                                   valueIsPill:YES]];
  [row_one addArrangedSubview:[self detailsMetricTileWithLabel:@"DEVICE"
                                                         value:device
                                                    valueColor:[XeniaTheme textPrimary]
                                                   valueIsPill:NO]];
  [row_two addArrangedSubview:[self detailsMetricTileWithLabel:@"PLATFORM"
                                                         value:platform_display
                                                    valueColor:[XeniaTheme textPrimary]
                                                   valueIsPill:NO]];
  [row_two addArrangedSubview:[self detailsMetricTileWithLabel:@"GPU"
                                                         value:gpu
                                                    valueColor:[XeniaTheme textPrimary]
                                                   valueIsPill:NO]];

  UILabel* footer = [[[UILabel alloc] init] autorelease];
  footer.translatesAutoresizingMaskIntoConstraints = NO;
  footer.text = footnote;
  footer.font = [UIFont systemFontOfSize:12];
  footer.textColor = [XeniaTheme textSecondary];
  footer.numberOfLines = 0;
  [card addSubview:footer];

  NSMutableArray<NSLayoutConstraint*>* constraints = [NSMutableArray arrayWithArray:@[
    [heading.topAnchor constraintEqualToAnchor:card.topAnchor constant:16.0],
    [heading.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16.0],
    [heading.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16.0],
    [subheading.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:12.0],
    [subheading.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16.0],
    [subheading.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16.0],
    [grid.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16.0],
    [grid.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16.0],
    [footer.topAnchor constraintEqualToAnchor:grid.bottomAnchor constant:12.0],
    [footer.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16.0],
    [footer.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16.0],
    [footer.bottomAnchor constraintEqualToAnchor:card.bottomAnchor constant:-14.0],
  ]];
  if (build_row) {
    [constraints addObjectsFromArray:@[
      [build_row.topAnchor constraintEqualToAnchor:subheading.bottomAnchor constant:8.0],
      [build_row.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16.0],
      [build_row.trailingAnchor constraintLessThanOrEqualToAnchor:card.trailingAnchor
                                                         constant:-16.0],
      [grid.topAnchor constraintEqualToAnchor:build_row.bottomAnchor constant:10.0],
    ]];
  } else {
    [constraints addObject:[grid.topAnchor constraintEqualToAnchor:subheading.bottomAnchor
                                                          constant:10.0]];
  }
  [NSLayoutConstraint activateConstraints:constraints];

  return cell;
}

- (UITableViewCell*)ctaCellForTableView:(UITableView*)__unused tableView {
  UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                  reuseIdentifier:nil] autorelease];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor clearColor];
  cell.contentView.backgroundColor = [UIColor clearColor];

  UIView* card = [[[UIView alloc] init] autorelease];
  card.translatesAutoresizingMaskIntoConstraints = NO;
  card.backgroundColor = [[XeniaTheme accent] colorWithAlphaComponent:0.04];
  card.layer.cornerRadius = XeniaRadiusXl;
  card.layer.borderWidth = 1.0;
  card.layer.borderColor = [[XeniaTheme accent] colorWithAlphaComponent:0.20].CGColor;
  card.clipsToBounds = YES;
  [cell.contentView addSubview:card];

  UILabel* heading = [[[UILabel alloc] init] autorelease];
  heading.translatesAutoresizingMaskIntoConstraints = NO;
  heading.text = @"Tested this game?";
  heading.font = [UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
  heading.textColor = [XeniaTheme textPrimary];
  heading.textAlignment = NSTextAlignmentCenter;
  [card addSubview:heading];

  UILabel* subtext = [[[UILabel alloc] init] autorelease];
  subtext.translatesAutoresizingMaskIntoConstraints = NO;
  subtext.text = @"Help the community by sharing how well this title runs on your device.";
  subtext.font = [UIFont systemFontOfSize:14];
  subtext.textColor = [XeniaTheme textSecondary];
  subtext.numberOfLines = 0;
  subtext.textAlignment = NSTextAlignmentCenter;
  [card addSubview:subtext];

  UIButton* submit_button = [UIButton buttonWithType:UIButtonTypeSystem];
  submit_button.translatesAutoresizingMaskIntoConstraints = NO;
  [submit_button setTitle:@"Submit Report" forState:UIControlStateNormal];
  submit_button.titleLabel.font = [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
  [submit_button setTitleColor:[XeniaTheme accentFg] forState:UIControlStateNormal];
  submit_button.backgroundColor = [XeniaTheme accent];
  submit_button.layer.cornerRadius = XeniaRadiusMd;
  submit_button.clipsToBounds = YES;
  [submit_button addTarget:self
                    action:@selector(submitReportTapped:)
          forControlEvents:UIControlEventTouchUpInside];
  [card addSubview:submit_button];

  [NSLayoutConstraint activateConstraints:@[
    [card.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor constant:6.0],
    [card.leadingAnchor constraintEqualToAnchor:cell.contentView.leadingAnchor constant:16.0],
    [card.trailingAnchor constraintEqualToAnchor:cell.contentView.trailingAnchor constant:-16.0],
    [card.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor constant:-6.0],
    [heading.topAnchor constraintEqualToAnchor:card.topAnchor constant:18.0],
    [heading.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16.0],
    [heading.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16.0],
    [subtext.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:8.0],
    [subtext.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:24.0],
    [subtext.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-24.0],
    [submit_button.topAnchor constraintEqualToAnchor:subtext.bottomAnchor constant:16.0],
    [submit_button.centerXAnchor constraintEqualToAnchor:card.centerXAnchor],
    [submit_button.heightAnchor constraintEqualToConstant:44.0],
    [submit_button.widthAnchor constraintGreaterThanOrEqualToConstant:164.0],
    [submit_button.leadingAnchor constraintGreaterThanOrEqualToAnchor:card.leadingAnchor
                                                             constant:16.0],
    [submit_button.trailingAnchor constraintLessThanOrEqualToAnchor:card.trailingAnchor
                                                           constant:-16.0],
    [submit_button.bottomAnchor constraintEqualToAnchor:card.bottomAnchor constant:-18.0],
  ]];

  return cell;
}

- (UIView*)discussionPreviewCardForReport:(NSDictionary*)report
                              reportIndex:(NSInteger)report_index {
  NSString* author = [report[@"submittedBy"] isKindOfClass:[NSString class]]
                         ? report[@"submittedBy"]
                         : @"anonymous";
  NSString* notes = [report[@"notes"] isKindOfClass:[NSString class]] ? report[@"notes"] : @"";
  NSString* date_string = [report[@"date"] isKindOfClass:[NSString class]] ? report[@"date"] : @"";
  NSString* formatted_date =
      date_string.length >= 10 ? xe_format_iso_date(date_string) : date_string;

  if (![notes isKindOfClass:[NSString class]] || notes.length == 0) {
    notes = @"No details provided.";
  }

  NSMutableArray<NSString*>* info_parts = [NSMutableArray array];
  NSString* status = [report[@"status"] isKindOfClass:[NSString class]] ? report[@"status"] : nil;
  if (status.length > 0) {
    [info_parts addObject:xe_compat_status_label(status)];
  }
  NSString* report_device = [report[@"deviceMachine"] isKindOfClass:[NSString class]]
                                ? report[@"deviceMachine"]
                                : report[@"device"];
  if ([report_device isKindOfClass:[NSString class]] && report_device.length > 0) {
    [info_parts addObject:xe_device_display_name_for_machine(report_device)];
  }
  NSString* platform_display = xe_platform_display_text(report[@"platform"], report[@"osVersion"]);
  if (platform_display.length > 0) {
    [info_parts addObject:platform_display];
  }
  NSString* gpu_backend =
      [report[@"gpuBackend"] isKindOfClass:[NSString class]] ? report[@"gpuBackend"] : nil;
  if (gpu_backend.length > 0) {
    [info_parts addObject:[gpu_backend uppercaseString]];
  }
  NSString* info_text = [info_parts componentsJoinedByString:@" \u00B7 "];

  UIView* card = [[[UIView alloc] init] autorelease];
  card.translatesAutoresizingMaskIntoConstraints = NO;
  card.backgroundColor = [XeniaTheme bgPrimary];
  card.layer.cornerRadius = XeniaRadiusMd;
  card.layer.borderWidth = 0.5;
  card.layer.borderColor = [XeniaTheme border].CGColor;

  UILabel* author_label = [[[UILabel alloc] init] autorelease];
  author_label.translatesAutoresizingMaskIntoConstraints = NO;
  author_label.text = author.length > 0 ? author : @"anonymous";
  author_label.font = [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
  author_label.textColor = [XeniaTheme textPrimary];
  [card addSubview:author_label];

  UILabel* date_label = [[[UILabel alloc] init] autorelease];
  date_label.translatesAutoresizingMaskIntoConstraints = NO;
  date_label.text = formatted_date;
  date_label.font = [UIFont systemFontOfSize:13];
  date_label.textColor = [XeniaTheme textMuted];
  date_label.textAlignment = NSTextAlignmentRight;
  [date_label setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [date_label setContentCompressionResistancePriority:UILayoutPriorityRequired
                                              forAxis:UILayoutConstraintAxisHorizontal];
  [card addSubview:date_label];

  UIView* build_row = [self buildMetadataPillRowForEntry:report];
  if (build_row) {
    [card addSubview:build_row];
  }

  UILabel* info_label = [[[UILabel alloc] init] autorelease];
  info_label.translatesAutoresizingMaskIntoConstraints = NO;
  info_label.text = info_text;
  info_label.font = [UIFont systemFontOfSize:13];
  info_label.textColor = [XeniaTheme textMuted];
  info_label.numberOfLines = 2;
  info_label.hidden = info_label.text.length == 0;
  [card addSubview:info_label];

  UILabel* notes_label = [[[UILabel alloc] init] autorelease];
  notes_label.translatesAutoresizingMaskIntoConstraints = NO;
  notes_label.text = notes;
  notes_label.font = [UIFont systemFontOfSize:15];
  notes_label.textColor = [XeniaTheme textSecondary];
  BOOL report_expanded = [discussion_expanded_report_indexes_
      containsObject:[NSNumber numberWithInteger:report_index]];
  notes_label.numberOfLines = report_expanded ? 0 : 3;
  notes_label.lineBreakMode =
      report_expanded ? NSLineBreakByWordWrapping : NSLineBreakByTruncatingTail;
  [card addSubview:notes_label];

  BOOL can_expand_notes = notes.length > 170 || [notes rangeOfString:@"\n"].location != NSNotFound;
  UIButton* expand_notes_button = [UIButton buttonWithType:UIButtonTypeSystem];
  expand_notes_button.translatesAutoresizingMaskIntoConstraints = NO;
  [expand_notes_button setTitle:(report_expanded ? @"Show less" : @"Show more")
                       forState:UIControlStateNormal];
  [expand_notes_button setTitleColor:[XeniaTheme accent] forState:UIControlStateNormal];
  expand_notes_button.titleLabel.font = [UIFont systemFontOfSize:12 weight:UIFontWeightSemibold];
  expand_notes_button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
  expand_notes_button.tag = report_index;
  expand_notes_button.hidden = !can_expand_notes;
  [expand_notes_button addTarget:self
                          action:@selector(toggleDiscussionReportExpansionTapped:)
                forControlEvents:UIControlEventTouchUpInside];
  [card addSubview:expand_notes_button];

  UIButton* open_comment_button = [UIButton buttonWithType:UIButtonTypeSystem];
  open_comment_button.translatesAutoresizingMaskIntoConstraints = NO;
  [open_comment_button setTitle:@"Open comment" forState:UIControlStateNormal];
  [open_comment_button setTitleColor:[XeniaTheme accent] forState:UIControlStateNormal];
  open_comment_button.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold];
  open_comment_button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
  open_comment_button.hidden = (discussion_issue_url_ == nil);
  [open_comment_button addTarget:self
                          action:@selector(viewIssueTapped:)
                forControlEvents:UIControlEventTouchUpInside];
  [card addSubview:open_comment_button];

  NSMutableArray<NSLayoutConstraint*>* constraints = [NSMutableArray arrayWithArray:@[
    [author_label.topAnchor constraintEqualToAnchor:card.topAnchor constant:12],
    [author_label.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:10],
    [date_label.firstBaselineAnchor constraintEqualToAnchor:author_label.firstBaselineAnchor],
    [date_label.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-10],
    [date_label.leadingAnchor constraintGreaterThanOrEqualToAnchor:author_label.trailingAnchor
                                                          constant:8],
    [notes_label.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:10],
    [notes_label.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-10],
    [expand_notes_button.topAnchor constraintEqualToAnchor:notes_label.bottomAnchor constant:4],
    [expand_notes_button.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:10],
    [expand_notes_button.trailingAnchor constraintLessThanOrEqualToAnchor:card.trailingAnchor
                                                                 constant:-10],
    [info_label.topAnchor constraintEqualToAnchor:expand_notes_button.bottomAnchor constant:6],
    [info_label.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:10],
    [info_label.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-10],
    [open_comment_button.topAnchor constraintEqualToAnchor:info_label.bottomAnchor constant:8],
    [open_comment_button.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:10],
    [open_comment_button.trailingAnchor constraintLessThanOrEqualToAnchor:card.trailingAnchor
                                                                 constant:-10],
    [open_comment_button.bottomAnchor constraintEqualToAnchor:card.bottomAnchor constant:-10],
  ]];
  if (build_row) {
    [constraints addObjectsFromArray:@[
      [build_row.topAnchor constraintEqualToAnchor:author_label.bottomAnchor constant:8],
      [build_row.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:10],
      [build_row.trailingAnchor constraintLessThanOrEqualToAnchor:card.trailingAnchor constant:-10],
      [notes_label.topAnchor constraintEqualToAnchor:build_row.bottomAnchor constant:8],
    ]];
  } else {
    [constraints addObject:[notes_label.topAnchor constraintEqualToAnchor:author_label.bottomAnchor
                                                                 constant:6]];
  }
  [NSLayoutConstraint activateConstraints:constraints];
  if (expand_notes_button.hidden) {
    [expand_notes_button.heightAnchor constraintEqualToConstant:0].active = YES;
  }
  if (info_label.hidden) {
    [info_label.heightAnchor constraintEqualToConstant:0].active = YES;
  }
  if (open_comment_button.hidden) {
    [open_comment_button.heightAnchor constraintEqualToConstant:0].active = YES;
  }

  return card;
}

- (UITableViewCell*)discussionPreviewCellForTableView:(UITableView*)__unused tableView {
  UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                  reuseIdentifier:nil] autorelease];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor clearColor];
  cell.contentView.backgroundColor = [UIColor clearColor];

  UIView* card = [self cardViewForCell:cell];
  UIStackView* stack = [[[UIStackView alloc] init] autorelease];
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = 12.0;
  [card addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
    [stack.topAnchor constraintEqualToAnchor:card.topAnchor constant:16],
    [stack.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16],
    [stack.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16],
    [stack.bottomAnchor constraintEqualToAnchor:card.bottomAnchor constant:-16],
  ]];

  UIView* heading_row = [[[UIView alloc] init] autorelease];
  heading_row.translatesAutoresizingMaskIntoConstraints = NO;
  heading_row.backgroundColor = [XeniaTheme bgSurface];
  UILabel* heading_label = [[[UILabel alloc] init] autorelease];
  heading_label.translatesAutoresizingMaskIntoConstraints = NO;
  heading_label.text = @"Discussion";
  heading_label.font = [UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
  heading_label.textColor = [XeniaTheme textPrimary];
  [heading_row addSubview:heading_label];

  UIButton* heading_button = [UIButton buttonWithType:UIButtonTypeSystem];
  heading_button.translatesAutoresizingMaskIntoConstraints = NO;
  NSString* button_title =
      discussion_issue_number_ > 0
          ? [NSString stringWithFormat:@"View Issue #%ld", (long)discussion_issue_number_]
          : @"View on GitHub";
  [heading_button setTitle:button_title forState:UIControlStateNormal];
  [heading_button setTitleColor:[XeniaTheme accent] forState:UIControlStateNormal];
  heading_button.titleLabel.font = [UIFont systemFontOfSize:14 weight:UIFontWeightMedium];
  heading_button.hidden = (discussion_issue_url_ == nil);
  [heading_button addTarget:self
                     action:@selector(viewIssueTapped:)
           forControlEvents:UIControlEventTouchUpInside];
  [heading_row addSubview:heading_button];

  [NSLayoutConstraint activateConstraints:@[
    [heading_label.topAnchor constraintEqualToAnchor:heading_row.topAnchor],
    [heading_label.leadingAnchor constraintEqualToAnchor:heading_row.leadingAnchor],
    [heading_label.bottomAnchor constraintEqualToAnchor:heading_row.bottomAnchor],
    [heading_button.firstBaselineAnchor constraintEqualToAnchor:heading_label.firstBaselineAnchor],
    [heading_button.trailingAnchor constraintEqualToAnchor:heading_row.trailingAnchor],
    [heading_button.leadingAnchor constraintGreaterThanOrEqualToAnchor:heading_label.trailingAnchor
                                                              constant:8],
  ]];
  [stack addArrangedSubview:heading_row];

  if (discussion_loading_) {
    UILabel* loading_label = [[[UILabel alloc] init] autorelease];
    loading_label.text = @"Loading discussion...";
    loading_label.font = [UIFont systemFontOfSize:15];
    loading_label.textColor = [XeniaTheme textMuted];
    loading_label.numberOfLines = 1;
    [stack addArrangedSubview:loading_label];
    return cell;
  }

  if (discussion_reports_.count == 0) {
    UILabel* empty_label = [[[UILabel alloc] init] autorelease];
    empty_label.text = @"No reports yet. Be the first to submit one.";
    empty_label.font = [UIFont systemFontOfSize:15];
    empty_label.textColor = [XeniaTheme textMuted];
    empty_label.numberOfLines = 0;
    [stack addArrangedSubview:empty_label];
    return cell;
  }

  NSInteger report_count = (NSInteger)discussion_reports_.count;
  NSInteger visible_count =
      discussion_show_all_ ? report_count : MIN(report_count, kXeniaDiscussionPreviewCount);
  for (NSInteger report_index = 0; report_index < visible_count; ++report_index) {
    NSDictionary* report = discussion_reports_[report_index];
    if (![report isKindOfClass:[NSDictionary class]]) {
      continue;
    }
    [stack addArrangedSubview:[self discussionPreviewCardForReport:report
                                                       reportIndex:report_index]];
  }

  if (report_count > kXeniaDiscussionPreviewCount) {
    UILabel* summary_label = [[[UILabel alloc] init] autorelease];
    summary_label.text =
        discussion_show_all_
            ? [NSString stringWithFormat:@"Showing all %ld reports.", (long)report_count]
            : [NSString stringWithFormat:@"Showing latest %ld of %ld reports.", (long)visible_count,
                                         (long)report_count];
    summary_label.font = [UIFont systemFontOfSize:12];
    summary_label.textColor = [XeniaTheme textMuted];
    summary_label.numberOfLines = 1;
    [stack addArrangedSubview:summary_label];

    UIButton* toggle_button = [UIButton buttonWithType:UIButtonTypeSystem];
    toggle_button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
    NSString* toggle_title =
        discussion_show_all_
            ? @"Show fewer reports"
            : [NSString stringWithFormat:@"Show all %ld reports", (long)report_count];
    [toggle_button setTitle:toggle_title forState:UIControlStateNormal];
    [toggle_button setTitleColor:[XeniaTheme accent] forState:UIControlStateNormal];
    toggle_button.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold];
    [toggle_button addTarget:self
                      action:@selector(toggleDiscussionExpansionTapped:)
            forControlEvents:UIControlEventTouchUpInside];
    [stack addArrangedSubview:toggle_button];
  }

  return cell;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView* __unused)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView* __unused)tableView
    numberOfRowsInSection:(NSInteger)__unused section {
  return 3;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.row == 0) {
    return [self discussionPreviewCellForTableView:tableView];
  }
  if (indexPath.row == 1) {
    return [self detailsCellForTableView:tableView];
  }
  return [self ctaCellForTableView:tableView];
}

@end
