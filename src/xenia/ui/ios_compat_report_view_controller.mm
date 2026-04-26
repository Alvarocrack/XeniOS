/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_compat_report_view_controller.h"

#include "xenia/base/logging.h"
#import "xenia/ui/ios_compat_data.h"
#import "xenia/ui/ios_system_utils.h"
#import "xenia/ui/ios_theme.h"

@implementation XeniaCompatReportViewController {
  uint32_t title_id_;
  NSString* game_title_;
  NSInteger selected_status_;
  NSInteger selected_perf_;
  UITextView* notes_text_view_;
  UILabel* notes_placeholder_label_;
  UIBarButtonItem* keyboard_done_button_;
  NSMutableArray<UIImage*>* screenshots_;
  BOOL submitting_;
}

- (instancetype)initWithTitleID:(uint32_t)title_id title:(NSString*)title {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];
  if (self) {
    title_id_ = title_id;
    game_title_ = [title copy];
    selected_status_ = -1;
    selected_perf_ = -1;
    screenshots_ = [[NSMutableArray alloc] init];
    submitting_ = NO;
    self.title = @"Submit Report";
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [game_title_ release];
  [notes_text_view_ release];
  [notes_placeholder_label_ release];
  [keyboard_done_button_ release];
  [screenshots_ release];
  [super dealloc];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = [UIColor systemBackgroundColor];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 64.0;
  self.tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
  keyboard_done_button_ =
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                    target:self
                                                    action:@selector(dismissKeyboard)];
  keyboard_done_button_.tintColor = [XeniaTheme accent];
  if (@available(iOS 15.0, *)) {
    self.tableView.sectionHeaderTopPadding = 0;
  }
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardWillChangeFrame:)
                                               name:UIKeyboardWillChangeFrameNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(keyboardWillHide:)
                                               name:UIKeyboardWillHideNotification
                                             object:nil];
}

- (void)scrollNotesEditorIntoViewAnimated:(BOOL)animated {
  NSIndexPath* notes_path = [NSIndexPath indexPathForRow:0 inSection:4];
  if ([self.tableView numberOfSections] <= notes_path.section ||
      [self.tableView numberOfRowsInSection:notes_path.section] <= notes_path.row) {
    return;
  }
  [self.tableView scrollToRowAtIndexPath:notes_path
                        atScrollPosition:UITableViewScrollPositionTop
                                animated:animated];
  if (!notes_text_view_) {
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    CGRect target = [self->notes_text_view_ convertRect:self->notes_text_view_.bounds
                                                 toView:self.tableView];
    target = CGRectInset(target, 0.0, -12.0);
    [self.tableView scrollRectToVisible:target animated:animated];
  });
}

