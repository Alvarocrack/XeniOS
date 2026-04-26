/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_config_view_controller.h"

#include <string>
#include <vector>

#include "xenia/base/logging.h"

#import "xenia/ui/ios_choice_list_view_controller.h"
#import "xenia/ui/ios_compat_data.h"
#import "xenia/ui/ios_config_builder.h"
#import "xenia/ui/ios_config_models.h"
#import "xenia/ui/ios_landscape_navigation_controller.h"
#import "xenia/ui/ios_log_view_controller.h"
#import "xenia/ui/ios_system_utils.h"
#import "xenia/ui/ios_theme.h"

namespace {

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

NSString* const kXeniOSWebsiteURL = @"https://xenios.jp";
NSString* const kXeniOSDiscordURL = @"https://discord.gg/QwcTtNKTGf";
NSString* const kXeniOSGitHubURL = @"https://github.com/xenios-jp/XeniOS";
NSString* const kXeniOSKoFiURL = @"https://ko-fi.com/xenios";

typedef NS_ENUM(NSInteger, XeniaConfigFooterLinkTag) {
  kXeniaConfigFooterLinkWebsite = 1,
  kXeniaConfigFooterLinkGitHub = 2,
  kXeniaConfigFooterLinkDiscord = 3,
  kXeniaConfigFooterLinkKoFi = 4,
};

void OpenExternalURLString(NSString* url_string) {
  if (!url_string || url_string.length == 0) {
    return;
  }
  NSURL* url = [NSURL URLWithString:url_string];
  if (!url) {
    XELOGW("iOS: Failed to create URL from string: {}",
           url_string ? [url_string UTF8String] : "(null)");
    return;
  }
  [[UIApplication sharedApplication] openURL:url
      options:@{}
      completionHandler:^(BOOL success) {
        if (!success) {
          XELOGW("iOS: Failed to open external URL: {}", [url_string UTF8String]);
        }
      }];
}

}  // namespace

@implementation XeniaConfigViewController {
  std::vector<IOSConfigSection> sections_;
  BOOL hasPendingChanges_;
  UIBarButtonItem* saveButton_;
}

- (UIView*)restartNoticeHeaderView {
  UIView* container = [[[UIView alloc] initWithFrame:CGRectMake(0, 0, 1, 136)] autorelease];
  container.backgroundColor = [UIColor clearColor];

  UIView* card = [[[UIView alloc] init] autorelease];
  card.translatesAutoresizingMaskIntoConstraints = NO;
  card.backgroundColor = [XeniaTheme bgSurface];
  card.layer.cornerRadius = 12.0;
  card.layer.borderWidth = 0.5;
  card.layer.borderColor = [XeniaTheme border].CGColor;
  [container addSubview:card];

  UILabel* title = [[[UILabel alloc] init] autorelease];
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.backgroundColor = [UIColor clearColor];
  title.text = @"Change Settings Before Launch";
  title.textColor = [XeniaTheme textPrimary];
  title.numberOfLines = 0;
  xe_apply_label_font(title, UIFontTextStyleHeadline, 17.0, UIFontWeightSemibold);
  [card addSubview:title];

  UILabel* body = [[[UILabel alloc] init] autorelease];
  body.translatesAutoresizingMaskIntoConstraints = NO;
  body.backgroundColor = [UIColor clearColor];
  body.text = @"To avoid partial updates, change settings before launching a game. If "
              @"you save changes while a game is already running, fully relaunch "
              @"XeniOS before testing them.";
  body.textColor = [XeniaTheme textSecondary];
  body.numberOfLines = 0;
  xe_apply_label_font(body, UIFontTextStyleBody, 17.0, UIFontWeightRegular);
  [card addSubview:body];

  [NSLayoutConstraint activateConstraints:@[
    [card.topAnchor constraintEqualToAnchor:container.topAnchor constant:8.0],
    [card.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:16.0],
    [card.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-16.0],
    [card.bottomAnchor constraintEqualToAnchor:container.bottomAnchor constant:-4.0],

    [title.topAnchor constraintEqualToAnchor:card.topAnchor constant:14.0],
    [title.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:14.0],
    [title.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-14.0],

    [body.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:6.0],
    [body.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:14.0],
    [body.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-14.0],
    [body.bottomAnchor constraintEqualToAnchor:card.bottomAnchor constant:-14.0],
  ]];

  return container;
}

