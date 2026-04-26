/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_game_tile_cell.h"

@implementation XeniaGameTileCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }

  self.backgroundColor = [UIColor clearColor];
  self.layer.cornerRadius = XeniaRadiusLg;
  self.layer.masksToBounds = NO;
  self.layer.shadowOffset = CGSizeMake(0.0f, 6.0f);
  self.contentView.backgroundColor = [UIColor clearColor];

  self.cardView = [[UIView alloc] init];
  self.cardView.translatesAutoresizingMaskIntoConstraints = NO;
  self.cardView.backgroundColor = [XeniaTheme bgSurface];
  self.cardView.layer.cornerRadius = XeniaRadiusLg;
  self.cardView.layer.borderWidth = 0.5;
  self.cardView.layer.borderColor = [XeniaTheme border].CGColor;
  self.cardView.clipsToBounds = YES;
  [self.contentView addSubview:self.cardView];

  self.iconView = [[UIImageView alloc] init];
  self.iconView.translatesAutoresizingMaskIntoConstraints = NO;
  self.iconView.contentMode = UIViewContentModeScaleAspectFill;
  self.iconView.clipsToBounds = YES;
  self.iconView.backgroundColor = [XeniaTheme bgSurface2];
  [self.cardView addSubview:self.iconView];

  self.titleLabel = [[UILabel alloc] init];
  self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightMedium];
  self.titleLabel.textColor = [XeniaTheme textSecondary];
  self.titleLabel.textAlignment = NSTextAlignmentLeft;
  self.titleLabel.numberOfLines = 2;
  self.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  self.titleLabel.adjustsFontSizeToFitWidth = NO;
  [self.titleLabel setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                                   forAxis:UILayoutConstraintAxisHorizontal];
  [self.titleLabel setContentCompressionResistancePriority:UILayoutPriorityRequired
                                                   forAxis:UILayoutConstraintAxisVertical];
  [self.cardView addSubview:self.titleLabel];

  self.compatPill = [[XeniaPaddedLabel alloc] init];
  self.compatPill.translatesAutoresizingMaskIntoConstraints = NO;
  self.compatPill.padding = UIEdgeInsetsMake(2, 6, 2, 6);
  self.compatPill.textAlignment = NSTextAlignmentCenter;
  self.compatPill.layer.cornerRadius = 6;
  self.compatPill.clipsToBounds = YES;
  self.compatPill.font = [UIFont systemFontOfSize:10 weight:UIFontWeightMedium];
  self.compatPill.hidden = YES;
  [self.compatPill setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisHorizontal];
  [self.compatPill setContentCompressionResistancePriority:UILayoutPriorityRequired
                                                   forAxis:UILayoutConstraintAxisHorizontal];
  [self.cardView addSubview:self.compatPill];

  [NSLayoutConstraint activateConstraints:@[
    [self.cardView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
    [self.cardView.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor],
    [self.cardView.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor],
    [self.cardView.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor],
    [self.iconView.topAnchor constraintEqualToAnchor:self.cardView.topAnchor],
    [self.iconView.leadingAnchor constraintEqualToAnchor:self.cardView.leadingAnchor],
    [self.iconView.trailingAnchor constraintEqualToAnchor:self.cardView.trailingAnchor],
    [self.iconView.heightAnchor constraintEqualToAnchor:self.iconView.widthAnchor
                                             multiplier:300.0 / 219.0],
    [self.titleLabel.topAnchor constraintEqualToAnchor:self.iconView.bottomAnchor],
    [self.titleLabel.bottomAnchor constraintEqualToAnchor:self.cardView.bottomAnchor],
    [self.titleLabel.leadingAnchor constraintEqualToAnchor:self.cardView.leadingAnchor
                                                  constant:8.0],
    [self.titleLabel.trailingAnchor constraintEqualToAnchor:self.compatPill.leadingAnchor
                                                   constant:-4.0],
    [self.compatPill.centerYAnchor constraintEqualToAnchor:self.titleLabel.centerYAnchor],
    [self.compatPill.trailingAnchor constraintEqualToAnchor:self.cardView.trailingAnchor
                                                   constant:-8.0],
  ]];

  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.shadowPath =
      [UIBezierPath bezierPathWithRoundedRect:self.bounds cornerRadius:XeniaRadiusLg].CGPath;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.iconView.image = nil;
  self.titleLabel.text = @"";
  self.compatPill.text = @"";
  self.compatPill.hidden = YES;
  self.cardView.layer.borderWidth = 0.5f;
  self.cardView.layer.borderColor = [XeniaTheme border].CGColor;
  self.controllerFocused = NO;
}

- (void)setControllerFocused:(BOOL)controllerFocused {
  if (_controllerFocused == controllerFocused) {
    return;
  }
  _controllerFocused = controllerFocused;
  [self updateControllerFocusAppearance];
}

- (void)updateControllerFocusAppearance {
  if (self.controllerFocused) {
    self.cardView.layer.borderWidth = 1.5f;
    self.cardView.layer.borderColor = [XeniaTheme accent].CGColor;
    self.titleLabel.textColor = [XeniaTheme textPrimary];
    self.layer.shadowColor = [XeniaTheme accent].CGColor;
    self.layer.shadowOpacity = 0.24f;
    self.layer.shadowRadius = 10.0f;
    self.layer.zPosition = 1.0f;
  } else {
    self.cardView.layer.borderWidth = 0.5f;
    self.cardView.layer.borderColor = [XeniaTheme border].CGColor;
    self.titleLabel.textColor = [XeniaTheme textPrimary];
    self.layer.shadowOpacity = 0.0f;
    self.layer.shadowRadius = 0.0f;
    self.layer.zPosition = 0.0f;
  }
}

@end