- (void)keyboardWillChangeFrame:(NSNotification*)notification {
  NSDictionary* user_info = notification.userInfo;
  CGRect keyboard_end = [user_info[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  CGRect keyboard_in_view = [self.view convertRect:keyboard_end fromView:nil];
  CGFloat overlap = MAX(0.0, CGRectGetMaxY(self.view.bounds) - CGRectGetMinY(keyboard_in_view));
  CGFloat safe_bottom = self.view.safeAreaInsets.bottom;
  CGFloat bottom_inset = MAX(0.0, overlap - safe_bottom);

  NSTimeInterval duration = [user_info[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationOptions options =
      (UIViewAnimationOptions)([user_info[UIKeyboardAnimationCurveUserInfoKey] integerValue] << 16);

  [UIView animateWithDuration:duration
                        delay:0.0
                      options:options
                   animations:^{
                     UIEdgeInsets content_inset = self.tableView.contentInset;
                     content_inset.bottom = bottom_inset + 16.0;
                     self.tableView.contentInset = content_inset;
                     self.tableView.scrollIndicatorInsets = content_inset;
                   }
                   completion:nil];

  if ([notes_text_view_ isFirstResponder]) {
    [self scrollNotesEditorIntoViewAnimated:YES];
  }
}

- (void)keyboardWillHide:(NSNotification*)notification {
  NSDictionary* user_info = notification.userInfo;
  NSTimeInterval duration = [user_info[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationOptions options =
      (UIViewAnimationOptions)([user_info[UIKeyboardAnimationCurveUserInfoKey] integerValue] << 16);
  [UIView animateWithDuration:duration
                        delay:0.0
                      options:options
                   animations:^{
                     UIEdgeInsets content_inset = self.tableView.contentInset;
                     content_inset.bottom = 0.0;
                     self.tableView.contentInset = content_inset;
                     self.tableView.scrollIndicatorInsets = content_inset;
                   }
                   completion:nil];
}

- (void)textViewDidBeginEditing:(UITextView*)textView {
  if (textView != notes_text_view_) {
    return;
  }
  self.navigationItem.rightBarButtonItem = keyboard_done_button_;
  [self scrollNotesEditorIntoViewAnimated:YES];
}

- (void)textViewDidChange:(UITextView*)textView {
  if (textView == notes_text_view_) {
    notes_placeholder_label_.hidden = (textView.text.length > 0);
  }
}

- (void)textViewDidEndEditing:(UITextView*)textView {
  if (textView == notes_text_view_) {
    self.navigationItem.rightBarButtonItem = nil;
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (!notes_text_view_ || !notes_placeholder_label_) {
    return;
  }
  UIEdgeInsets insets = notes_text_view_.textContainerInset;
  CGFloat line_padding = notes_text_view_.textContainer.lineFragmentPadding;
  CGFloat available_width =
      CGRectGetWidth(notes_text_view_.bounds) - insets.left - insets.right - (line_padding * 2.0);
  if (available_width > 0.0) {
    notes_placeholder_label_.preferredMaxLayoutWidth = floor(available_width);
  }
}

- (UIButton*)reportOptionButtonWithTitle:(NSString*)title
                                   color:(UIColor*)color
                                selected:(BOOL)selected
                                 enabled:(BOOL)enabled
                                  target:(SEL)target
                                     tag:(NSInteger)tag {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button setTitle:title forState:UIControlStateNormal];
  [button setTitleColor:color forState:UIControlStateNormal];
  xe_apply_button_title_font(button, UIFontTextStyleCaption1, 13.0, UIFontWeightSemibold);
  button.contentEdgeInsets = UIEdgeInsetsMake(6.0, 12.0, 6.0, 12.0);
  button.backgroundColor = [color colorWithAlphaComponent:selected ? 0.16 : 0.10];
  button.layer.cornerRadius = 10.0;
  button.layer.borderWidth = selected ? 1.0 : 0.0;
  button.layer.borderColor = [color colorWithAlphaComponent:0.45].CGColor;
  button.enabled = enabled;
  button.alpha = enabled ? 1.0 : 0.35;
  button.tag = tag;
  [button addTarget:self action:target forControlEvents:UIControlEventTouchUpInside];
  return button;
}

- (UITableViewCell*)reportOptionsCellForSection:(NSInteger)section
                                      tableView:(UITableView* __unused)tableView {
  UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                  reuseIdentifier:nil] autorelease];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor clearColor];
  cell.contentView.backgroundColor = [UIColor clearColor];

  NSArray<NSString*>* keys = section == 2 ? xe_compat_statuses() : xe_compat_perfs();
  NSArray<NSString*>* labels = section == 2 ? xe_compat_status_labels() : xe_compat_perf_labels();

  UIView* card = [[[UIView alloc] init] autorelease];
  card.translatesAutoresizingMaskIntoConstraints = NO;
  card.backgroundColor = [XeniaTheme bgSurface];
  card.layer.cornerRadius = XeniaRadiusXl;
  card.layer.borderWidth = 0.5;
  card.layer.borderColor = [XeniaTheme border].CGColor;
  [cell.contentView addSubview:card];
  [NSLayoutConstraint activateConstraints:@[
    [card.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor constant:6.0],
    [card.leadingAnchor constraintEqualToAnchor:cell.contentView.leadingAnchor constant:16.0],
    [card.trailingAnchor constraintEqualToAnchor:cell.contentView.trailingAnchor constant:-16.0],
    [card.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor constant:-6.0],
  ]];
  UIStackView* vertical_stack = [[[UIStackView alloc] init] autorelease];
  vertical_stack.translatesAutoresizingMaskIntoConstraints = NO;
  vertical_stack.axis = UILayoutConstraintAxisVertical;
  vertical_stack.spacing = 10.0;
  [card addSubview:vertical_stack];

  NSMutableArray<NSArray<NSNumber*>*>* rows = [NSMutableArray array];
  if (section == 2) {
    [rows addObject:@[ @0, @1, @2 ]];
    [rows addObject:@[ @3, @4 ]];
  } else {
    [rows addObject:@[ @0, @1 ]];
    [rows addObject:@[ @2, @3 ]];
  }

  for (NSArray<NSNumber*>* row_indexes in rows) {
    UIStackView* row = [[[UIStackView alloc] init] autorelease];
    row.axis = UILayoutConstraintAxisHorizontal;
    row.spacing = 10.0;
    row.alignment = UIStackViewAlignmentLeading;
    row.distribution = UIStackViewDistributionFillProportionally;

    for (NSNumber* index_number in row_indexes) {
      NSInteger option_index = [index_number integerValue];
      NSString* key = keys[option_index];
      NSString* label = labels[option_index];
      UIColor* color = section == 2 ? xe_compat_status_color(key) : xe_compat_perf_color(key);
      BOOL selected =
          section == 2 ? (option_index == selected_status_) : (option_index == selected_perf_);
      BOOL enabled = YES;
      if (section == 3) {
        BOOL force_na = (selected_status_ == 4);
        enabled = !force_na || option_index == 3;
      }
      UIButton* button =
          [self reportOptionButtonWithTitle:label
                                      color:color
                                   selected:selected
                                    enabled:enabled
                                     target:(section == 2) ? @selector(reportStatusButtonTapped:)
                                                           : @selector(reportPerfButtonTapped:)
                                        tag:option_index];
      [row addArrangedSubview:button];
    }

    UIView* spacer = [[[UIView alloc] init] autorelease];
    spacer.translatesAutoresizingMaskIntoConstraints = NO;
    [spacer.widthAnchor constraintGreaterThanOrEqualToConstant:1.0].active = YES;
    [row addArrangedSubview:spacer];
    [vertical_stack addArrangedSubview:row];
  }

  [NSLayoutConstraint activateConstraints:@[
    [vertical_stack.topAnchor constraintEqualToAnchor:card.topAnchor constant:14.0],
    [vertical_stack.leadingAnchor constraintEqualToAnchor:card.leadingAnchor constant:16.0],
    [vertical_stack.trailingAnchor constraintEqualToAnchor:card.trailingAnchor constant:-16.0],
    [vertical_stack.bottomAnchor constraintEqualToAnchor:card.bottomAnchor constant:-14.0],
  ]];

  return cell;
}

- (void)reportStatusButtonTapped:(UIButton*)sender {
  selected_status_ = sender.tag;
  if (selected_status_ == 4) {
    selected_perf_ = 3;
  }
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(2, 2)]
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)reportPerfButtonTapped:(UIButton*)sender {
  if (selected_status_ == 4 && sender.tag != 3) {
    return;
  }
  selected_perf_ = sender.tag;
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:3]
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)dismissKeyboard {
  [notes_text_view_ resignFirstResponder];
}

- (void)showAlertWithTitle:(NSString*)title message:(NSString*)message {
  XEPresentOKAlert(self, title, message);
}

- (void)finishSuccessfulSubmissionWithIssueURL:(NSString*)issue_url
                                    compatInfo:(NSDictionary*)compat_info
                            discussionSnapshot:(NSDictionary*)discussion_snapshot {
  NSMutableDictionary* compat_user_info = [NSMutableDictionary dictionaryWithObject:@(title_id_)
                                                                             forKey:@"titleId"];
  if (compat_info) {
    compat_user_info[@"compatInfo"] = compat_info;
  }
  [[NSNotificationCenter defaultCenter] postNotificationName:kXeniaCompatDataDidUpdateNotification
                                                      object:nil
                                                    userInfo:compat_user_info];

  NSMutableDictionary* discussion_user_info = [NSMutableDictionary dictionaryWithObject:@(title_id_)
                                                                                 forKey:@"titleId"];
  if (discussion_snapshot) {
    discussion_user_info[@"discussion"] = discussion_snapshot;
  }
  [[NSNotificationCenter defaultCenter] postNotificationName:kXeniaDiscussionDidUpdateNotification
                                                      object:nil
                                                    userInfo:discussion_user_info];

  NSString* message = @"Your compatibility report has been submitted.";
  if ([issue_url isKindOfClass:[NSString class]] && issue_url.length > 0) {
    message = [message stringByAppendingFormat:@"\n\nGitHub issue: %@", issue_url];
  }

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:@"Report Submitted"
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"OK"
                                 style:UIAlertActionStyleDefault
                               handler:^(__unused UIAlertAction* action) {
                                 if (self.navigationController) {
                                   [self.navigationController popViewControllerAnimated:YES];
                                 } else {
                                   [self dismissViewControllerAnimated:YES completion:nil];
                                 }
                               }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (NSString*)trimmedNotes {
  return [notes_text_view_.text
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

- (void)submitReport {
  if (submitting_) {
    return;
  }
  if (selected_status_ < 0) {
    [self showAlertWithTitle:@"Missing Status" message:@"Please select a compatibility status."];
    return;
  }
  if (selected_perf_ < 0) {
    [self showAlertWithTitle:@"Missing Performance" message:@"Please select a performance tier."];
    return;
  }

  NSString* notes = [self trimmedNotes];
  if (notes.length == 0) {
    [self showAlertWithTitle:@"Missing Notes"
                     message:@"Please add a short note about your experience."];
    return;
  }

  submitting_ = YES;
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:6]
                withRowAnimation:UITableViewRowAnimationNone];

  NSMutableArray<NSString*>* screenshot_data =
      [NSMutableArray arrayWithCapacity:screenshots_.count];
  NSUInteger screenshot_total_bytes = 0;
  for (UIImage* image in screenshots_) {
    NSData* jpeg = UIImageJPEGRepresentation(image, 0.8);
    if (!jpeg) {
      continue;
    }
    screenshot_total_bytes += jpeg.length;
    [screenshot_data addObject:[jpeg base64EncodedStringWithOptions:0]];
  }

  NSString* device_machine = xe_device_machine();
  NSString* device_display = xe_device_display_name();
  NSDictionary* build_info = xe_current_compat_report_build_info();
  NSDictionary* payload = @{
    @"titleId" : XEFormatTitleIDHexUpper(title_id_),
    @"title" : game_title_ ?: @"",
    @"status" : xe_compat_statuses()[selected_status_],
    @"perf" : xe_compat_perfs()[selected_perf_],
    @"platform" : @"ios",
    @"device" : device_display ?: @"Unknown",
    @"deviceMachine" : device_machine ?: @"Unknown",
    @"osVersion" : [UIDevice currentDevice].systemVersion ?: @"",
    @"arch" : @"arm64",
    @"gpuBackend" : @"msl",
    @"notes" : notes,
    @"tags" : @[],
    @"screenshots" : screenshot_data,
    @"build" : build_info,
    @"buildId" : xe_string_from_object(build_info[@"buildId"]) ?: @"",
    @"channel" : xe_string_from_object(build_info[@"channel"]) ?: @"self-built",
    @"official" : build_info[@"official"] ?: @NO,
    @"appVersion" : xe_string_from_object(build_info[@"appVersion"]) ?: @"",
    @"buildNumber" : xe_string_from_object(build_info[@"buildNumber"]) ?: @"",
    @"commitShort" : xe_string_from_object(build_info[@"commitShort"]) ?: @"",
  };

  NSError* json_error = nil;
  NSData* request_body = [NSJSONSerialization dataWithJSONObject:payload
                                                         options:0
                                                           error:&json_error];
  if (!request_body) {
    submitting_ = NO;
    [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:6]
                  withRowAnimation:UITableViewRowAnimationNone];
    NSString* message =
        json_error.localizedDescription ?: @"Unable to serialize the report payload.";
    [self showAlertWithTitle:@"Submission Failed" message:message];
    return;
  }

  XELOGI("iOS compat submit: title_id={:08X} status={} perf={} channel={} build_id={} "
         "commit={} screenshots={} screenshot_bytes={} body_bytes={}",
         title_id_, [xe_compat_statuses()[selected_status_] UTF8String],
         [xe_compat_perfs()[selected_perf_] UTF8String],
         [xe_string_from_object(build_info[@"channel"]) UTF8String],
         [xe_string_from_object(build_info[@"buildId"]) UTF8String],
         [xe_string_from_object(build_info[@"commitShort"]) UTF8String], (int)screenshot_data.count,
         static_cast<unsigned long long>(screenshot_total_bytes),
         static_cast<unsigned long long>(request_body.length));

  NSURL* url = [NSURL URLWithString:@"https://xenios-compat-api.xenios.workers.dev/report"];
  NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
  request.HTTPMethod = @"POST";
  [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
  [request setValue:@"Bearer xenios-compat-report" forHTTPHeaderField:@"Authorization"];
  request.HTTPBody = request_body;

  NSURLSessionDataTask* task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
          dispatch_async(dispatch_get_main_queue(), ^{
            self->submitting_ = NO;
            [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:6]
                          withRowAnimation:UITableViewRowAnimationNone];

            if (error) {
              [self showAlertWithTitle:@"Network Error" message:error.localizedDescription];
              return;
            }

            NSHTTPURLResponse* http_response = (NSHTTPURLResponse*)response;
            NSInteger status_code = [http_response isKindOfClass:[NSHTTPURLResponse class]]
                                        ? http_response.statusCode
                                        : 0;

            NSString* response_text = @"";
            if (data.length > 0) {
              NSString* body_text =
                  [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
              if (body_text.length > 0) {
                response_text = body_text;
              }
            }

            if (![http_response isKindOfClass:[NSHTTPURLResponse class]] || status_code < 200 ||
                status_code >= 300) {
              NSString* message =
                  [NSString stringWithFormat:@"Server returned HTTP %ld", (long)status_code];
              if (response_text.length > 0) {
                message = [message stringByAppendingFormat:@"\n%@", response_text];
              }
              [self showAlertWithTitle:@"Submission Failed" message:message];
              return;
            }

            NSString* issue_url = nil;
            if (data.length > 0) {
              id response_json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
              if ([response_json isKindOfClass:[NSDictionary class]] &&
                  [response_json[@"issueUrl"] isKindOfClass:[NSString class]]) {
                issue_url = response_json[@"issueUrl"];
              }
            }

            NSDictionary* local_report = xe_build_local_compat_report(
                device_display, device_machine, [UIDevice currentDevice].systemVersion,
                xe_compat_statuses()[self->selected_status_],
                xe_compat_perfs()[self->selected_perf_], notes, build_info);
            NSDictionary* compat_info = xe_update_cached_compat_entry_for_submission(
                self->title_id_, self->game_title_, local_report, issue_url);
            NSDictionary* discussion_snapshot =
                xe_cache_discussion_snapshot_for_submission(self->title_id_, compat_info);

            [self finishSuccessfulSubmissionWithIssueURL:issue_url
                                              compatInfo:compat_info
                                      discussionSnapshot:discussion_snapshot];
          });
        }];
  [task resume];
}