- (void)updateTableAccessoryLayoutForView:(UIView*)view isHeader:(BOOL)isHeader {
  if (!view) {
    return;
  }
  CGFloat width = CGRectGetWidth(self.tableView.bounds);
  if (width <= 0.0) {
    width = CGRectGetWidth(self.view.bounds);
  }
  if (width <= 0.0 && self.navigationController) {
    width = CGRectGetWidth(self.navigationController.view.bounds);
  }
  if (width <= 0.0) {
    width = CGRectGetWidth(UIScreen.mainScreen.bounds);
  }
  if (width <= 0.0) {
    return;
  }
  CGRect frame = view.frame;
  frame.size.width = width;
  view.frame = frame;
  [view setNeedsLayout];
  [view layoutIfNeeded];
  CGSize fitting_size =
      [view systemLayoutSizeFittingSize:CGSizeMake(width, UILayoutFittingCompressedSize.height)
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
  CGFloat target_height = ceil(fitting_size.height);
  if (target_height <= 0.0) {
    return;
  }
  if (fabs(CGRectGetHeight(view.frame) - target_height) <= 0.5 &&
      fabs(CGRectGetWidth(view.frame) - width) <= 0.5) {
    return;
  }
  frame.size.height = target_height;
  view.frame = frame;
  if (isHeader) {
    self.tableView.tableHeaderView = view;
  } else {
    self.tableView.tableFooterView = view;
  }
}

- (void)updateTableHeaderAndFooterLayout {
  [self updateTableAccessoryLayoutForView:self.tableView.tableHeaderView isHeader:YES];
  [self updateTableAccessoryLayoutForView:self.tableView.tableFooterView isHeader:NO];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Settings";
  self.tableView.backgroundColor = [UIColor systemBackgroundColor];
  self.tableView.separatorInset = UIEdgeInsetsMake(0, 16, 0, 16);
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 132.0;
  if (@available(iOS 15.0, *)) {
    self.tableView.sectionHeaderTopPadding = 0;
  }
  sections_ = BuildIOSConfigSections();
  hasPendingChanges_ = NO;
  self.tableView.tableHeaderView = [self restartNoticeHeaderView];
  self.tableView.tableFooterView = [self versionFooterView];
  [self updateTableHeaderAndFooterLayout];

  self.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                                                    target:self
                                                    action:@selector(cancelTapped:)];
  saveButton_ = [[UIBarButtonItem alloc] initWithTitle:@"Save"
                                                 style:UIBarButtonItemStyleDone
                                                target:self
                                                action:@selector(saveTapped:)];
  saveButton_.enabled = NO;
  self.navigationItem.rightBarButtonItem = saveButton_;
}

- (void)footerLinkTapped:(UIButton*)sender {
  switch ((XeniaConfigFooterLinkTag)sender.tag) {
    case kXeniaConfigFooterLinkWebsite:
      OpenExternalURLString(kXeniOSWebsiteURL);
      break;
    case kXeniaConfigFooterLinkGitHub:
      OpenExternalURLString(kXeniOSGitHubURL);
      break;
    case kXeniaConfigFooterLinkDiscord:
      OpenExternalURLString(kXeniOSDiscordURL);
      break;
    case kXeniaConfigFooterLinkKoFi:
      OpenExternalURLString(kXeniOSKoFiURL);
      break;
    default:
      break;
  }
}

