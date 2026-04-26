/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_choice_list_view_controller.h"

#include <string>

#import "xenia/ui/ios_system_utils.h"
#import "xenia/ui/ios_theme.h"

namespace {

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

}  // namespace

@implementation XeniaChoiceListViewController {
  std::vector<IOSConfigChoice> choices_;
  int64_t selected_value_;
  NSString* subtitle_;
  IOSChoiceSelectionHandler on_selection_;
}

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                      choices:(const std::vector<IOSConfigChoice>&)choices
                selectedValue:(int64_t)selectedValue
                  onSelection:(IOSChoiceSelectionHandler)onSelection {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];
  if (self) {
    self.title = title;
    subtitle_ = [subtitle copy];
    choices_ = choices;
    selected_value_ = selectedValue;
    on_selection_ = [onSelection copy];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = [UIColor systemBackgroundColor];
  self.tableView.separatorInset = UIEdgeInsetsMake(0, 16, 0, 16);

  if (subtitle_.length > 0) {
    UILabel* header_label = [[UILabel alloc] initWithFrame:CGRectZero];
    header_label.text = subtitle_;
    header_label.textColor = [XeniaTheme textSecondary];
    header_label.font = [UIFont systemFontOfSize:13];
    header_label.numberOfLines = 0;
    header_label.textAlignment = NSTextAlignmentLeft;
    header_label.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* header = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 1, 56)];
    [header addSubview:header_label];
    [NSLayoutConstraint activateConstraints:@[
      [header_label.leadingAnchor constraintEqualToAnchor:header.leadingAnchor constant:18],
      [header_label.trailingAnchor constraintEqualToAnchor:header.trailingAnchor constant:-18],
      [header_label.topAnchor constraintEqualToAnchor:header.topAnchor constant:8],
      [header_label.bottomAnchor constraintEqualToAnchor:header.bottomAnchor constant:-8],
    ]];
    self.tableView.tableHeaderView = header;
  }
}

- (NSInteger)tableView:(UITableView* __unused)tableView
    numberOfRowsInSection:(NSInteger)__unused section {
  return static_cast<NSInteger>(choices_.size());
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  static NSString* const kChoiceCellIdentifier = @"XeniaChoiceCell";
  UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:kChoiceCellIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:kChoiceCellIdentifier];
  }
  if (indexPath.row < 0 || indexPath.row >= static_cast<NSInteger>(choices_.size())) {
    cell.textLabel.text = @"";
    cell.accessoryType = UITableViewCellAccessoryNone;
    return cell;
  }
  const IOSConfigChoice& choice = choices_[indexPath.row];
  cell.textLabel.text = ToNSString(choice.title);
  cell.accessoryType = (choice.value == selected_value_) ? UITableViewCellAccessoryCheckmark
                                                         : UITableViewCellAccessoryNone;
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.row < 0 || indexPath.row >= static_cast<NSInteger>(choices_.size())) {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }
  const IOSConfigChoice& choice = choices_[indexPath.row];
  selected_value_ = choice.value;
  if (on_selection_) {
    on_selection_(selected_value_);
  }
  [self.navigationController popViewControllerAnimated:YES];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskAllButUpsideDown;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return xe_current_interface_orientation(self.view);
}

@end