- (void)addScreenshotTapped {
  if (screenshots_.count >= 5) {
    [self showAlertWithTitle:@"Limit Reached" message:@"You can attach up to 5 screenshots."];
    return;
  }

  PHPickerConfiguration* configuration = [[PHPickerConfiguration alloc] init];
  configuration.selectionLimit = static_cast<NSInteger>(5 - screenshots_.count);
  configuration.filter = [PHPickerFilter imagesFilter];

  PHPickerViewController* picker =
      [[PHPickerViewController alloc] initWithConfiguration:configuration];
  picker.delegate = self;
  [self presentViewController:picker animated:YES completion:nil];
  [picker release];
  [configuration release];
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker didFinishPicking:(NSArray<PHPickerResult*>*)results {
  [picker dismissViewControllerAnimated:YES completion:nil];

  for (PHPickerResult* result in results) {
    if (screenshots_.count >= 5) {
      break;
    }
    [result.itemProvider
        loadObjectOfClass:[UIImage class]
        completionHandler:^(id<NSItemProviderReading> object, NSError* __unused error) {
          UIImage* image = (UIImage*)object;
          if (!image) {
            return;
          }

          // Re-render to strip metadata and keep uploads bounded.
          CGFloat max_dimension = 1280.0;
          CGSize size = image.size;
          CGFloat scale = 1.0;
          if (size.width > max_dimension || size.height > max_dimension) {
            scale = (size.width > size.height) ? (max_dimension / size.width)
                                               : (max_dimension / size.height);
          }
          CGSize new_size = CGSizeMake(size.width * scale, size.height * scale);
          UIGraphicsBeginImageContextWithOptions(new_size, NO, 1.0);
          [image drawInRect:CGRectMake(0, 0, new_size.width, new_size.height)];
          UIImage* sanitized_image = UIGraphicsGetImageFromCurrentImageContext();
          UIGraphicsEndImageContext();
          if (!sanitized_image) {
            return;
          }

          dispatch_async(dispatch_get_main_queue(), ^{
            if (self->screenshots_.count >= 5) {
              return;
            }
            [self->screenshots_ addObject:sanitized_image];
            [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:5]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
          });
        }];
  }
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView* __unused)tableView {
  return 7;
}

- (NSInteger)tableView:(UITableView* __unused)tableView numberOfRowsInSection:(NSInteger)section {
  switch (section) {
    case 0:
      return 2;
    case 1:
      return 5;
    case 2:
      return 1;
    case 3:
      return 1;
    case 4:
      return 1;
    case 5:
      return static_cast<NSInteger>(screenshots_.count) + 1;
    case 6:
      return 1;
    default:
      return 0;
  }
}

- (NSString*)tableView:(UITableView* __unused)tableView titleForHeaderInSection:(NSInteger)section {
  switch (section) {
    case 0:
      return @"Game";
    case 1:
      return @"Environment";
    case 2:
      return @"Compatibility Status";
    case 3:
      return @"Performance";
    case 4:
      return @"Notes";
    case 5:
      return @"Screenshots";
    default:
      return nil;
  }
}

- (NSString*)tableView:(UITableView* __unused)tableView titleForFooterInSection:(NSInteger)section {
  if (section != 1) {
    return nil;
  }
  NSDictionary* build_info = xe_current_compat_report_build_info();
  NSString* build_label = xe_user_facing_build_label(build_info);
  return build_label.length > 0
             ? [NSString stringWithFormat:@"Reports are tagged as %@.", build_label]
             : nil;
}

- (CGFloat)tableView:(UITableView* __unused)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == 4) {
    return 128.0;
  }
  if (indexPath.section == 6) {
    return 52.0;
  }
  return UITableViewAutomaticDimension;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == 0 || indexPath.section == 1) {
    UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
                                                    reuseIdentifier:nil] autorelease];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    cell.detailTextLabel.adjustsFontSizeToFitWidth = YES;
    cell.detailTextLabel.minimumScaleFactor = 0.8;
    if (indexPath.section == 0) {
      if (indexPath.row == 0) {
        cell.textLabel.text = @"Title";
        cell.detailTextLabel.text = game_title_;
      } else {
        cell.textLabel.text = @"Title ID";
        cell.detailTextLabel.text = XEFormatTitleIDHexUpper(title_id_);
      }
    } else {
      NSDictionary* build_info = xe_current_compat_report_build_info();
      switch (indexPath.row) {
        case 0:
          cell.textLabel.text = @"Device";
          cell.detailTextLabel.text = xe_device_display_name();
          break;
        case 1:
          cell.textLabel.text = @"OS Version";
          cell.detailTextLabel.text = [UIDevice currentDevice].systemVersion;
          break;
        case 2:
          cell.textLabel.text = @"Architecture";
          cell.detailTextLabel.text = @"arm64";
          break;
        case 3:
          cell.textLabel.text = @"GPU Backend";
          cell.detailTextLabel.text = @"msl";
          break;
        case 4:
          cell.textLabel.text = @"Build";
          cell.detailTextLabel.text = xe_user_facing_build_label(build_info);
          break;
      }
    }
    return cell;
  }

  if (indexPath.section == 2 || indexPath.section == 3) {
    return [self reportOptionsCellForSection:indexPath.section tableView:tableView];
  }

  if (indexPath.section == 4) {
    UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                    reuseIdentifier:nil] autorelease];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    if (!notes_text_view_) {
      notes_text_view_ = [[UITextView alloc] init];
      notes_text_view_.delegate = self;
      notes_text_view_.backgroundColor = [UIColor clearColor];
      notes_text_view_.textColor = [XeniaTheme textPrimary];
      notes_text_view_.textContainerInset = UIEdgeInsetsMake(8, 4, 8, 4);
      xe_apply_text_view_font(notes_text_view_, UIFontTextStyleBody, 15.0, UIFontWeightRegular, NO);

      notes_placeholder_label_ = [[UILabel alloc] init];
      notes_placeholder_label_.translatesAutoresizingMaskIntoConstraints = NO;
      notes_placeholder_label_.text = @"Describe your experience (e.g. crashes, graphical "
                                      @"glitches, audio issues, performance drops)...";
      notes_placeholder_label_.textColor = [XeniaTheme textMuted];
      notes_placeholder_label_.numberOfLines = 0;
      notes_placeholder_label_.lineBreakMode = NSLineBreakByWordWrapping;
      notes_placeholder_label_.userInteractionEnabled = NO;
      xe_apply_label_font(notes_placeholder_label_, UIFontTextStyleBody, 15.0, UIFontWeightRegular);
    }

    if (notes_text_view_.superview != cell.contentView) {
      notes_text_view_.translatesAutoresizingMaskIntoConstraints = NO;
      [cell.contentView addSubview:notes_text_view_];
      [NSLayoutConstraint activateConstraints:@[
        [notes_text_view_.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor constant:8],
        [notes_text_view_.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor
                                                      constant:-8],
        [notes_text_view_.leadingAnchor constraintEqualToAnchor:cell.contentView.leadingAnchor
                                                       constant:8],
        [notes_text_view_.trailingAnchor constraintEqualToAnchor:cell.contentView.trailingAnchor
                                                        constant:-8],
      ]];
    }
    if (notes_placeholder_label_.superview != cell.contentView) {
      UIEdgeInsets insets = notes_text_view_.textContainerInset;
      CGFloat line_padding = notes_text_view_.textContainer.lineFragmentPadding;
      [cell.contentView addSubview:notes_placeholder_label_];
      [NSLayoutConstraint activateConstraints:@[
        [notes_placeholder_label_.topAnchor constraintEqualToAnchor:notes_text_view_.topAnchor
                                                           constant:insets.top],
        [notes_placeholder_label_.leadingAnchor
            constraintEqualToAnchor:notes_text_view_.leadingAnchor
                           constant:insets.left + line_padding],
        [notes_placeholder_label_.trailingAnchor
            constraintEqualToAnchor:notes_text_view_.trailingAnchor
                           constant:-(insets.right + line_padding)],
      ]];
    }
    notes_placeholder_label_.hidden = (notes_text_view_.text.length > 0);
    return cell;
  }

  if (indexPath.section == 5) {
    if (indexPath.row < static_cast<NSInteger>(screenshots_.count)) {
      UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                      reuseIdentifier:nil] autorelease];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      UIImage* thumbnail = screenshots_[indexPath.row];
      cell.imageView.image = thumbnail;
      cell.imageView.contentMode = UIViewContentModeScaleAspectFill;
      cell.imageView.clipsToBounds = YES;
      cell.imageView.layer.cornerRadius = 4.0;
      cell.textLabel.text =
          [NSString stringWithFormat:@"Screenshot %ld", (long)(indexPath.row + 1)];
      cell.textLabel.textColor = [XeniaTheme textPrimary];
      return cell;
    }

    UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                    reuseIdentifier:nil] autorelease];
    cell.textLabel.text = @"Add Screenshot";
    cell.textLabel.textColor = [XeniaTheme accent];
    cell.imageView.image = [UIImage systemImageNamed:@"plus.circle"];
    cell.imageView.tintColor = [XeniaTheme accent];
    return cell;
  }

  UITableViewCell* cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                                  reuseIdentifier:nil] autorelease];
  cell.backgroundColor = [XeniaTheme accent];
  cell.clipsToBounds = YES;
  cell.layer.cornerRadius = XeniaRadiusMd;
  cell.textLabel.text = submitting_ ? @"Submitting..." : @"Submit Report";
  cell.textLabel.textColor = [XeniaTheme accentFg];
  cell.textLabel.textAlignment = NSTextAlignmentCenter;
  xe_apply_label_font(cell.textLabel, UIFontTextStyleHeadline, 17.0, UIFontWeightSemibold);
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  if (indexPath.section == 4) {
    [notes_text_view_ becomeFirstResponder];
    return;
  }

  if (indexPath.section == 5) {
    if (indexPath.row >= static_cast<NSInteger>(screenshots_.count)) {
      [self addScreenshotTapped];
    }
    return;
  }

  if (indexPath.section == 6) {
    [self submitReport];
  }
}

- (BOOL)tableView:(UITableView* __unused)tableView canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  return (indexPath.section == 5 && indexPath.row < static_cast<NSInteger>(screenshots_.count));
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete) {
    return;
  }
  if (indexPath.section != 5 || indexPath.row >= static_cast<NSInteger>(screenshots_.count)) {
    return;
  }
  [screenshots_ removeObjectAtIndex:indexPath.row];
  [tableView reloadSections:[NSIndexSet indexSetWithIndex:5]
           withRowAnimation:UITableViewRowAnimationAutomatic];
}

@end