- (UIView*)versionFooterView {
  NSString* footer_text = xe_user_facing_build_label(xe_current_compat_report_build_info());
  const BOOL has_footer_text = footer_text.length > 0;
  NSString* memorial_text = @"XeniOS is one of several apps with dedication keeping the memory of "
                            @"\"Lily\" alive 11/03/2023";
  UIView* footer =
      [[[UIView alloc] initWithFrame:CGRectMake(0, 0, 1, has_footer_text ? 176 : 78)] autorelease];
  footer.backgroundColor = [UIColor clearColor];

  UILabel* links_label = [[[UILabel alloc] init] autorelease];
  links_label.translatesAutoresizingMaskIntoConstraints = NO;
  links_label.backgroundColor = [UIColor clearColor];
  links_label.text = @"Links";
  links_label.textAlignment = NSTextAlignmentCenter;
  links_label.textColor = [XeniaTheme textMuted];
  links_label.numberOfLines = 1;
  xe_apply_label_font(links_label, UIFontTextStyleCaption1, 12.0, UIFontWeightSemibold);
  [footer addSubview:links_label];

  UIStackView* links_row = [[[UIStackView alloc] initWithArrangedSubviews:@[
    xe_make_settings_footer_button(@"SettingsLinkWebsite", @"globe", @"Website",
                                   kXeniaConfigFooterLinkWebsite, NO, self,
                                   @selector(footerLinkTapped:)),
    xe_make_settings_footer_button(
        @"SettingsLinkGitHub", @"chevron.left.forwardslash.chevron.right", @"GitHub",
        kXeniaConfigFooterLinkGitHub, NO, self, @selector(footerLinkTapped:)),
    xe_make_settings_footer_button(@"SettingsLinkDiscord", @"bubble.left.and.bubble.right",
                                   @"Discord", kXeniaConfigFooterLinkDiscord, NO, self,
                                   @selector(footerLinkTapped:)),
    xe_make_settings_footer_button(@"SettingsLinkKoFi", @"cup.and.saucer", @"Ko-fi",
                                   kXeniaConfigFooterLinkKoFi, NO, self,
                                   @selector(footerLinkTapped:)),
  ]] autorelease];
  links_row.translatesAutoresizingMaskIntoConstraints = NO;
  links_row.axis = UILayoutConstraintAxisHorizontal;
  links_row.alignment = UIStackViewAlignmentCenter;
  links_row.distribution = UIStackViewDistributionEqualCentering;
  links_row.spacing = 22.0;
  [footer addSubview:links_row];

  UIView* build_separator = nil;
  UILabel* build_label = nil;
  UILabel* memorial_label = nil;
  if (has_footer_text) {
    build_separator = [[[UIView alloc] init] autorelease];
    build_separator.translatesAutoresizingMaskIntoConstraints = NO;
    build_separator.backgroundColor = [XeniaTheme border];
    [footer addSubview:build_separator];

    build_label = [[[UILabel alloc] init] autorelease];
    build_label.translatesAutoresizingMaskIntoConstraints = NO;
    build_label.backgroundColor = [UIColor clearColor];
    build_label.text = footer_text;
    build_label.textAlignment = NSTextAlignmentCenter;
    build_label.textColor = [XeniaTheme textMuted];
    build_label.numberOfLines = 1;
    xe_apply_label_font(build_label, UIFontTextStyleFootnote, 14.0, UIFontWeightMedium);
    [footer addSubview:build_label];

    memorial_label = [[[UILabel alloc] init] autorelease];
    memorial_label.translatesAutoresizingMaskIntoConstraints = NO;
    memorial_label.backgroundColor = [UIColor clearColor];
    memorial_label.text = memorial_text;
    memorial_label.textAlignment = NSTextAlignmentCenter;
    memorial_label.textColor = [XeniaTheme textSecondary];
    memorial_label.numberOfLines = 0;
    xe_apply_label_font(memorial_label, UIFontTextStyleCaption1, 12.0, UIFontWeightRegular);
    [footer addSubview:memorial_label];
  }

  NSMutableArray<NSLayoutConstraint*>* constraints = [NSMutableArray arrayWithArray:@[
    [links_label.topAnchor constraintEqualToAnchor:footer.topAnchor constant:6],
    [links_label.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:24],
    [links_label.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-24],
    [links_row.topAnchor constraintEqualToAnchor:links_label.bottomAnchor constant:8],
    [links_row.leadingAnchor constraintGreaterThanOrEqualToAnchor:footer.leadingAnchor constant:24],
    [links_row.trailingAnchor constraintLessThanOrEqualToAnchor:footer.trailingAnchor constant:-24],
    [links_row.centerXAnchor constraintEqualToAnchor:footer.centerXAnchor],
  ]];

  if (has_footer_text) {
    [constraints addObjectsFromArray:@[
      [build_separator.topAnchor constraintEqualToAnchor:links_row.bottomAnchor constant:16],
      [build_separator.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:24],
      [build_separator.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-24],
      [build_separator.heightAnchor constraintEqualToConstant:0.5],
      [build_label.topAnchor constraintEqualToAnchor:build_separator.bottomAnchor constant:12],
      [build_label.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:24],
      [build_label.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-24],
      [memorial_label.topAnchor constraintEqualToAnchor:build_label.bottomAnchor constant:8],
      [memorial_label.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:24],
      [memorial_label.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-24],
      [memorial_label.bottomAnchor constraintEqualToAnchor:footer.bottomAnchor constant:-8],
    ]];
  } else {
    [constraints addObject:[links_row.bottomAnchor constraintEqualToAnchor:footer.bottomAnchor
                                                                  constant:-10]];
  }

  [NSLayoutConstraint activateConstraints:constraints];
  return footer;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateTableHeaderAndFooterLayout];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateTableHeaderAndFooterLayout];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self updateTableHeaderAndFooterLayout];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskAllButUpsideDown;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return xe_current_interface_orientation(self.view);
}

- (void)markPendingChanges {
  hasPendingChanges_ = YES;
  saveButton_.enabled = YES;
}

- (IOSConfigItem*)itemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section < 0 || indexPath.section >= static_cast<NSInteger>(sections_.size())) {
    return nullptr;
  }
  IOSConfigSection& section = sections_[indexPath.section];
  if (indexPath.row < 0 || indexPath.row >= static_cast<NSInteger>(section.items.size())) {
    return nullptr;
  }
  return &section.items[indexPath.row];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return static_cast<NSInteger>(sections_.size());
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section {
  if (section < 0 || section >= static_cast<NSInteger>(sections_.size())) {
    return 0;
  }
  return static_cast<NSInteger>(sections_[section].items.size());
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section {
  if (section < 0 || section >= static_cast<NSInteger>(sections_.size())) {
    return nil;
  }
  return ToNSString(sections_[section].title);
}

- (NSString*)tableView:(UITableView*)tableView titleForFooterInSection:(NSInteger)section {
  if (section < 0 || section >= static_cast<NSInteger>(sections_.size())) {
    return nil;
  }
  return sections_[section].footer.empty() ? nil : ToNSString(sections_[section].footer);
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  static NSString* const kCellIdentifier = @"XeniaConfigCell";
  UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:kCellIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:kCellIdentifier];
  }

  IOSConfigItem* item = [self itemAtIndexPath:indexPath];
  if (!item) {
    cell.contentConfiguration = nil;
    cell.accessoryType = UITableViewCellAccessoryNone;
    cell.accessoryView = nil;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    return cell;
  }

  UIListContentConfiguration* content = [UIListContentConfiguration subtitleCellConfiguration];
  content.text = ToNSString(item->title);
  content.textProperties.color = [XeniaTheme textPrimary];
  content.textProperties.numberOfLines = 0;
  content.textProperties.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  content.secondaryTextProperties.color = [XeniaTheme textSecondary];
  content.secondaryTextProperties.numberOfLines = 0;
  content.secondaryTextProperties.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  content.prefersSideBySideTextAndSecondaryText = NO;
  content.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(12.0, 0.0, 12.0, 0.0);

  if (item->control_type == IOSConfigControlType::kToggle) {
    content.secondaryText = ToNSString(item->subtitle);
    UISwitch* toggle = [[[UISwitch alloc] init] autorelease];
    toggle.on = item->bool_value;
    [toggle addTarget:self
                  action:@selector(toggleChanged:)
        forControlEvents:UIControlEventValueChanged];
    cell.contentConfiguration = content;
    cell.accessoryView = toggle;
    cell.accessoryType = UITableViewCellAccessoryNone;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  } else if (item->control_type == IOSConfigControlType::kAction) {
    content.textProperties.color = self.view.tintColor;
    content.secondaryText = ToNSString(item->subtitle);
    cell.contentConfiguration = content;
    cell.accessoryView = nil;
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    cell.selectionStyle = UITableViewCellSelectionStyleDefault;
  } else {
    std::string value_title = ChoiceTitleForItem(*item);
    std::string subtitle = item->subtitle;
    if (!value_title.empty()) {
      content.secondaryText = ToNSString(value_title + " · " + subtitle);
    } else {
      content.secondaryText = ToNSString(subtitle);
    }
    cell.contentConfiguration = content;
    cell.accessoryView = nil;
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    cell.selectionStyle = UITableViewCellSelectionStyleDefault;
  }

  return cell;
}

- (void)toggleChanged:(UISwitch*)sender {
  CGPoint point = [sender convertPoint:CGPointZero toView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:point];
  if (!indexPath) {
    return;
  }
  IOSConfigItem* item = [self itemAtIndexPath:indexPath];
  if (!item || item->control_type != IOSConfigControlType::kToggle) {
    return;
  }
  item->bool_value = sender.isOn;
  [self markPendingChanges];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  IOSConfigItem* item = [self itemAtIndexPath:indexPath];
  if (!item || item->control_type == IOSConfigControlType::kToggle) {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }

  if (item->control_type == IOSConfigControlType::kAction) {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    switch (item->action) {
      case IOSConfigAction::kViewRecentLog: {
        XeniaLogViewController* log_vc = [[XeniaLogViewController alloc] init];
        [self.navigationController pushViewController:log_vc animated:YES];
      } break;
      case IOSConfigAction::kNone:
      default:
        break;
    }
    return;
  }

  XeniaChoiceListViewController* choice_vc = [[XeniaChoiceListViewController alloc]
      initWithTitle:ToNSString(item->title)
           subtitle:ToNSString(item->subtitle)
            choices:item->choices
      selectedValue:item->choice_value
        onSelection:^(int64_t selected_value) {
          item->choice_value = selected_value;
          if (item->control_type == IOSConfigControlType::kChoiceString && selected_value >= 0 &&
              selected_value < static_cast<int64_t>(item->choice_string_values.size())) {
            item->string_value = item->choice_string_values[static_cast<size_t>(selected_value)];
          }
          [self markPendingChanges];
          [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                                withRowAnimation:UITableViewRowAnimationNone];
        }];
  [self.navigationController pushViewController:choice_vc animated:YES];
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)cancelTapped:(id)sender {
  if (!hasPendingChanges_) {
    [self dismissViewControllerAnimated:YES completion:nil];
    return;
  }

  UIAlertController* confirm =
      [UIAlertController alertControllerWithTitle:@"Discard Changes?"
                                          message:@"You have unsaved setting changes."
                                   preferredStyle:UIAlertControllerStyleAlert];
  [confirm addAction:[UIAlertAction actionWithTitle:@"Keep Editing"
                                              style:UIAlertActionStyleCancel
                                            handler:nil]];
  [confirm addAction:[UIAlertAction actionWithTitle:@"Discard"
                                              style:UIAlertActionStyleDestructive
                                            handler:^(__unused UIAlertAction* action) {
                                              [self dismissViewControllerAnimated:YES
                                                                       completion:nil];
                                            }]];
  [self presentViewController:confirm animated:YES completion:nil];
}

- (void)saveTapped:(id)sender {
  BOOL saved = ApplyIOSConfigSections(sections_) ? YES : NO;
  hasPendingChanges_ = NO;
  saveButton_.enabled = NO;

  NSString* title = saved ? @"Settings Saved" : @"Save Completed With Warnings";
  NSString* message = saved ? @"Saved to XeniOS settings.\n\nFor reliable results, change "
                              @"settings before launching a game. If you saved while a game was "
                              @"already running, fully relaunch XeniOS before testing."
                            : @"Some settings could not be applied. Check xenia.log.\n\nAny "
                              @"settings that were saved should be tested after a full XeniOS "
                              @"relaunch, or by changing them before launching a game.";
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"Done"
                                            style:UIAlertActionStyleDefault
                                          handler:^(__unused UIAlertAction* action) {
                                            [self dismissViewControllerAnimated:YES completion:nil];
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
}

@end
