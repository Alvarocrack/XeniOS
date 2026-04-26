/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#import "xenia/ui/ios_main_view_controller.h"

#import <GameController/GameController.h>
#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/string.h"
#include "xenia/config.h"
#include "xenia/hid/input.h"
#include "xenia/vfs/devices/xcontent_container_device.h"
#include "xenia/vfs/iso_metadata.h"
#include "xenia/vfs/xex_metadata.h"
#include "xenia/xbox.h"

#import "xenia/ui/apple_ui_flags.h"
#import "xenia/ui/apple_ui_navigation.h"
#import "xenia/ui/ios_choice_list_view_controller.h"
#import "xenia/ui/ios_compat_data.h"
#import "xenia/ui/ios_compat_report_view_controller.h"
#import "xenia/ui/ios_config_builder.h"
#import "xenia/ui/ios_config_models.h"
#import "xenia/ui/ios_config_view_controller.h"
#import "xenia/ui/ios_content_management.h"
#import "xenia/ui/ios_game_art.h"
#import "xenia/ui/ios_game_compatibility_view_controller.h"
#import "xenia/ui/ios_game_content_view_controller.h"
#import "xenia/ui/ios_game_tile_cell.h"
#import "xenia/ui/ios_landscape_navigation_controller.h"
#import "xenia/ui/ios_log_view_controller.h"
#import "xenia/ui/ios_metal_view.h"
#import "xenia/ui/ios_profile_view_controller.h"
#import "xenia/ui/ios_system_utils.h"
#import "xenia/ui/ios_theme.h"
#import "xenia/ui/ios_view_helpers.h"
#import "xenia/ui/windowed_app_context_ios.h"

DECLARE_path(log_file);

namespace {

using IOSFocusNodeId = xe::ui::apple::FocusNodeId;
static constexpr IOSFocusNodeId kLauncherFocusSettings = 1;
static constexpr IOSFocusNodeId kLauncherFocusProfile = 2;
static constexpr IOSFocusNodeId kLauncherFocusImport = 3;
static constexpr IOSFocusNodeId kLauncherFocusLibrary = 4;
static constexpr IOSFocusNodeId kInGameFocusResume = 101;
static constexpr IOSFocusNodeId kInGameFocusSettings = 102;
static constexpr IOSFocusNodeId kInGameFocusLog = 103;
static constexpr IOSFocusNodeId kInGameFocusExit = 104;

constexpr NSTimeInterval kXeniaAutoStikDebugCooldownSeconds = 10.0;

uint64_t GetNowMs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

NSTimeInterval GetUnixTimeSeconds() { return [[NSDate date] timeIntervalSince1970]; }

int16_t ToThumbAxis(float value) {
  const float clamped = std::clamp(value, -1.0f, 1.0f);
  return static_cast<int16_t>(clamped * 32767.0f);
}

uint8_t ToTriggerAxis(float value) {
  const float clamped = std::clamp(value, 0.0f, 1.0f);
  return static_cast<uint8_t>(clamped * 255.0f);
}

std::string TrimAscii(std::string value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

NSString* DecodeURLComponent(NSString* value) {
  if (!value || value.length == 0) {
    return nil;
  }
  NSString* decoded = [value stringByRemovingPercentEncoding];
  if (decoded && decoded.length > 0) {
    return decoded;
  }
  return value;
}

NSString* NormalizeURLToken(NSString* value) {
  NSString* decoded = DecodeURLComponent(value);
  if (!decoded) {
    return nil;
  }
  NSString* trimmed =
      [decoded stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
  return trimmed.length > 0 ? trimmed : nil;
}

NSString* URLQueryItemValueCaseInsensitive(NSURLComponents* components,
                                           NSArray<NSString*>* candidate_keys) {
  if (!components || candidate_keys.count == 0) {
    return nil;
  }
  for (NSString* key in candidate_keys) {
    for (NSURLQueryItem* item in components.queryItems) {
      if (!item.name || [item.name caseInsensitiveCompare:key] != NSOrderedSame) {
        continue;
      }
      NSString* value = NormalizeURLToken(item.value);
      if (value.length > 0) {
        return value;
      }
    }
  }
  return nil;
}

NSString* ExternalURLActionName(NSURL* url) {
  if (!url) {
    return nil;
  }
  NSString* host = NormalizeURLToken(url.host);
  if (host.length > 0) {
    return [host lowercaseString];
  }
  for (NSString* path_component in url.pathComponents) {
    NSString* component = NormalizeURLToken(path_component);
    if (!component || [component isEqualToString:@"/"]) {
      continue;
    }
    return [component lowercaseString];
  }
  return nil;
}

bool BuildLaunchPathFromURLValue(NSString* value, std::filesystem::path* path_out) {
  if (!path_out) {
    return false;
  }
  NSString* normalized = DecodeURLComponent(value);
  if (!normalized || normalized.length == 0) {
    return false;
  }

  NSURL* nested_url = [NSURL URLWithString:normalized];
  if (nested_url && nested_url.isFileURL) {
    normalized = nested_url.path;
  }
  if (!normalized || normalized.length == 0 || [normalized isEqualToString:@"/"]) {
    return false;
  }

  if ([normalized hasPrefix:@"private/"]) {
    normalized = [@"/" stringByAppendingString:normalized];
  } else if (![normalized hasPrefix:@"/"]) {
    normalized = [ToNSString(xe_get_ios_documents_path().string())
        stringByAppendingPathComponent:normalized];
  }

  *path_out = std::filesystem::path([normalized UTF8String]).lexically_normal();
  return !path_out->empty();
}

bool ExtractLaunchPathFromExternalURL(NSURL* url, std::filesystem::path* path_out) {
  if (!url || !path_out) {
    return false;
  }

  if (url.isFileURL) {
    const char* url_path = [url.path UTF8String];
    if (url_path && url_path[0]) {
      *path_out = std::filesystem::path(url_path).lexically_normal();
      return true;
    }
  }

  NSURLComponents* components = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
  if (components) {
    NSArray<NSURLQueryItem*>* query_items = components.queryItems;
    NSArray<NSString*>* candidate_keys = @[ @"path", @"file", @"game", @"rom", @"url" ];
    for (NSString* key in candidate_keys) {
      for (NSURLQueryItem* item in query_items) {
        if (!item.name || [item.name caseInsensitiveCompare:key] != NSOrderedSame) {
          continue;
        }
        if (BuildLaunchPathFromURLValue(item.value, path_out)) {
          return true;
        }
      }
    }
  }

  if (BuildLaunchPathFromURLValue(url.path, path_out)) {
    return true;
  }

  BOOL host_looks_like_path = NO;
  if (url.host && url.host.length > 0) {
    host_looks_like_path = [url.host hasPrefix:@"/"] || [url.host hasPrefix:@"private/"] ||
                           [url.host hasPrefix:@"%2F"] || [url.host hasPrefix:@"%2f"];
  }
  if (host_looks_like_path &&
      (!url.path || url.path.length == 0 || [url.path isEqualToString:@"/"]) &&
      BuildLaunchPathFromURLValue(url.host, path_out)) {
    return true;
  }

  return false;
}

bool ParseTitleIDFromURLValue(NSString* value, uint32_t* title_id_out) {
  if (!value || !title_id_out) {
    return false;
  }
  NSString* normalized = DecodeURLComponent(value);
  if (!normalized) {
    return false;
  }
  normalized = [normalized
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
  if ([normalized hasPrefix:@"0x"] || [normalized hasPrefix:@"0X"]) {
    normalized = [normalized substringFromIndex:2];
  }
  if (normalized.length != 8) {
    return false;
  }
  const char* utf8 = [normalized UTF8String];
  if (!utf8 || !utf8[0]) {
    return false;
  }
  char* end = nullptr;
  errno = 0;
  unsigned long parsed = std::strtoul(utf8, &end, 16);
  if (errno != 0 || !end || *end != '\0' || parsed > UINT32_MAX || parsed == 0) {
    return false;
  }
  *title_id_out = static_cast<uint32_t>(parsed);
  return true;
}

bool ParseGameSystemFromURLValue(NSString* value, xe::ui::IOSGameSystem* system_out) {
  if (!value || !system_out) {
    return false;
  }
  NSString* normalized = NormalizeURLToken(value);
  if (!normalized) {
    return false;
  }
  normalized = [normalized lowercaseString];
  if ([normalized isEqualToString:@"xbox360"] || [normalized isEqualToString:@"360"] ||
      [normalized isEqualToString:@"xenia"]) {
    *system_out = xe::ui::IOSGameSystem::kXbox360;
    return true;
  }
  return false;
}

bool IsExternalGameInfoRequestURL(NSURL* url, NSString** callback_scheme_out) {
  if (callback_scheme_out) {
    *callback_scheme_out = nil;
  }
  if (!url || url.isFileURL) {
    return false;
  }
  NSString* action = ExternalURLActionName(url);
  if (!action || [action caseInsensitiveCompare:@"gameinfo"] != NSOrderedSame) {
    return false;
  }
  NSURLComponents* components = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
  if (callback_scheme_out) {
    *callback_scheme_out = URLQueryItemValueCaseInsensitive(
        components, @[ @"scheme", @"callback-scheme", @"callback_scheme" ]);
  }
  return true;
}

bool ExtractLaunchTitleIDFromExternalURL(NSURL* url, uint32_t* title_id_out,
                                         xe::ui::IOSGameSystem* system_out,
                                         bool* system_present_out) {
  if (!url || !title_id_out || url.isFileURL) {
    return false;
  }

  if (system_out) {
    *system_out = xe::ui::IOSGameSystem::kXbox360;
  }
  if (system_present_out) {
    *system_present_out = false;
  }

  NSURLComponents* components = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
  if (components) {
    if (system_out) {
      NSArray<NSString*>* system_keys = @[ @"system", @"platform", @"core" ];
      for (NSString* key in system_keys) {
        for (NSURLQueryItem* item in components.queryItems) {
          if (!item.name || [item.name caseInsensitiveCompare:key] != NSOrderedSame) {
            continue;
          }
          if (ParseGameSystemFromURLValue(item.value, system_out)) {
            if (system_present_out) {
              *system_present_out = true;
            }
            break;
          }
        }
      }
    }
    NSArray<NSString*>* candidate_keys = @[ @"title-id", @"title_id", @"titleid", @"tid" ];
    for (NSString* key in candidate_keys) {
      for (NSURLQueryItem* item in components.queryItems) {
        if (!item.name || [item.name caseInsensitiveCompare:key] != NSOrderedSame) {
          continue;
        }
        if (ParseTitleIDFromURLValue(item.value, title_id_out)) {
          return true;
        }
      }
    }
  }

  NSArray<NSString*>* path_components = url.pathComponents;
  for (NSString* component in [path_components reverseObjectEnumerator]) {
    if (ParseTitleIDFromURLValue(component, title_id_out)) {
      return true;
    }
  }

  return ParseTitleIDFromURLValue(url.host, title_id_out);
}

struct IOSDiscoveredGame {
  std::filesystem::path path;
  std::string title;
  xe::ui::IOSGameSystem system = xe::ui::IOSGameSystem::kXbox360;
  uint32_t title_id = 0;
  std::vector<uint8_t> icon_data;
  bool has_compat_info = false;
  std::string compat_status;
  std::string compat_perf;
  std::string compat_notes;
  bool has_installed_content = false;
};

std::string ToLowerAsciiCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return value;
}

bool LooksLikeHexIdentifier(const std::string& value, size_t min_length = 8,
                            size_t max_length = 32) {
  if (value.size() < min_length || value.size() > max_length) {
    return false;
  }
  return std::all_of(value.begin(), value.end(),
                     [](unsigned char c) { return std::isxdigit(c) != 0; });
}

bool IsISOPath(const std::filesystem::path& path) {
  return ToLowerAsciiCopy(path.extension().string()) == ".iso";
}

bool IsDefaultXexPath(const std::filesystem::path& path) {
  return ToLowerAsciiCopy(path.filename().string()) == "default.xex";
}

bool IsDefaultXbePath(const std::filesystem::path& path) {
  return ToLowerAsciiCopy(path.filename().string()) == "default.xbe";
}

bool IsLikelyGodPath(const std::filesystem::path& path) {
  if (!path.has_filename()) {
    return false;
  }
  std::filesystem::path parent = path.parent_path();
  while (!parent.empty()) {
    std::string name_lower = ToLowerAsciiCopy(parent.filename().string());
    if (name_lower == "00007000" || name_lower == "00004000") {
      return true;
    }
    std::filesystem::path next = parent.parent_path();
    if (next == parent) {
      break;
    }
    parent = next;
  }
  return false;
}

bool LooksLikeHexContentFilename(const std::filesystem::path& path) {
  const std::string filename = path.filename().string();
  return LooksLikeHexIdentifier(filename, 24, 40);
}

bool IsLikelyGodContainerFile(const std::filesystem::path& path) {
  if (IsLikelyGodPath(path)) {
    return true;
  }
  if (path.has_extension()) {
    return false;
  }
  return LooksLikeHexContentFilename(path);
}

std::string LibraryFallbackTitleFromPath(const std::filesystem::path& path) {
  if (IsDefaultXexPath(path) || IsDefaultXbePath(path)) {
    std::filesystem::path parent = path.parent_path();
    while (!parent.empty()) {
      std::string candidate = parent.filename().string();
      std::string candidate_lower = ToLowerAsciiCopy(candidate);
      if (!candidate.empty() && !LooksLikeHexIdentifier(candidate) &&
          candidate_lower != "content" && candidate_lower != "games" &&
          candidate_lower != "files" && candidate_lower != "default") {
        return candidate;
      }
      std::filesystem::path next = parent.parent_path();
      if (next == parent) {
        break;
      }
      parent = next;
    }
  }

  std::string stem = path.stem().string();
  if (!stem.empty()) {
    return stem;
  }
  return path.filename().string();
}

void SortDiscoveredGames(std::vector<IOSDiscoveredGame>* games) {
  if (!games) {
    return;
  }
  std::sort(games->begin(), games->end(),
            [](const IOSDiscoveredGame& a, const IOSDiscoveredGame& b) {
              if (a.title == b.title) {
                return a.path.filename().string() < b.path.filename().string();
              }
              return a.title < b.title;
            });
}

std::string FormatTitleID(uint32_t title_id) {
  if (!title_id) {
    return std::string();
  }
  char buffer[9] = {};
  std::snprintf(buffer, sizeof(buffer), "%08X", title_id);
  return std::string(buffer);
}

static NSString* xe_game_system_url_value(xe::ui::IOSGameSystem system) {
  switch (system) {
    case xe::ui::IOSGameSystem::kXbox360:
    default:
      return @"xbox360";
  }
}

static BOOL xe_game_system_supports_compatibility(xe::ui::IOSGameSystem system) {
  return system == xe::ui::IOSGameSystem::kXbox360;
}

static BOOL xe_game_system_supports_manage_content(xe::ui::IOSGameSystem system) {
  return system == xe::ui::IOSGameSystem::kXbox360;
}

static BOOL xe_game_system_supports_remote_art(xe::ui::IOSGameSystem system) {
  return system == xe::ui::IOSGameSystem::kXbox360;
}

static NSString* xe_launch_url_for_title_id(uint32_t title_id, xe::ui::IOSGameSystem system) {
  if (!title_id) {
    return nil;
  }
  NSURLComponents* components = [[[NSURLComponents alloc] init] autorelease];
  components.scheme = @"xenios";
  components.host = @"launch";
  components.queryItems = @[
    [NSURLQueryItem queryItemWithName:@"title-id" value:ToNSString(FormatTitleID(title_id))],
    [NSURLQueryItem queryItemWithName:@"system" value:xe_game_system_url_value(system)]
  ];
  return components.URL.absoluteString;
}

static NSString* xe_game_info_callback_provider(NSURL* request_url) {
  NSString* scheme = NormalizeURLToken(request_url ? request_url.scheme : nil);
  if (!scheme || scheme.length == 0) {
    return @"xenios";
  }
  return [scheme lowercaseString];
}

static NSURL* xe_stikdebug_enable_jit_url_for_bundle_identifier(NSString* bundle_identifier) {
  if (!bundle_identifier || bundle_identifier.length == 0) {
    return nil;
  }
  NSURLComponents* components = [[[NSURLComponents alloc] init] autorelease];
  components.scheme = @"stikjit";
  components.host = @"enable-jit";
  components.queryItems = @[ [NSURLQueryItem queryItemWithName:@"bundle-id"
                                                         value:bundle_identifier] ];
  return components.URL;
}

static NSString* xe_normalize_game_title_for_ui(NSString* title) {
  if (!title || title.length == 0) {
    return title;
  }
  if ([title rangeOfCharacterFromSet:[NSCharacterSet whitespaceCharacterSet]].location !=
      NSNotFound) {
    return title;
  }
  NSRange letter_range = [title rangeOfCharacterFromSet:[NSCharacterSet letterCharacterSet]];
  if (letter_range.location == NSNotFound) {
    return title;
  }
  NSRange lower_range =
      [title rangeOfCharacterFromSet:[NSCharacterSet lowercaseLetterCharacterSet]];
  if (lower_range.location != NSNotFound) {
    return title;
  }
  NSCharacterSet* roman_set = [NSCharacterSet characterSetWithCharactersInString:@"IVXLCDM"];
  NSCharacterSet* non_roman_set = [roman_set invertedSet];
  if ([title rangeOfCharacterFromSet:non_roman_set].location == NSNotFound) {
    return title;
  }
  return [title localizedCapitalizedString];
}

std::string NormalizeGameTitleForUI(const std::string& title) {
  NSString* normalized = xe_normalize_game_title_for_ui(ToNSString(title));
  return normalized ? std::string([normalized UTF8String]) : title;
}

std::string DisplayNameFromXexMetadata(const std::filesystem::path& path,
                                       const std::optional<xe::vfs::XexMetadata>& metadata) {
  if (metadata.has_value() && !metadata->module_name.empty() && !IsDefaultXexPath(path)) {
    return metadata->module_name;
  }
  return LibraryFallbackTitleFromPath(path);
}

bool BuildDiscoveredGameFromPath(const std::filesystem::path& path, IOSDiscoveredGame* game_out) {
  if (!game_out) {
    return false;
  }

  IOSDiscoveredGame game;
  game.path = path;
  if (IsISOPath(path)) {
    auto metadata = xe::vfs::ExtractIsoMetadata(path);
    if (metadata.has_value()) {
      game.system = xe::ui::IOSGameSystem::kXbox360;
      game.title_id = metadata->title_id;
      game.title = NormalizeGameTitleForUI(DisplayNameFromXexMetadata(path, metadata));
      *game_out = std::move(game);
      return true;
    }

    game.system = xe::ui::IOSGameSystem::kXbox360;
    game.title = NormalizeGameTitleForUI(LibraryFallbackTitleFromPath(path));
    *game_out = std::move(game);
    return true;
  }

  if (IsDefaultXexPath(path)) {
    game.system = xe::ui::IOSGameSystem::kXbox360;
    auto metadata = xe::vfs::ExtractXexMetadata(path);
    if (metadata.has_value()) {
      game.title_id = metadata->title_id;
    }
    game.title = NormalizeGameTitleForUI(DisplayNameFromXexMetadata(path, metadata));
    *game_out = std::move(game);
    return true;
  }

  if (!IsLikelyGodContainerFile(path)) {
    return false;
  }

  auto header = xe::vfs::XContentContainerDevice::ReadContainerHeader(path);
  if (!header || !header->content_header.is_magic_valid()) {
    return false;
  }

  if (header->content_metadata.data_file_count > 0 && !HasContentSidecarDataDirectory(path)) {
    XELOGW("iOS: Skipping XContent package missing .data sidecar: {}", path);
    return false;
  }

  xe::XContentType content_type =
      static_cast<xe::XContentType>(header->content_metadata.content_type.get());
  if (content_type != xe::XContentType::kXbox360Title &&
      content_type != xe::XContentType::kInstalledGame) {
    return false;
  }

  game.system = xe::ui::IOSGameSystem::kXbox360;
  game.title_id = header->content_metadata.execution_info.title_id;
  std::string display_name =
      xe::to_utf8(header->content_metadata.display_name(xe::XLanguage::kEnglish));
  if (display_name.empty()) {
    display_name = xe::to_utf8(header->content_metadata.title_name());
  }
  if (display_name.empty()) {
    game.title = LibraryFallbackTitleFromPath(path);
  } else {
    game.title = display_name;
  }
  game.title = NormalizeGameTitleForUI(game.title);

  uint32_t thumb_size = header->content_metadata.title_thumbnail_size;
  if (thumb_size > 0 && thumb_size <= xe::vfs::XContentMetadata::kThumbLengthV1) {
    game.icon_data.assign(header->content_metadata.title_thumbnail,
                          header->content_metadata.title_thumbnail + thumb_size);
  }

  *game_out = std::move(game);
  return true;
}

}  // namespace
@implementation XeniaViewController {
  std::vector<IOSDiscoveredGame> discovered_games_;
  NSDictionary* compat_data_;
  xe::ui::apple::ControllerNavigationMapper controller_navigation_mapper_;
  xe::ui::apple::FocusGraph launcher_focus_graph_;
  xe::ui::apple::FocusGraph in_game_focus_graph_;
  NSInteger focused_game_index_;
  BOOL launcher_library_focus_active_;
  BOOL controller_navigation_was_enabled_;
  uint32_t native_controller_packet_number_;
  CGSize last_collection_layout_size_;
  BOOL compat_fetch_started_;
  std::filesystem::path pending_external_launch_path_;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor blackColor];
  self.jitAcquired = NO;
  self.gameRunning = NO;
  self.gameStopInProgress = NO;
  focused_game_index_ = -1;
  launcher_library_focus_active_ = NO;
  controller_navigation_was_enabled_ = NO;
  native_controller_packet_number_ = 0;
  last_collection_layout_size_ = CGSizeZero;
  compat_fetch_started_ = NO;
  controller_navigation_mapper_.Reset();

  // Create the Metal-backed rendering view (full screen, behind everything).
  self.metalView = [[XeniaMetalView alloc] initWithFrame:self.view.bounds];
  self.metalView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.metalView.contentScaleFactor = [UIScreen mainScreen].scale;
  [self.view addSubview:self.metalView];

  // Create the launcher overlay UI immediately. When JIT is missing, keep
  // settings/navigation available but gate game launch with status.
  [self setupLauncherOverlay];
  [self setupInGameMenuOverlay];
  UITapGestureRecognizer* tap =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(toggleInGameMenuTapped:)];
  tap.numberOfTapsRequired = 1;
  tap.cancelsTouchesInView = NO;
  [self.view addGestureRecognizer:tap];
  [self updateJITStatusIndicator];
  [self updateJITAvailabilityUI];
  [self refreshSignedInProfileUI];
  NSDictionary* cached_compat_data = xe_load_cached_compat_data();
  if (cached_compat_data) {
    [compat_data_ release];
    compat_data_ = [cached_compat_data retain];
  }
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(onCompatDataDidUpdate:)
                                               name:kXeniaCompatDataDidUpdateNotification
                                             object:nil];
  [self refreshImportedGames];

  // Start polling for JIT.
  [self startJITPoll];
  self.controllerNavTimer =
      [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                       target:self
                                     selector:@selector(pollControllerNavigation:)
                                     userInfo:nil
                                      repeats:YES];
  self.controllerNavTimer.tolerance = 0.01;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self startCompatFetchIfNeeded];
  xe_request_current_orientation(self);
}

- (void)startCompatFetchIfNeeded {
  if (compat_fetch_started_) {
    return;
  }
  compat_fetch_started_ = YES;
  xe_fetch_compat_data(^(NSDictionary* data) {
    if (!data) {
      return;
    }
    if (self->compat_data_ && [self->compat_data_ isEqualToDictionary:data]) {
      return;
    }
    std::filesystem::path focused_path;
    const BOOL has_focused_path =
        self->focused_game_index_ >= 0 &&
        self->focused_game_index_ < static_cast<NSInteger>(self->discovered_games_.size());
    if (has_focused_path) {
      focused_path = self->discovered_games_[static_cast<size_t>(self->focused_game_index_)].path;
    }
    [self->compat_data_ release];
    self->compat_data_ = [data retain];
    [self applyCompatDataToDiscoveredGames];
    SortDiscoveredGames(&self->discovered_games_);
    if (has_focused_path) {
      auto focused_it = std::find_if(
          self->discovered_games_.begin(), self->discovered_games_.end(),
          [&focused_path](const IOSDiscoveredGame& game) { return game.path == focused_path; });
      if (focused_it != self->discovered_games_.end()) {
        self->focused_game_index_ =
            static_cast<NSInteger>(std::distance(self->discovered_games_.begin(), focused_it));
      }
    }
    [self.importedGamesCollectionView reloadData];
    [self rebuildLauncherFocusGraph];
    [self applyLauncherFocusVisuals];
  });
}

- (void)onCompatDataDidUpdate:(NSNotification*)__unused notification {
  NSDictionary* cached_compat_data = xe_load_cached_compat_data();
  if (!cached_compat_data) {
    return;
  }
  std::filesystem::path focused_path;
  const BOOL has_focused_path =
      focused_game_index_ >= 0 &&
      focused_game_index_ < static_cast<NSInteger>(discovered_games_.size());
  if (has_focused_path) {
    focused_path = discovered_games_[static_cast<size_t>(focused_game_index_)].path;
  }
  [compat_data_ release];
  compat_data_ = [cached_compat_data retain];
  [self applyCompatDataToDiscoveredGames];
  SortDiscoveredGames(&discovered_games_);
  if (has_focused_path) {
    auto focused_it = std::find_if(
        discovered_games_.begin(), discovered_games_.end(),
        [&focused_path](const IOSDiscoveredGame& game) { return game.path == focused_path; });
    if (focused_it != discovered_games_.end()) {
      focused_game_index_ =
          static_cast<NSInteger>(std::distance(discovered_games_.begin(), focused_it));
    }
  }
  [self.importedGamesCollectionView reloadData];
  [self rebuildLauncherFocusGraph];
  [self applyLauncherFocusVisuals];
}

- (void)setButton:(UIButton*)button controllerFocused:(BOOL)focused {
  if (!button) {
    return;
  }
  button.layer.cornerRadius = XeniaRadiusMd;
  button.layer.borderWidth = focused ? 1.5 : 0.0;
  button.layer.borderColor = focused ? [XeniaTheme accent].CGColor : [UIColor clearColor].CGColor;
  button.layer.shadowColor = [XeniaTheme accent].CGColor;
  button.layer.shadowOpacity = focused ? 0.35f : 0.0f;
  button.layer.shadowRadius = focused ? 6.0f : 0.0f;
  button.layer.shadowOffset = CGSizeZero;
}

- (void)clearButtonFocusChrome:(UIButton*)button {
  if (!button) {
    return;
  }
  button.layer.cornerRadius = XeniaRadiusMd;
  button.layer.borderWidth = 0.0f;
  button.layer.borderColor = [UIColor clearColor].CGColor;
  button.layer.shadowOpacity = 0.0f;
  button.layer.shadowRadius = 0.0f;
  button.layer.shadowOffset = CGSizeZero;
}

- (void)setFocusedGameIndex:(NSInteger)index scroll:(BOOL)scroll {
  if (discovered_games_.empty()) {
    index = -1;
  } else {
    if (index < 0) {
      index = 0;
    }
    NSInteger max_index = static_cast<NSInteger>(discovered_games_.size() - 1);
    if (index > max_index) {
      index = max_index;
    }
  }
  NSInteger previous = focused_game_index_;
  focused_game_index_ = index;

  NSMutableArray<NSIndexPath*>* reload_paths = [NSMutableArray array];
  if (previous >= 0 && previous < static_cast<NSInteger>(discovered_games_.size())) {
    [reload_paths addObject:[NSIndexPath indexPathForItem:previous inSection:0]];
  }
  if (focused_game_index_ >= 0 &&
      focused_game_index_ < static_cast<NSInteger>(discovered_games_.size()) &&
      focused_game_index_ != previous) {
    [reload_paths addObject:[NSIndexPath indexPathForItem:focused_game_index_ inSection:0]];
  }
  if (reload_paths.count > 0) {
    [self.importedGamesCollectionView reloadItemsAtIndexPaths:reload_paths];
  }

  if (scroll && focused_game_index_ >= 0 &&
      focused_game_index_ < static_cast<NSInteger>(discovered_games_.size())) {
    NSIndexPath* path = [NSIndexPath indexPathForItem:focused_game_index_ inSection:0];
    [self.importedGamesCollectionView
        scrollToItemAtIndexPath:path
               atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                       animated:YES];
  }
}

- (void)rebuildLauncherFocusGraph {
  IOSFocusNodeId previous_focus = launcher_focus_graph_.current();
  launcher_focus_graph_.Clear();

  xe::ui::apple::FocusNode settings;
  settings.id = kLauncherFocusSettings;
  settings.right = kLauncherFocusProfile;
  settings.down = kLauncherFocusImport;
  settings.enabled = self.settingsButton.enabled && !self.settingsButton.hidden;

  xe::ui::apple::FocusNode profile;
  profile.id = kLauncherFocusProfile;
  profile.left = kLauncherFocusSettings;
  profile.down = kLauncherFocusImport;
  profile.enabled = self.profileButton.enabled && !self.profileButton.hidden;

  xe::ui::apple::FocusNode import_button;
  import_button.id = kLauncherFocusImport;
  import_button.left = kLauncherFocusProfile;
  import_button.right = kLauncherFocusLibrary;
  import_button.up = kLauncherFocusSettings;
  import_button.down = kLauncherFocusLibrary;
  import_button.enabled = self.openGameButton.enabled && !self.openGameButton.hidden;

  xe::ui::apple::FocusNode library;
  library.id = kLauncherFocusLibrary;
  library.left = kLauncherFocusImport;
  library.up = kLauncherFocusImport;
  library.enabled = !discovered_games_.empty();

  launcher_focus_graph_.AddOrUpdateNode(settings);
  launcher_focus_graph_.AddOrUpdateNode(profile);
  launcher_focus_graph_.AddOrUpdateNode(import_button);
  launcher_focus_graph_.AddOrUpdateNode(library);

  if (previous_focus != xe::ui::apple::kInvalidFocusNodeId) {
    launcher_focus_graph_.SetCurrent(previous_focus);
  }
}

- (void)rebuildInGameFocusGraph {
  IOSFocusNodeId previous_focus = in_game_focus_graph_.current();
  in_game_focus_graph_.Clear();

  xe::ui::apple::FocusNode resume;
  resume.id = kInGameFocusResume;
  resume.down = kInGameFocusSettings;
  resume.enabled =
      self.inGameResumeButton && self.inGameResumeButton.enabled && !self.inGameResumeButton.hidden;

  xe::ui::apple::FocusNode settings;
  settings.id = kInGameFocusSettings;
  settings.up = kInGameFocusResume;
  settings.down = kInGameFocusLog;
  settings.enabled = self.inGameSettingsButton && self.inGameSettingsButton.enabled &&
                     !self.inGameSettingsButton.hidden;

  xe::ui::apple::FocusNode log;
  log.id = kInGameFocusLog;
  log.up = kInGameFocusSettings;
  log.down = kInGameFocusExit;
  log.enabled = self.inGameLiveLogButton && self.inGameLiveLogButton.enabled &&
                !self.inGameLiveLogButton.hidden;

  xe::ui::apple::FocusNode exit;
  exit.id = kInGameFocusExit;
  exit.up = kInGameFocusLog;
  exit.enabled =
      self.inGameExitButton && self.inGameExitButton.enabled && !self.inGameExitButton.hidden;

  in_game_focus_graph_.AddOrUpdateNode(resume);
  in_game_focus_graph_.AddOrUpdateNode(settings);
  in_game_focus_graph_.AddOrUpdateNode(log);
  in_game_focus_graph_.AddOrUpdateNode(exit);

  if (previous_focus != xe::ui::apple::kInvalidFocusNodeId) {
    in_game_focus_graph_.SetCurrent(previous_focus);
  }
}

- (void)applyLauncherFocusVisuals {
  if (!controller_navigation_was_enabled_) {
    launcher_library_focus_active_ = NO;
    [self clearButtonFocusChrome:self.settingsButton];
    [self clearButtonFocusChrome:self.profileButton];
    [self clearButtonFocusChrome:self.openGameButton];
    if (focused_game_index_ >= 0 &&
        focused_game_index_ < static_cast<NSInteger>(discovered_games_.size())) {
      NSIndexPath* focused_path = [NSIndexPath indexPathForItem:focused_game_index_ inSection:0];
      [self.importedGamesCollectionView reloadItemsAtIndexPaths:@[ focused_path ]];
    }
    return;
  }

  IOSFocusNodeId current_focus = launcher_focus_graph_.current();
  BOOL settings_focused = current_focus == kLauncherFocusSettings;
  BOOL profile_focused = current_focus == kLauncherFocusProfile;
  BOOL import_focused = current_focus == kLauncherFocusImport;
  BOOL library_focused = current_focus == kLauncherFocusLibrary;

  (void)settings_focused;
  (void)profile_focused;
  (void)import_focused;
  [self clearButtonFocusChrome:self.settingsButton];
  [self clearButtonFocusChrome:self.profileButton];
  [self clearButtonFocusChrome:self.openGameButton];

  if (library_focused && focused_game_index_ < 0 && !discovered_games_.empty()) {
    [self setFocusedGameIndex:0 scroll:NO];
  }

  BOOL previous_library_focus_active = launcher_library_focus_active_;
  launcher_library_focus_active_ = library_focused;

  if (previous_library_focus_active != launcher_library_focus_active_ && focused_game_index_ >= 0 &&
      focused_game_index_ < static_cast<NSInteger>(discovered_games_.size())) {
    NSIndexPath* focused_path = [NSIndexPath indexPathForItem:focused_game_index_ inSection:0];
    [self.importedGamesCollectionView reloadItemsAtIndexPaths:@[ focused_path ]];
  }
}

- (void)applyInGameMenuFocusVisuals {
  if (!self.inGameMenuOverlay || self.inGameMenuOverlay.hidden ||
      !controller_navigation_was_enabled_) {
    [self setButton:self.inGameResumeButton controllerFocused:NO];
    [self setButton:self.inGameSettingsButton controllerFocused:NO];
    [self setButton:self.inGameLiveLogButton controllerFocused:NO];
    [self setButton:self.inGameExitButton controllerFocused:NO];
    return;
  }

  IOSFocusNodeId current_focus = in_game_focus_graph_.current();
  [self setButton:self.inGameResumeButton controllerFocused:current_focus == kInGameFocusResume];
  [self setButton:self.inGameSettingsButton
      controllerFocused:current_focus == kInGameFocusSettings];
  [self setButton:self.inGameLiveLogButton controllerFocused:current_focus == kInGameFocusLog];
  [self setButton:self.inGameExitButton controllerFocused:current_focus == kInGameFocusExit];
}

- (BOOL)launcherGridUsesCompactLandscapeLayoutForContentSize:(CGSize)content_size {
  return content_size.width > content_size.height && content_size.height < 430.0f;
}

- (NSInteger)launcherGridColumnCountForContentSize:(CGSize)content_size {
  CGFloat content_width = content_size.width;
  BOOL compact_landscape = [self launcherGridUsesCompactLandscapeLayoutForContentSize:content_size];
  CGFloat grid_spacing = compact_landscape ? 12.0f : 16.0f;
  CGFloat minimum_tile_width = compact_landscape ? 170.0f : 190.0f;
  NSInteger minimum_columns = compact_landscape ? 4 : 2;
  NSInteger maximum_columns = compact_landscape ? 5 : 6;

  NSInteger columns = static_cast<NSInteger>(
      floor((content_width + grid_spacing) / (minimum_tile_width + grid_spacing)));
  columns = MAX(columns, minimum_columns);
  columns = MIN(columns, maximum_columns);
  return columns;
}

- (NSInteger)launcherGridColumnCount {
  return [self launcherGridColumnCountForContentSize:self.importedGamesCollectionView.bounds.size];
}

- (CGFloat)launcherGridInteritemSpacingForCollectionView:(UICollectionView*)collectionView {
  return [self launcherGridUsesCompactLandscapeLayoutForContentSize:collectionView.bounds.size]
             ? 12.0f
             : 16.0f;
}

- (CGFloat)launcherGridLineSpacingForCollectionView:(UICollectionView*)collectionView {
  return [self launcherGridUsesCompactLandscapeLayoutForContentSize:collectionView.bounds.size]
             ? 14.0f
             : 20.0f;
}

- (CGFloat)launcherGridTitleHeightForCollectionView:(UICollectionView*)collectionView {
  return [self launcherGridUsesCompactLandscapeLayoutForContentSize:collectionView.bounds.size]
             ? 36.0f
             : 44.0f;
}

- (CGFloat)launcherGridTileWidthForCollectionView:(UICollectionView*)collectionView
                                          columns:(NSInteger)columns
                                 interitemSpacing:(CGFloat)spacing {
  CGFloat content_width = collectionView.bounds.size.width;
  CGFloat total_spacing = spacing * MAX(columns - 1, 0);
  CGFloat available_width = MAX(content_width - total_spacing, 0.0f);
  CGFloat tile_width = available_width / MAX(columns, 1);
  CGFloat screen_scale =
      collectionView.window.screen ? collectionView.window.screen.scale : UIScreen.mainScreen.scale;
  tile_width = floor(tile_width * screen_scale) / screen_scale;
  return MAX(tile_width, 100.0f);
}

- (NSInteger)launcherPageStep {
  NSArray<NSIndexPath*>* visible = self.importedGamesCollectionView.indexPathsForVisibleItems;
  if (visible.count > 0) {
    return visible.count;
  }
  return 6;
}

- (BOOL)handleControllerActionsForTableController:(UITableViewController*)table_controller
                                          actions:
                                              (const xe::ui::apple::ControllerActionSet&)actions {
  UITableView* table_view = table_controller.tableView;
  if (!table_view) {
    return NO;
  }

  NSMutableArray<NSIndexPath*>* all_paths = [NSMutableArray array];
  NSInteger sections = [table_view numberOfSections];
  for (NSInteger section = 0; section < sections; ++section) {
    NSInteger rows = [table_view numberOfRowsInSection:section];
    for (NSInteger row = 0; row < rows; ++row) {
      [all_paths addObject:[NSIndexPath indexPathForRow:row inSection:section]];
    }
  }
  if (all_paths.count == 0) {
    return NO;
  }

  NSIndexPath* selected = table_view.indexPathForSelectedRow;
  NSInteger selected_index = 0;
  if (selected) {
    NSUInteger found = [all_paths indexOfObject:selected];
    if (found != NSNotFound) {
      selected_index = static_cast<NSInteger>(found);
    }
  } else {
    selected = all_paths.firstObject;
    [table_view selectRowAtIndexPath:selected
                            animated:NO
                      scrollPosition:UITableViewScrollPositionMiddle];
  }

  BOOL handled = NO;
  if (actions.navigate_up && selected_index > 0) {
    selected_index--;
    handled = YES;
  }
  if (actions.navigate_down && selected_index + 1 < static_cast<NSInteger>(all_paths.count)) {
    selected_index++;
    handled = YES;
  }
  if (actions.page_prev && selected_index > 0) {
    NSInteger step = std::max<NSInteger>(1, table_view.indexPathsForVisibleRows.count - 1);
    selected_index = std::max<NSInteger>(0, selected_index - step);
    handled = YES;
  }
  if (actions.page_next && selected_index + 1 < static_cast<NSInteger>(all_paths.count)) {
    NSInteger step = std::max<NSInteger>(1, table_view.indexPathsForVisibleRows.count - 1);
    selected_index =
        std::min<NSInteger>(static_cast<NSInteger>(all_paths.count - 1), selected_index + step);
    handled = YES;
  }
  if (actions.section_prev && selected.section > 0) {
    for (NSInteger target_section = selected.section - 1; target_section >= 0; --target_section) {
      NSInteger rows = [table_view numberOfRowsInSection:target_section];
      if (rows > 0) {
        selected = [NSIndexPath indexPathForRow:0 inSection:target_section];
        handled = YES;
        break;
      }
    }
  } else if (actions.section_next && selected.section + 1 < sections) {
    for (NSInteger target_section = selected.section + 1; target_section < sections;
         ++target_section) {
      NSInteger rows = [table_view numberOfRowsInSection:target_section];
      if (rows > 0) {
        selected = [NSIndexPath indexPathForRow:0 inSection:target_section];
        handled = YES;
        break;
      }
    }
  } else {
    selected = all_paths[selected_index];
  }

  if (handled && selected) {
    [table_view selectRowAtIndexPath:selected
                            animated:YES
                      scrollPosition:UITableViewScrollPositionMiddle];
  }

  if (actions.accept && selected) {
    UITableViewCell* cell = [table_view cellForRowAtIndexPath:selected];
    if ([cell.accessoryView isKindOfClass:[UISwitch class]]) {
      UISwitch* toggle = (UISwitch*)cell.accessoryView;
      [toggle setOn:!toggle.isOn animated:YES];
      [toggle sendActionsForControlEvents:UIControlEventValueChanged];
    } else {
      id<UITableViewDelegate> delegate = table_view.delegate;
      if ([delegate respondsToSelector:@selector(tableView:didSelectRowAtIndexPath:)]) {
        [delegate tableView:table_view didSelectRowAtIndexPath:selected];
      }
    }
    handled = YES;
  }

  if (actions.back) {
    UINavigationController* nav = table_controller.navigationController;
    if (nav && nav.viewControllers.count > 1) {
      [nav popViewControllerAnimated:YES];
    } else {
      [table_controller dismissViewControllerAnimated:YES completion:nil];
    }
    handled = YES;
  }

  return handled;
}

- (BOOL)handlePresentedControllerActions:(const xe::ui::apple::ControllerActionSet&)actions {
  UIViewController* presented = self.presentedViewController;
  if (!presented) {
    return NO;
  }

  if ([presented isKindOfClass:[UIAlertController class]]) {
    if (actions.back) {
      [presented dismissViewControllerAnimated:YES completion:nil];
      return YES;
    }
    return NO;
  }

  if ([presented isKindOfClass:[UINavigationController class]]) {
    UINavigationController* nav = (UINavigationController*)presented;
    UIViewController* top = nav.topViewController;
    if ([top isKindOfClass:[XeniaLogViewController class]] &&
        [(XeniaLogViewController*)top handleControllerActions:actions]) {
      return YES;
    }
    if ([top isKindOfClass:[UITableViewController class]] &&
        [self handleControllerActionsForTableController:(UITableViewController*)top
                                                actions:actions]) {
      return YES;
    }
    if (actions.back) {
      if (nav.viewControllers.count > 1) {
        [nav popViewControllerAnimated:YES];
      } else {
        [nav dismissViewControllerAnimated:YES completion:nil];
      }
      return YES;
    }
    return NO;
  }

  if (actions.back) {
    [presented dismissViewControllerAnimated:YES completion:nil];
    return YES;
  }
  return NO;
}

- (BOOL)handleLauncherControllerActions:(const xe::ui::apple::ControllerActionSet&)actions {
  if (self.launcherOverlay.hidden) {
    return NO;
  }

  [self rebuildLauncherFocusGraph];
  IOSFocusNodeId current_focus = launcher_focus_graph_.current();
  NSInteger game_count = static_cast<NSInteger>(discovered_games_.size());
  BOOL handled = NO;
  BOOL focus_changed = NO;

  auto move_focus = [&](xe::ui::apple::NavigationDirection direction) {
    IOSFocusNodeId previous = launcher_focus_graph_.current();
    IOSFocusNodeId next = launcher_focus_graph_.Move(direction);
    if (next != previous) {
      focus_changed = YES;
    }
  };

  if (actions.section_prev) {
    IOSFocusNodeId target =
        current_focus == kLauncherFocusLibrary ? kLauncherFocusImport : kLauncherFocusSettings;
    if (launcher_focus_graph_.SetCurrent(target)) {
      focus_changed = YES;
    }
    handled = YES;
  }
  if (actions.section_next && game_count > 0) {
    if (launcher_focus_graph_.SetCurrent(kLauncherFocusLibrary)) {
      focus_changed = YES;
    }
    handled = YES;
  }

  current_focus = launcher_focus_graph_.current();
  if (current_focus == kLauncherFocusLibrary && game_count > 0) {
    NSInteger columns = [self launcherGridColumnCount];
    NSInteger next_index = focused_game_index_ < 0 ? 0 : focused_game_index_;

    if (actions.navigate_left) {
      if (next_index % columns == 0) {
        move_focus(xe::ui::apple::NavigationDirection::kLeft);
      } else if (next_index > 0) {
        next_index--;
      }
      handled = YES;
    }
    if (actions.navigate_right) {
      if (next_index + 1 < game_count) {
        next_index++;
      }
      handled = YES;
    }
    if (actions.navigate_up) {
      if (next_index - columns >= 0) {
        next_index -= columns;
      } else {
        move_focus(xe::ui::apple::NavigationDirection::kUp);
      }
      handled = YES;
    }
    if (actions.navigate_down) {
      if (next_index + columns < game_count) {
        next_index += columns;
      }
      handled = YES;
    }
    if (actions.page_prev) {
      NSInteger page_step = [self launcherPageStep];
      next_index = std::max<NSInteger>(0, next_index - page_step);
      handled = YES;
    }
    if (actions.page_next) {
      NSInteger page_step = [self launcherPageStep];
      next_index = std::min<NSInteger>(game_count - 1, next_index + page_step);
      handled = YES;
    }

    if (launcher_focus_graph_.current() == kLauncherFocusLibrary &&
        next_index != focused_game_index_) {
      [self setFocusedGameIndex:next_index scroll:YES];
      handled = YES;
    }
  } else {
    if (actions.navigate_up) {
      move_focus(xe::ui::apple::NavigationDirection::kUp);
      handled = YES;
    }
    if (actions.navigate_down) {
      move_focus(xe::ui::apple::NavigationDirection::kDown);
      handled = YES;
    }
    if (actions.navigate_left) {
      move_focus(xe::ui::apple::NavigationDirection::kLeft);
      handled = YES;
    }
    if (actions.navigate_right) {
      move_focus(xe::ui::apple::NavigationDirection::kRight);
      handled = YES;
    }
  }

  if (actions.context) {
    if (launcher_focus_graph_.current() == kLauncherFocusLibrary && focused_game_index_ >= 0 &&
        focused_game_index_ < game_count) {
      [self presentManageContentSheetForIndex:static_cast<size_t>(focused_game_index_)];
    } else {
      [self openProfileTapped:self.profileButton];
    }
    handled = YES;
  }
  if (actions.quick_action) {
    [self openGameTapped:self.openGameButton];
    handled = YES;
  }
  if (actions.guide) {
    [self openSettingsTapped:self.settingsButton];
    handled = YES;
  }

  if (actions.accept) {
    switch (launcher_focus_graph_.current()) {
      case kLauncherFocusSettings:
        [self openSettingsTapped:self.settingsButton];
        handled = YES;
        break;
      case kLauncherFocusProfile:
        [self openProfileTapped:self.profileButton];
        handled = YES;
        break;
      case kLauncherFocusImport:
        [self openGameTapped:self.openGameButton];
        handled = YES;
        break;
      case kLauncherFocusLibrary:
        if (focused_game_index_ >= 0 && focused_game_index_ < game_count) {
          const IOSDiscoveredGame& game = discovered_games_[focused_game_index_];
          [self launchGameAtPath:game.path displayName:ToNSString(game.title)];
          handled = YES;
        }
        break;
      default:
        break;
    }
  }

  if (focus_changed || handled) {
    [self applyLauncherFocusVisuals];
  }
  return handled;
}

- (BOOL)handleInGameControllerActions:(const xe::ui::apple::ControllerActionSet&)actions {
  if (self.launcherOverlay.hidden == NO || !self.gameRunning) {
    return NO;
  }

  if (actions.guide) {
    if (self.inGameMenuOverlay.hidden) {
      self.inGameMenuOverlay.alpha = 0.0;
      self.inGameMenuOverlay.hidden = NO;
      [self rebuildInGameFocusGraph];
      in_game_focus_graph_.SetCurrent(kInGameFocusResume);
      [self applyInGameMenuFocusVisuals];
      [UIView animateWithDuration:0.18
                       animations:^{
                         self.inGameMenuOverlay.alpha = 1.0;
                       }];
    } else {
      [self hideInGameMenuOverlay];
    }
    return YES;
  }

  if (self.inGameMenuOverlay.hidden) {
    return NO;
  }

  if (actions.back) {
    [self hideInGameMenuOverlay];
    return YES;
  }

  [self rebuildInGameFocusGraph];
  if (in_game_focus_graph_.current() == xe::ui::apple::kInvalidFocusNodeId) {
    in_game_focus_graph_.SetCurrent(kInGameFocusResume);
  }

  BOOL handled = NO;
  BOOL focus_changed = NO;
  auto move_focus = [&](xe::ui::apple::NavigationDirection direction) {
    IOSFocusNodeId previous = in_game_focus_graph_.current();
    IOSFocusNodeId next = in_game_focus_graph_.Move(direction);
    if (next != previous) {
      focus_changed = YES;
    }
  };

  if (actions.navigate_up) {
    move_focus(xe::ui::apple::NavigationDirection::kUp);
    handled = YES;
  }
  if (actions.navigate_down) {
    move_focus(xe::ui::apple::NavigationDirection::kDown);
    handled = YES;
  }
  if (actions.navigate_left) {
    move_focus(xe::ui::apple::NavigationDirection::kUp);
    handled = YES;
  }
  if (actions.navigate_right) {
    move_focus(xe::ui::apple::NavigationDirection::kDown);
    handled = YES;
  }

  if (actions.section_prev && in_game_focus_graph_.SetCurrent(kInGameFocusResume)) {
    focus_changed = YES;
    handled = YES;
  }
  if (actions.section_next && in_game_focus_graph_.SetCurrent(kInGameFocusExit)) {
    focus_changed = YES;
    handled = YES;
  }

  if (actions.context) {
    [self inGameSettingsTapped:self.inGameSettingsButton];
    handled = YES;
  }
  if (actions.quick_action) {
    [self inGameLiveLogTapped:self.inGameLiveLogButton];
    handled = YES;
  }

  if (actions.accept) {
    switch (in_game_focus_graph_.current()) {
      case kInGameFocusResume:
        [self.inGameResumeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
        break;
      case kInGameFocusSettings:
        [self.inGameSettingsButton sendActionsForControlEvents:UIControlEventTouchUpInside];
        break;
      case kInGameFocusLog:
        [self.inGameLiveLogButton sendActionsForControlEvents:UIControlEventTouchUpInside];
        break;
      case kInGameFocusExit:
        [self.inGameExitButton sendActionsForControlEvents:UIControlEventTouchUpInside];
        break;
      default:
        break;
    }
    handled = YES;
  }

  if (focus_changed || handled) {
    [self applyInGameMenuFocusVisuals];
  }
  return handled;
}

- (void)pollControllerNavigation:(NSTimer* __unused)timer {
  const bool navigation_enabled = cvars::ui_controller_navigation;
  if (!navigation_enabled) {
    if (controller_navigation_was_enabled_) {
      controller_navigation_was_enabled_ = NO;
      controller_navigation_mapper_.Reset();
      launcher_focus_graph_.Clear();
      in_game_focus_graph_.Clear();
      [self applyLauncherFocusVisuals];
      [self applyInGameMenuFocusVisuals];
    }
    return;
  }

  if (!controller_navigation_was_enabled_) {
    controller_navigation_was_enabled_ = YES;
    [self rebuildLauncherFocusGraph];
    [self applyLauncherFocusVisuals];
  }

  xe::hid::X_INPUT_STATE state = {};
  bool has_state = false;
  if (self.appContext) {
    for (uint32_t user_index = 0; user_index < xe::XUserMaxUserCount; ++user_index) {
      if (self.appContext->GetControllerState(user_index, &state)) {
        has_state = true;
        break;
      }
    }
  }
  if (!has_state) {
    has_state = [self readNativeControllerState:&state];
  }
  if (!has_state) {
    controller_navigation_mapper_.Reset();
    return;
  }

  xe::ui::apple::ControllerActionSet actions =
      controller_navigation_mapper_.Update(state, GetNowMs());
  if (!actions.Any()) {
    return;
  }

  if ([self handlePresentedControllerActions:actions]) {
    return;
  }
  if ([self handleLauncherControllerActions:actions]) {
    return;
  }
  [self handleInGameControllerActions:actions];
}

- (BOOL)readNativeControllerState:(xe::hid::X_INPUT_STATE*)out_state {
  if (!out_state) {
    return NO;
  }

  NSArray<GCController*>* controllers = [GCController controllers];
  for (GCController* controller in controllers) {
    GCExtendedGamepad* gamepad = controller.extendedGamepad;
    if (!gamepad) {
      continue;
    }

    uint16_t buttons = 0;
    auto set_button = [&buttons](BOOL pressed, uint16_t mask) {
      if (pressed) {
        buttons |= mask;
      }
    };

    set_button(gamepad.dpad.up.pressed, xe::hid::X_INPUT_GAMEPAD_DPAD_UP);
    set_button(gamepad.dpad.down.pressed, xe::hid::X_INPUT_GAMEPAD_DPAD_DOWN);
    set_button(gamepad.dpad.left.pressed, xe::hid::X_INPUT_GAMEPAD_DPAD_LEFT);
    set_button(gamepad.dpad.right.pressed, xe::hid::X_INPUT_GAMEPAD_DPAD_RIGHT);
    set_button(gamepad.buttonA.pressed, xe::hid::X_INPUT_GAMEPAD_A);
    set_button(gamepad.buttonB.pressed, xe::hid::X_INPUT_GAMEPAD_B);
    set_button(gamepad.buttonX.pressed, xe::hid::X_INPUT_GAMEPAD_X);
    set_button(gamepad.buttonY.pressed, xe::hid::X_INPUT_GAMEPAD_Y);
    set_button(gamepad.leftShoulder.pressed, xe::hid::X_INPUT_GAMEPAD_LEFT_SHOULDER);
    set_button(gamepad.rightShoulder.pressed, xe::hid::X_INPUT_GAMEPAD_RIGHT_SHOULDER);

    if (@available(iOS 13.0, tvOS 13.0, macCatalyst 13.0, *)) {
      set_button(gamepad.buttonMenu.pressed, xe::hid::X_INPUT_GAMEPAD_START);
    }
    if (@available(iOS 14.0, tvOS 14.0, macCatalyst 14.0, *)) {
      set_button(gamepad.buttonOptions.pressed, xe::hid::X_INPUT_GAMEPAD_BACK);
    }
    if (@available(iOS 12.1, tvOS 12.1, macCatalyst 13.1, *)) {
      set_button(gamepad.leftThumbstickButton.pressed, xe::hid::X_INPUT_GAMEPAD_LEFT_THUMB);
      set_button(gamepad.rightThumbstickButton.pressed, xe::hid::X_INPUT_GAMEPAD_RIGHT_THUMB);
    }

    out_state->packet_number = ++native_controller_packet_number_;
    out_state->gamepad.buttons = buttons;
    out_state->gamepad.left_trigger = ToTriggerAxis(gamepad.leftTrigger.value);
    out_state->gamepad.right_trigger = ToTriggerAxis(gamepad.rightTrigger.value);
    out_state->gamepad.thumb_lx = ToThumbAxis(gamepad.leftThumbstick.xAxis.value);
    out_state->gamepad.thumb_ly = ToThumbAxis(gamepad.leftThumbstick.yAxis.value);
    out_state->gamepad.thumb_rx = ToThumbAxis(gamepad.rightThumbstick.xAxis.value);
    out_state->gamepad.thumb_ry = ToThumbAxis(gamepad.rightThumbstick.yAxis.value);
    return YES;
  }
  return NO;
}

// ---------------------------------------------------------------------------
// JIT polling -- checks every 0.5s until JIT is available.
// ---------------------------------------------------------------------------
- (void)startJITPoll {
  // Check immediately first.
  if (xe_check_jit_available()) {
    [self onJITAcquired];
    return;
  }

  XELOGI("iOS: JIT not yet available, polling...");
  self.jitPollTimer = [NSTimer scheduledTimerWithTimeInterval:0.5
                                                       target:self
                                                     selector:@selector(pollJIT:)
                                                     userInfo:nil
                                                      repeats:YES];
}

- (void)pollJIT:(NSTimer*)timer {
  if (xe_check_jit_available()) {
    [timer invalidate];
    self.jitPollTimer = nil;
    [self onJITAcquired];
  }
}

- (void)onJITAcquired {
  self.jitAcquired = YES;
  XELOGI("iOS: JIT acquired!");
  [self updateJITStatusIndicator];
  [self updateJITAvailabilityUI];

  std::filesystem::path queued_path = pending_external_launch_path_;
  std::filesystem::path persisted_path = TakePendingExternalLaunchPathPreference();
  pending_external_launch_path_.clear();

  if (!queued_path.empty() || !persisted_path.empty()) {
    std::filesystem::path path_to_launch = !queued_path.empty() ? queued_path : persisted_path;
    NSString* display_name = [self displayNameForGamePath:path_to_launch];
    if (!display_name || display_name.length == 0) {
      display_name = ToNSString(path_to_launch.filename().string());
    }
    XELOGI("iOS: Launching queued external request: {}", path_to_launch.string());
    [self launchGameAtPath:path_to_launch displayName:display_name];
  }
}

// ---------------------------------------------------------------------------
// Launcher overlay — content-first design matching xenios-website aesthetic.
// ---------------------------------------------------------------------------
- (void)setupLauncherOverlay {
  self.launcherOverlay = [[UIView alloc] initWithFrame:self.view.bounds];
  self.launcherOverlay.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.launcherOverlay.backgroundColor = [XeniaTheme bgPrimary];
  [self.view addSubview:self.launcherOverlay];
  UILayoutGuide* safe = self.launcherOverlay.safeAreaLayoutGuide;
  CGFloat hPad = 16.0;

  // ── Nav bar: XeniOS (left) · gear + profile (right) ────────────────────

  self.titleLabel = [[UILabel alloc] init];
  {
    UIFont* titleFont = [UIFont systemFontOfSize:22 weight:UIFontWeightBold];
    NSDictionary* whiteAttrs = @{
      NSFontAttributeName : titleFont,
      NSForegroundColorAttributeName : [XeniaTheme textPrimary],
      NSKernAttributeName : @(0.8),
    };
    NSDictionary* accentAttrs = @{
      NSFontAttributeName : titleFont,
      NSForegroundColorAttributeName : [XeniaTheme accent],
      NSKernAttributeName : @(0.8),
    };
    NSMutableAttributedString* title =
        [[NSMutableAttributedString alloc] initWithString:@"Xeni" attributes:whiteAttrs];
    NSAttributedString* title_suffix = [[NSAttributedString alloc] initWithString:@"OS"
                                                                       attributes:accentAttrs];
    [title appendAttributedString:title_suffix];
    [title_suffix release];
    self.titleLabel.attributedText = title;
    [title release];
  }
  self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.launcherOverlay addSubview:self.titleLabel];

  UIButtonConfiguration* settingsCfg = [UIButtonConfiguration plainButtonConfiguration];
  settingsCfg.image =
      [UIImage systemImageNamed:@"gearshape"
              withConfiguration:[UIImageSymbolConfiguration
                                    configurationWithPointSize:20
                                                        weight:UIImageSymbolWeightRegular]];
  settingsCfg.baseForegroundColor = [XeniaTheme textMuted];
  settingsCfg.contentInsets = NSDirectionalEdgeInsetsMake(8, 8, 8, 8);
  self.settingsButton = [UIButton buttonWithConfiguration:settingsCfg primaryAction:nil];
  self.settingsButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.settingsButton addTarget:self
                          action:@selector(openSettingsTapped:)
                forControlEvents:UIControlEventTouchUpInside];
  [self.launcherOverlay addSubview:self.settingsButton];

  UIButtonConfiguration* profileCfg = [UIButtonConfiguration plainButtonConfiguration];
  profileCfg.image =
      [UIImage systemImageNamed:@"person.circle"
              withConfiguration:[UIImageSymbolConfiguration
                                    configurationWithPointSize:20
                                                        weight:UIImageSymbolWeightRegular]];
  profileCfg.baseForegroundColor = [XeniaTheme textMuted];
  profileCfg.contentInsets = NSDirectionalEdgeInsetsMake(8, 8, 8, 8);
  self.profileButton = [UIButton buttonWithConfiguration:profileCfg primaryAction:nil];
  self.profileButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.profileButton addTarget:self
                         action:@selector(openProfileTapped:)
               forControlEvents:UIControlEventTouchUpInside];
  [self.launcherOverlay addSubview:self.profileButton];

  // JIT status indicator — dot + ring pulse + label, shown when JIT active.
  self.jitReadyRing = [[UIView alloc] init];
  self.jitReadyRing.translatesAutoresizingMaskIntoConstraints = NO;
  self.jitReadyRing.backgroundColor = [UIColor clearColor];
  self.jitReadyRing.layer.cornerRadius = 7.0;
  self.jitReadyRing.layer.borderWidth = 1.25;
  self.jitReadyRing.layer.borderColor = [XeniaTheme accent].CGColor;
  self.jitReadyRing.alpha = 0;
  self.jitReadyRing.userInteractionEnabled = NO;
  [self.launcherOverlay addSubview:self.jitReadyRing];

  self.jitReadyDot = [[UIView alloc] init];
  self.jitReadyDot.translatesAutoresizingMaskIntoConstraints = NO;
  self.jitReadyDot.backgroundColor = [XeniaTheme accent];
  self.jitReadyDot.layer.cornerRadius = 4.0;
  [self.launcherOverlay addSubview:self.jitReadyDot];

  self.jitReadyLabel = [[UILabel alloc] init];
  self.jitReadyLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.jitReadyLabel.text = @"JIT Enabled";
  self.jitReadyLabel.textColor = [XeniaTheme textSecondary];
  self.jitReadyLabel.font = [UIFont systemFontOfSize:11 weight:UIFontWeightSemibold];
  [self.launcherOverlay addSubview:self.jitReadyLabel];

  // Thin separator below the nav bar.
  UIView* navSep = [[UIView alloc] init];
  navSep.translatesAutoresizingMaskIntoConstraints = NO;
  navSep.backgroundColor = [XeniaTheme border];
  [self.launcherOverlay addSubview:navSep];

  // ── Compact JIT notice row shown only when JIT is not enabled ──────────

  self.jitWarningCard = [[UIView alloc] init];
  self.jitWarningCard.translatesAutoresizingMaskIntoConstraints = NO;
  self.jitWarningCard.backgroundColor = [XeniaTheme bgSurface];
  self.jitWarningCard.layer.cornerRadius = 10.0;
  self.jitWarningCard.layer.borderWidth = 0.5;
  self.jitWarningCard.layer.borderColor = [XeniaTheme border].CGColor;

  self.jitStatusRing = [[UIView alloc] init];
  self.jitStatusRing.translatesAutoresizingMaskIntoConstraints = NO;
  self.jitStatusRing.backgroundColor = [UIColor clearColor];
  self.jitStatusRing.layer.cornerRadius = 6.0;
  self.jitStatusRing.layer.borderWidth = 1.25;
  self.jitStatusRing.layer.borderColor = [XeniaTheme statusError].CGColor;
  self.jitStatusRing.alpha = 0;
  self.jitStatusRing.userInteractionEnabled = NO;
  [self.jitWarningCard addSubview:self.jitStatusRing];

  self.jitStatusDot = [[UIView alloc] init];
  self.jitStatusDot.translatesAutoresizingMaskIntoConstraints = NO;
  self.jitStatusDot.backgroundColor = [XeniaTheme statusError];
  self.jitStatusDot.layer.cornerRadius = 3.5;
  [self.jitWarningCard addSubview:self.jitStatusDot];

  self.jitStatusLabel = [[UILabel alloc] init];
  self.jitStatusLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.jitStatusLabel.text = xe_jit_waiting_status_message();
  self.jitStatusLabel.textColor = [XeniaTheme textPrimary];
  xe_apply_label_font(self.jitStatusLabel, UIFontTextStyleSubheadline, 13.0, UIFontWeightMedium);
  self.jitStatusLabel.numberOfLines = 0;
  [self.jitWarningCard addSubview:self.jitStatusLabel];

  // ── Library header: "Library" (left) + "+" (right) ─────────────────────

  UIView* libraryRow = [[UIView alloc] init];
  libraryRow.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* libraryLabel = [[UILabel alloc] init];
  libraryLabel.translatesAutoresizingMaskIntoConstraints = NO;
  libraryLabel.text = @"Library";
  libraryLabel.textColor = [XeniaTheme textPrimary];
  libraryLabel.font = [UIFont systemFontOfSize:20 weight:UIFontWeightSemibold];
  [libraryRow addSubview:libraryLabel];

  UIButtonConfiguration* importCfg = [UIButtonConfiguration plainButtonConfiguration];
  importCfg.image =
      [UIImage systemImageNamed:@"plus"
              withConfiguration:[UIImageSymbolConfiguration
                                    configurationWithPointSize:20
                                                        weight:UIImageSymbolWeightMedium]];
  importCfg.baseForegroundColor = [XeniaTheme accent];
  importCfg.contentInsets = NSDirectionalEdgeInsetsMake(6, 6, 6, 6);
  self.openGameButton = [UIButton buttonWithConfiguration:importCfg primaryAction:nil];
  self.openGameButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.openGameButton addTarget:self
                          action:@selector(openGameTapped:)
                forControlEvents:UIControlEventTouchUpInside];
  [libraryRow addSubview:self.openGameButton];

  // ── Collapsible stack: JIT banner → library header ─────────────────────

  self.topInfoStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ self.jitWarningCard, libraryRow ]];
  self.topInfoStack.axis = UILayoutConstraintAxisVertical;
  self.topInfoStack.spacing = 12;
  self.topInfoStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.launcherOverlay addSubview:self.topInfoStack];

  // ── Games grid ─────────────────────────────────────────────────────────

  UICollectionViewFlowLayout* gridLayout = [[UICollectionViewFlowLayout alloc] init];
  gridLayout.minimumInteritemSpacing = 16;
  gridLayout.minimumLineSpacing = 20;
  gridLayout.sectionInset = UIEdgeInsetsZero;
  self.importedGamesCollectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                                        collectionViewLayout:gridLayout];
  self.importedGamesCollectionView.translatesAutoresizingMaskIntoConstraints = NO;
  self.importedGamesCollectionView.dataSource = self;
  self.importedGamesCollectionView.delegate = self;
  if (@available(iOS 11.0, *)) {
    self.importedGamesCollectionView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
  self.importedGamesCollectionView.backgroundColor = [UIColor clearColor];
  self.importedGamesCollectionView.alwaysBounceVertical = YES;
  [self.importedGamesCollectionView registerClass:[XeniaGameTileCell class]
                       forCellWithReuseIdentifier:@"ImportedGameCell"];
  [self.launcherOverlay addSubview:self.importedGamesCollectionView];

  // Empty-state label.
  UIView* emptyBg = [[UIView alloc] initWithFrame:CGRectZero];
  emptyBg.frame = self.importedGamesCollectionView.bounds;
  emptyBg.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.importedGamesEmptyLabel = [[UILabel alloc] init];
  self.importedGamesEmptyLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.importedGamesEmptyLabel.text =
      @"No games yet.\nTransfer ISO or GOD files to the\nDocuments folder, or tap +.";
  self.importedGamesEmptyLabel.textColor = [XeniaTheme textMuted];
  self.importedGamesEmptyLabel.font = [UIFont systemFontOfSize:15 weight:UIFontWeightRegular];
  self.importedGamesEmptyLabel.textAlignment = NSTextAlignmentCenter;
  self.importedGamesEmptyLabel.numberOfLines = 0;
  [emptyBg addSubview:self.importedGamesEmptyLabel];
  NSLayoutConstraint* empty_label_leading = [self.importedGamesEmptyLabel.leadingAnchor
      constraintGreaterThanOrEqualToAnchor:emptyBg.leadingAnchor
                                  constant:32];
  empty_label_leading.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* empty_label_trailing = [self.importedGamesEmptyLabel.trailingAnchor
      constraintLessThanOrEqualToAnchor:emptyBg.trailingAnchor
                               constant:-32];
  empty_label_trailing.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    [self.importedGamesEmptyLabel.centerXAnchor constraintEqualToAnchor:emptyBg.centerXAnchor],
    [self.importedGamesEmptyLabel.centerYAnchor constraintEqualToAnchor:emptyBg.centerYAnchor],
    empty_label_leading,
    empty_label_trailing,
  ]];
  self.importedGamesCollectionView.backgroundView = emptyBg;

  // ── Status label (ephemeral, bottom overlay) ─────────────────────────────

  self.statusLabel = [[UILabel alloc] init];
  self.statusLabel.text = @"";
  self.statusLabel.textColor = [XeniaTheme textMuted];
  self.statusLabel.font = [UIFont systemFontOfSize:11 weight:UIFontWeightRegular];
  self.statusLabel.textAlignment = NSTextAlignmentCenter;
  self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.launcherOverlay addSubview:self.statusLabel];

  // Allocated but off-screen — existing code can set .text without crashing.
  self.signedInProfileLabel = [[UILabel alloc] init];

  // ── Layout ─────────────────────────────────────────────────────────────

  [NSLayoutConstraint activateConstraints:@[
    // Nav bar.
    [self.titleLabel.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:hPad],
    [self.titleLabel.topAnchor constraintEqualToAnchor:safe.topAnchor constant:6],
    [self.profileButton.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-8],
    [self.profileButton.centerYAnchor constraintEqualToAnchor:self.titleLabel.centerYAnchor],
    [self.settingsButton.trailingAnchor constraintEqualToAnchor:self.profileButton.leadingAnchor
                                                       constant:-2],
    [self.settingsButton.centerYAnchor constraintEqualToAnchor:self.titleLabel.centerYAnchor],
    // JIT ready dot + ring + label sit right after the title.
    [self.jitReadyDot.leadingAnchor constraintEqualToAnchor:self.titleLabel.trailingAnchor
                                                   constant:10],
    [self.jitReadyDot.centerYAnchor constraintEqualToAnchor:self.titleLabel.centerYAnchor],
    [self.jitReadyDot.widthAnchor constraintEqualToConstant:8],
    [self.jitReadyDot.heightAnchor constraintEqualToConstant:8],
    [self.jitReadyRing.centerXAnchor constraintEqualToAnchor:self.jitReadyDot.centerXAnchor],
    [self.jitReadyRing.centerYAnchor constraintEqualToAnchor:self.jitReadyDot.centerYAnchor],
    [self.jitReadyRing.widthAnchor constraintEqualToConstant:14],
    [self.jitReadyRing.heightAnchor constraintEqualToConstant:14],
    [self.jitReadyLabel.leadingAnchor constraintEqualToAnchor:self.jitReadyDot.trailingAnchor
                                                     constant:6],
    [self.jitReadyLabel.centerYAnchor constraintEqualToAnchor:self.titleLabel.centerYAnchor],
    [self.jitReadyLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.settingsButton.leadingAnchor
                                 constant:-8],

    // Nav separator.
    [navSep.topAnchor constraintEqualToAnchor:self.titleLabel.bottomAnchor constant:6],
    [navSep.leadingAnchor constraintEqualToAnchor:self.launcherOverlay.leadingAnchor],
    [navSep.trailingAnchor constraintEqualToAnchor:self.launcherOverlay.trailingAnchor],
    [navSep.heightAnchor constraintEqualToConstant:0.5],

    // Header stack (JIT banner + library row) below separator.
    [self.topInfoStack.topAnchor constraintEqualToAnchor:navSep.bottomAnchor constant:12],
    [self.topInfoStack.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:hPad],
    [self.topInfoStack.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-hPad],

    // JIT banner internals.
    [self.jitStatusDot.leadingAnchor constraintEqualToAnchor:self.jitWarningCard.leadingAnchor
                                                    constant:14],
    [self.jitStatusDot.centerYAnchor constraintEqualToAnchor:self.jitWarningCard.centerYAnchor],
    [self.jitStatusDot.widthAnchor constraintEqualToConstant:7],
    [self.jitStatusDot.heightAnchor constraintEqualToConstant:7],
    [self.jitStatusRing.centerXAnchor constraintEqualToAnchor:self.jitStatusDot.centerXAnchor],
    [self.jitStatusRing.centerYAnchor constraintEqualToAnchor:self.jitStatusDot.centerYAnchor],
    [self.jitStatusRing.widthAnchor constraintEqualToConstant:12],
    [self.jitStatusRing.heightAnchor constraintEqualToConstant:12],
    [self.jitStatusLabel.leadingAnchor constraintEqualToAnchor:self.jitStatusDot.trailingAnchor
                                                      constant:8],
    [self.jitStatusLabel.trailingAnchor constraintEqualToAnchor:self.jitWarningCard.trailingAnchor
                                                       constant:-14],
    [self.jitStatusLabel.topAnchor constraintEqualToAnchor:self.jitWarningCard.topAnchor
                                                  constant:10],
    [self.jitStatusLabel.bottomAnchor constraintEqualToAnchor:self.jitWarningCard.bottomAnchor
                                                     constant:-10],

    // Library row internals.
    [libraryLabel.leadingAnchor constraintEqualToAnchor:libraryRow.leadingAnchor],
    [libraryLabel.centerYAnchor constraintEqualToAnchor:libraryRow.centerYAnchor],
    [self.openGameButton.trailingAnchor constraintEqualToAnchor:libraryRow.trailingAnchor],
    [self.openGameButton.centerYAnchor constraintEqualToAnchor:libraryRow.centerYAnchor],
    [libraryRow.heightAnchor constraintEqualToConstant:34],

    // Games grid.
    [self.importedGamesCollectionView.topAnchor
        constraintEqualToAnchor:self.topInfoStack.bottomAnchor
                       constant:8],
    [self.importedGamesCollectionView.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor
                                                                   constant:hPad],
    [self.importedGamesCollectionView.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor
                                                                    constant:-hPad],
    [self.importedGamesCollectionView.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],

    // Status label.
    [self.statusLabel.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
    [self.statusLabel.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-6],
  ]];
}

- (void)setupInGameMenuOverlay {
  self.inGameMenuOverlay = [[UIView alloc] initWithFrame:self.view.bounds];
  self.inGameMenuOverlay.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.inGameMenuOverlay.backgroundColor = [XeniaTheme overlayLight];
  self.inGameMenuOverlay.hidden = YES;
  [self.view addSubview:self.inGameMenuOverlay];

  UIView* panel = [[UIView alloc] init];
  panel.translatesAutoresizingMaskIntoConstraints = NO;
  panel.backgroundColor = [XeniaTheme bgSurface];
  panel.layer.cornerRadius = XeniaRadiusXl;
  panel.layer.borderWidth = 1.0;
  panel.layer.borderColor = [XeniaTheme border].CGColor;
  [self.inGameMenuOverlay addSubview:panel];

  UILabel* title = [[UILabel alloc] init];
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.text = @"In-Game Menu";
  title.textColor = [XeniaTheme textPrimary];
  title.font = [UIFont systemFontOfSize:22 weight:UIFontWeightSemibold];
  title.textAlignment = NSTextAlignmentCenter;
  [panel addSubview:title];

  UILabel* subtitle = [[UILabel alloc] init];
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;
  subtitle.text = @"Tap anywhere to close";
  subtitle.textColor = [XeniaTheme textMuted];
  subtitle.font = [UIFont systemFontOfSize:15 weight:UIFontWeightRegular];
  subtitle.textAlignment = NSTextAlignmentCenter;
  [panel addSubview:subtitle];

  UIButtonConfiguration* resume_config = [UIButtonConfiguration filledButtonConfiguration];
  resume_config.title = @"Resume";
  resume_config.baseBackgroundColor = [XeniaTheme accent];
  resume_config.baseForegroundColor = [XeniaTheme accentFg];
  resume_config.cornerStyle = UIButtonConfigurationCornerStyleLarge;
  resume_config.contentInsets = NSDirectionalEdgeInsetsMake(12, 18, 12, 18);
  self.inGameResumeButton = [UIButton buttonWithConfiguration:resume_config primaryAction:nil];
  self.inGameResumeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.inGameResumeButton addTarget:self
                              action:@selector(resumeGameTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  [panel addSubview:self.inGameResumeButton];

  UIButtonConfiguration* settings_config = [UIButtonConfiguration tintedButtonConfiguration];
  settings_config.title = @"Settings";
  settings_config.image = [UIImage systemImageNamed:@"slider.horizontal.3"];
  settings_config.imagePadding = 6;
  settings_config.baseForegroundColor = [XeniaTheme textPrimary];
  settings_config.baseBackgroundColor = [XeniaTheme bgSurface2];
  settings_config.cornerStyle = UIButtonConfigurationCornerStyleLarge;
  settings_config.contentInsets = NSDirectionalEdgeInsetsMake(10, 16, 10, 16);
  self.inGameSettingsButton = [UIButton buttonWithConfiguration:settings_config primaryAction:nil];
  self.inGameSettingsButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.inGameSettingsButton addTarget:self
                                action:@selector(inGameSettingsTapped:)
                      forControlEvents:UIControlEventTouchUpInside];
  [panel addSubview:self.inGameSettingsButton];

  UIButtonConfiguration* live_log_config = [UIButtonConfiguration tintedButtonConfiguration];
  live_log_config.title = @"Live Log";
  live_log_config.image = [UIImage systemImageNamed:@"doc.text"];
  live_log_config.imagePadding = 6;
  live_log_config.baseForegroundColor = [XeniaTheme textPrimary];
  live_log_config.baseBackgroundColor = [XeniaTheme bgSurface2];
  live_log_config.cornerStyle = UIButtonConfigurationCornerStyleLarge;
  live_log_config.contentInsets = NSDirectionalEdgeInsetsMake(10, 16, 10, 16);
  self.inGameLiveLogButton = [UIButton buttonWithConfiguration:live_log_config primaryAction:nil];
  self.inGameLiveLogButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.inGameLiveLogButton addTarget:self
                               action:@selector(inGameLiveLogTapped:)
                     forControlEvents:UIControlEventTouchUpInside];
  [panel addSubview:self.inGameLiveLogButton];

  UIButtonConfiguration* exit_config = [UIButtonConfiguration tintedButtonConfiguration];
  exit_config.title = @"Exit To Library";
  exit_config.image = [UIImage systemImageNamed:@"rectangle.portrait.and.arrow.right"];
  exit_config.imagePadding = 6;
  exit_config.baseForegroundColor = [XeniaTheme textPrimary];
  exit_config.baseBackgroundColor = [[XeniaTheme statusError] colorWithAlphaComponent:0.25];
  exit_config.cornerStyle = UIButtonConfigurationCornerStyleLarge;
  exit_config.contentInsets = NSDirectionalEdgeInsetsMake(10, 16, 10, 16);
  self.inGameExitButton = [UIButton buttonWithConfiguration:exit_config primaryAction:nil];
  self.inGameExitButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.inGameExitButton addTarget:self
                            action:@selector(exitGameTapped:)
                  forControlEvents:UIControlEventTouchUpInside];
  [panel addSubview:self.inGameExitButton];

  [NSLayoutConstraint activateConstraints:@[
    [panel.centerXAnchor constraintEqualToAnchor:self.inGameMenuOverlay.centerXAnchor],
    [panel.centerYAnchor constraintEqualToAnchor:self.inGameMenuOverlay.centerYAnchor],
    [panel.leadingAnchor constraintGreaterThanOrEqualToAnchor:self.inGameMenuOverlay
                                                                  .safeAreaLayoutGuide.leadingAnchor
                                                     constant:24],
    [panel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.inGameMenuOverlay.safeAreaLayoutGuide.trailingAnchor
                                 constant:-24],
    [panel.widthAnchor constraintLessThanOrEqualToConstant:420],

    [title.topAnchor constraintEqualToAnchor:panel.topAnchor constant:18],
    [title.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:20],
    [title.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor constant:-20],

    [subtitle.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:4],
    [subtitle.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
    [subtitle.trailingAnchor constraintEqualToAnchor:title.trailingAnchor],

    [self.inGameResumeButton.topAnchor constraintEqualToAnchor:subtitle.bottomAnchor constant:16],
    [self.inGameResumeButton.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:14],
    [self.inGameResumeButton.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor
                                                           constant:-14],

    [self.inGameSettingsButton.topAnchor
        constraintEqualToAnchor:self.inGameResumeButton.bottomAnchor
                       constant:10],
    [self.inGameSettingsButton.leadingAnchor
        constraintEqualToAnchor:self.inGameResumeButton.leadingAnchor],
    [self.inGameSettingsButton.trailingAnchor
        constraintEqualToAnchor:self.inGameResumeButton.trailingAnchor],

    [self.inGameLiveLogButton.topAnchor
        constraintEqualToAnchor:self.inGameSettingsButton.bottomAnchor
                       constant:10],
    [self.inGameLiveLogButton.leadingAnchor
        constraintEqualToAnchor:self.inGameResumeButton.leadingAnchor],
    [self.inGameLiveLogButton.trailingAnchor
        constraintEqualToAnchor:self.inGameResumeButton.trailingAnchor],

    [self.inGameExitButton.topAnchor constraintEqualToAnchor:self.inGameLiveLogButton.bottomAnchor
                                                    constant:10],
    [self.inGameExitButton.leadingAnchor
        constraintEqualToAnchor:self.inGameResumeButton.leadingAnchor],
    [self.inGameExitButton.trailingAnchor
        constraintEqualToAnchor:self.inGameResumeButton.trailingAnchor],
    [self.inGameExitButton.bottomAnchor constraintEqualToAnchor:panel.bottomAnchor constant:-14],
  ]];
}

- (void)toggleInGameMenuTapped:(UITapGestureRecognizer*)recognizer {
  if (recognizer.state != UIGestureRecognizerStateRecognized) {
    return;
  }
  if (self.launcherOverlay.hidden == NO || !self.gameRunning || self.presentedViewController) {
    return;
  }

  BOOL should_show = self.inGameMenuOverlay.hidden;
  if (should_show) {
    [self rebuildInGameFocusGraph];
    in_game_focus_graph_.SetCurrent(kInGameFocusResume);
    self.inGameMenuOverlay.alpha = 0.0;
    self.inGameMenuOverlay.hidden = NO;
    [self applyInGameMenuFocusVisuals];
    [UIView animateWithDuration:0.18
                     animations:^{
                       self.inGameMenuOverlay.alpha = 1.0;
                     }];
  } else {
    [self hideInGameMenuOverlay];
  }
}

- (void)hideInGameMenuOverlay {
  if (self.inGameMenuOverlay.hidden) {
    return;
  }
  [UIView animateWithDuration:0.15
      animations:^{
        self.inGameMenuOverlay.alpha = 0.0;
      }
      completion:^(__unused BOOL finished) {
        self.inGameMenuOverlay.hidden = YES;
        self.inGameMenuOverlay.alpha = 1.0;
        [self applyInGameMenuFocusVisuals];
      }];
}

- (void)resumeGameTapped:(UIButton*)sender {
  [self hideInGameMenuOverlay];
}

- (void)inGameSettingsTapped:(UIButton*)sender {
  [self hideInGameMenuOverlay];
  [self openSettingsTapped:nil];
}

- (void)inGameLiveLogTapped:(UIButton*)sender {
  [self hideInGameMenuOverlay];
  XeniaLogViewController* log_vc = [[XeniaLogViewController alloc] init];
  XeniaLandscapeNavigationController* nav =
      [[XeniaLandscapeNavigationController alloc] initWithRootViewController:log_vc];
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
    nav.modalPresentationStyle = UIModalPresentationFormSheet;
    if (@available(iOS 15.0, *)) {
      UISheetPresentationController* sheet = nav.sheetPresentationController;
      sheet.detents = @[
        [UISheetPresentationControllerDetent mediumDetent],
        [UISheetPresentationControllerDetent largeDetent]
      ];
      sheet.prefersGrabberVisible = YES;
    }
  } else {
    nav.modalPresentationStyle = UIModalPresentationFullScreen;
  }
  [self presentViewController:nav animated:YES completion:nil];
}

- (void)exitGameTapped:(UIButton*)sender {
  [self hideInGameMenuOverlay];
  if (self.gameStopInProgress) {
    self.statusLabel.text = @"Stopping game... Please wait.";
    return;
  }
  if (!self.appContext) {
    self.statusLabel.text = @"No active game to stop.";
    return;
  }

  self.gameStopInProgress = YES;
  self.gameRunning = NO;
  self.launcherOverlay.hidden = NO;
  self.launcherOverlay.alpha = 1.0;
  xe_request_current_orientation(self);
  self.statusLabel.text = @"Stopping game...";

  xe::ui::IOSWindowedAppContext* app_context = self.appContext;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    BOOL requested_stop = app_context->TerminateCurrentGame() ? YES : NO;
    if (requested_stop) {
      return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      self.gameStopInProgress = NO;
      self.statusLabel.text = @"No active game to stop.";
    });
  });
}

- (void)refreshSignedInProfileUI {
  if (!self.appContext) {
    self.profileButton.enabled = NO;
    self.profileButton.alpha = 0.5;
    self.signedInProfileLabel.text = @"Profile system unavailable";
    return;
  }

  self.profileButton.enabled = YES;
  self.profileButton.alpha = 1.0;

  const auto profiles = self.appContext->ListProfiles();
  const xe::ui::IOSProfileSummary* signed_in_profile = nullptr;
  for (const auto& profile : profiles) {
    if (profile.signed_in) {
      signed_in_profile = &profile;
      break;
    }
  }

  if (signed_in_profile) {
    self.signedInProfileLabel.text =
        [NSString stringWithFormat:@"Signed in: %@", ToNSString(signed_in_profile->gamertag)];
  } else if (profiles.empty()) {
    self.signedInProfileLabel.text = @"No local profile yet";
  } else {
    self.signedInProfileLabel.text = @"No profile signed in";
  }
}

- (void)presentProfileCreateAlert {
  // Under MRC, `__weak` is unavailable; rely on block strong captures.
  // Use `__block` for the alert to avoid a retain-cycle: alert -> action -> handler -> alert.
  __block UIAlertController* create_alert =
      [UIAlertController alertControllerWithTitle:@"Create Profile"
                                          message:@"Enter a gamertag (1-15 characters)."
                                   preferredStyle:UIAlertControllerStyleAlert];
  [create_alert addTextFieldWithConfigurationHandler:^(UITextField* text_field) {
    text_field.placeholder = @"Gamertag";
    text_field.autocapitalizationType = UITextAutocapitalizationTypeWords;
    text_field.autocorrectionType = UITextAutocorrectionTypeNo;
    text_field.clearButtonMode = UITextFieldViewModeWhileEditing;
  }];
  [create_alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                                   style:UIAlertActionStyleCancel
                                                 handler:nil]];
  [create_alert
      addAction:[UIAlertAction
                    actionWithTitle:@"Create"
                              style:UIAlertActionStyleDefault
                            handler:^(__unused UIAlertAction* action) {
                              if (!self.appContext) {
                                return;
                              }
                              UITextField* text_field = create_alert.textFields.firstObject;
                              NSString* raw_text = text_field.text ?: @"";
                              NSString* trimmed =
                                  [raw_text stringByTrimmingCharactersInSet:
                                                [NSCharacterSet whitespaceAndNewlineCharacterSet]];
                              if (trimmed.length == 0 || !self.appContext) {
                                return;
                              }
                              auto* app_context = self.appContext;
                              if (!app_context) {
                                return;
                              }
                              self.statusLabel.text = @"Creating profile...";
                              NSString* gamertag = [[trimmed copy] autorelease];
                              create_alert = nil;
                              uint64_t xuid =
                                  app_context->CreateProfile(std::string([gamertag UTF8String]));
                              if (!xuid) {
                                UIAlertController* failure = [UIAlertController
                                    alertControllerWithTitle:@"Profile Not Created"
                                                     message:@"Profile could not be created. "
                                                             @"Please try again."
                                              preferredStyle:UIAlertControllerStyleAlert];
                                [failure addAction:[UIAlertAction
                                                       actionWithTitle:@"OK"
                                                                 style:UIAlertActionStyleCancel
                                                               handler:nil]];
                                [self presentViewController:failure animated:YES completion:nil];
                                self.statusLabel.text = @"Profile creation failed.";
                                return;
                              }
                              BOOL signed_in = app_context->SignInProfile(xuid);
                              if (!signed_in) {
                                self.statusLabel.text = @"Failed to sign in with the new profile.";
                                return;
                              }
                              [self refreshSignedInProfileUI];
                              self.statusLabel.text =
                                  [NSString stringWithFormat:@"Signed in as %@.", gamertag];
                            }]];
  [self presentViewController:create_alert animated:YES completion:nil];
}

- (void)openProfileTapped:(UIButton*)sender {
  if (!self.appContext) {
    return;
  }

  XeniaProfileViewController* profile_vc =
      [[XeniaProfileViewController alloc] initWithAppContext:self.appContext
                                                    onStatus:^(NSString* status_message) {
                                                      [self refreshSignedInProfileUI];
                                                      if (status_message.length > 0) {
                                                        self.statusLabel.text = status_message;
                                                      }
                                                    }];
  XeniaLandscapeNavigationController* nav =
      [[XeniaLandscapeNavigationController alloc] initWithRootViewController:profile_vc];
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
    nav.modalPresentationStyle = UIModalPresentationFormSheet;
    if (@available(iOS 15.0, *)) {
      UISheetPresentationController* sheet = nav.sheetPresentationController;
      sheet.detents = @[
        [UISheetPresentationControllerDetent mediumDetent],
        [UISheetPresentationControllerDetent largeDetent]
      ];
      sheet.prefersGrabberVisible = YES;
    }
    UIPopoverPresentationController* popover = nav.popoverPresentationController;
    if (popover) {
      popover.sourceView = sender ?: self.profileButton;
      popover.sourceRect = (sender ?: self.profileButton).bounds;
    }
  } else {
    nav.modalPresentationStyle = UIModalPresentationFullScreen;
  }
  [self presentViewController:nav animated:YES completion:nil];
}

- (void)presentSystemSigninPromptForUserIndex:(uint32_t)user_index
                                  usersNeeded:(uint32_t)users_needed
                                   completion:(void (^)(BOOL success))completion {
  if (!self.appContext) {
    if (completion) {
      completion(NO);
    }
    return;
  }

  __block BOOL finished = NO;
  void (^finish)(BOOL) = ^(BOOL success) {
    if (finished) {
      return;
    }
    finished = YES;
    [self refreshSignedInProfileUI];
    if (completion) {
      completion(success);
    }
  };

  auto profiles = self.appContext->ListProfiles();
  void (^present_create_alert)(void) = ^{
    if (!self.appContext) {
      finish(NO);
      return;
    }

    // Under MRC, use `__block` to avoid a retain-cycle: alert -> action -> handler -> alert.
    __block UIAlertController* create_alert =
        [UIAlertController alertControllerWithTitle:@"Create Profile"
                                            message:@"Enter a gamertag (1-15 characters)."
                                     preferredStyle:UIAlertControllerStyleAlert];
    [create_alert addTextFieldWithConfigurationHandler:^(UITextField* text_field) {
      text_field.placeholder = @"Gamertag";
      text_field.autocapitalizationType = UITextAutocapitalizationTypeWords;
      text_field.autocorrectionType = UITextAutocorrectionTypeNo;
      text_field.clearButtonMode = UITextFieldViewModeWhileEditing;
    }];
    [create_alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                                     style:UIAlertActionStyleCancel
                                                   handler:^(__unused UIAlertAction* action) {
                                                     finish(NO);
                                                   }]];
    [create_alert
        addAction:[UIAlertAction
                      actionWithTitle:@"Create"
                                style:UIAlertActionStyleDefault
                              handler:^(__unused UIAlertAction* action) {
                                UITextField* text_field = create_alert.textFields.firstObject;
                                NSString* raw_text = text_field.text ?: @"";
                                NSString* trimmed = [raw_text
                                    stringByTrimmingCharactersInSet:
                                        [NSCharacterSet whitespaceAndNewlineCharacterSet]];
                                if (trimmed.length == 0 || !self.appContext) {
                                  finish(NO);
                                  return;
                                }
                                auto* app_context = self.appContext;
                                if (!app_context) {
                                  finish(NO);
                                  return;
                                }
                                NSString* gamertag = [[trimmed copy] autorelease];
                                create_alert = nil;
                                uint64_t xuid =
                                    app_context->CreateProfile(std::string([gamertag UTF8String]));
                                if (!xuid) {
                                  if (self.statusLabel) {
                                    self.statusLabel.text =
                                        @"Profile could not be created. Please try again.";
                                  }
                                  finish(NO);
                                  return;
                                }
                                BOOL signed_in = app_context->SignInProfile(xuid);
                                if (signed_in) {
                                  self.statusLabel.text =
                                      [NSString stringWithFormat:@"Signed in as %@.", gamertag];
                                } else if (self.statusLabel) {
                                  self.statusLabel.text =
                                      @"Failed to sign in with the new profile.";
                                }
                                finish(signed_in);
                              }]];

    UIViewController* presenter = self;
    while (presenter.presentedViewController) {
      presenter = presenter.presentedViewController;
    }
    [presenter presentViewController:create_alert animated:YES completion:nil];
  };

  if (profiles.empty()) {
    present_create_alert();
    return;
  }

  NSString* message = [NSString stringWithFormat:@"Select profile (needs %u user%@).", users_needed,
                                                 users_needed == 1 ? @"" : @"s"];
  UIAlertController* sheet =
      [UIAlertController alertControllerWithTitle:@"Select Profile"
                                          message:message
                                   preferredStyle:UIAlertControllerStyleActionSheet];

  [sheet addAction:[UIAlertAction actionWithTitle:@"Create Profile"
                                            style:UIAlertActionStyleDefault
                                          handler:^(__unused UIAlertAction* action) {
                                            present_create_alert();
                                          }]];

  for (const auto& profile : profiles) {
    NSString* gamertag = ToNSString(profile.gamertag);
    NSString* title = gamertag;
    if (profile.signed_in) {
      title = [title stringByAppendingString:@" (Signed In)"];
    }
    uint64_t xuid = profile.xuid;
    [sheet addAction:[UIAlertAction actionWithTitle:title
                                              style:UIAlertActionStyleDefault
                                            handler:^(__unused UIAlertAction* action) {
                                              if (!self.appContext) {
                                                finish(NO);
                                                return;
                                              }
                                              BOOL signed_in = self.appContext->SignInProfile(xuid);
                                              finish(signed_in);
                                            }]];
  }

  [sheet addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(__unused UIAlertAction* action) {
                                            finish(NO);
                                          }]];

  UIPopoverPresentationController* popover = sheet.popoverPresentationController;
  if (popover) {
    popover.sourceView = self.view;
    popover.sourceRect =
        CGRectMake(CGRectGetMidX(self.view.bounds), CGRectGetMidY(self.view.bounds), 1.0, 1.0);
    popover.permittedArrowDirections = 0;
  }

  UIViewController* presenter = self;
  while (presenter.presentedViewController) {
    presenter = presenter.presentedViewController;
  }
  [presenter presentViewController:sheet animated:YES completion:nil];
}

- (void)presentSystemKeyboardPromptWithTitle:(NSString*)title
                                 description:(NSString*)description
                                 defaultText:(NSString*)default_text
                                  completion:(void (^)(BOOL cancelled, NSString* text))completion {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:title.length ? title : @"Input Required"
                                          message:description
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addTextFieldWithConfigurationHandler:^(UITextField* text_field) {
    text_field.text = default_text ?: @"";
    text_field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    text_field.autocorrectionType = UITextAutocorrectionTypeNo;
    text_field.clearButtonMode = UITextFieldViewModeWhileEditing;
  }];

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(__unused UIAlertAction* action) {
                                            if (completion) {
                                              completion(YES, @"");
                                            }
                                          }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:^(__unused UIAlertAction* action) {
                                            UITextField* text_field = alert.textFields.firstObject;
                                            NSString* text = text_field.text ?: @"";
                                            if (completion) {
                                              completion(NO, text);
                                            }
                                          }]];

  UIViewController* presenter = self;
  while (presenter.presentedViewController) {
    presenter = presenter.presentedViewController;
  }
  [presenter presentViewController:alert animated:YES completion:nil];
}

- (void)updateJITStatusIndicator {
  if (self.jitAcquired) {
    self.jitStatusDot.backgroundColor = [XeniaTheme accent];
    self.jitStatusLabel.text = @"JIT Enabled";
    // Show nav-bar ready indicator.
    self.jitReadyDot.hidden = NO;
    self.jitReadyLabel.hidden = NO;
    self.jitReadyLabel.text = @"JIT Enabled";
    [self.jitStatusRing.layer removeAllAnimations];
    self.jitStatusRing.alpha = 0;
    // Pulse animation on the nav-bar ring.
    xe_add_jit_ring_pulse(self.jitReadyRing.layer, @"xenia.jit.ready.pulse", 1.7, 0.5, 2.0);
  } else {
    self.jitStatusDot.backgroundColor = [XeniaTheme statusError];
    self.jitStatusLabel.text = xe_jit_waiting_status_message();
    self.jitReadyDot.hidden = YES;
    self.jitReadyLabel.hidden = YES;
    [self.jitReadyRing.layer removeAllAnimations];
    self.jitReadyRing.alpha = 0;
    // Pulse animation on the warning card ring.
    xe_add_jit_ring_pulse(self.jitStatusRing.layer, @"xenia.jit.warn.pulse", 1.55, 0.42, 1.9);
  }
}

- (void)updateJITAvailabilityUI {
  BOOL previous_hidden = self.jitWarningCard.hidden;
  BOOL jit_ready = self.jitAcquired;
  self.jitWarningCard.hidden = jit_ready;
  if (previous_hidden != self.jitWarningCard.hidden) {
    [self.importedGamesCollectionView.collectionViewLayout invalidateLayout];
  }
  self.openGameButton.enabled = YES;
  self.openGameButton.alpha = 1.0;
}

- (std::filesystem::path)importedGamesDirectory {
  return xe_get_ios_documents_path() / "games";
}

- (std::filesystem::path)importGameIntoLibrary:(NSURL*)source_url error:(NSError**)error {
  std::filesystem::path source_path([source_url.path UTF8String]);
  std::filesystem::path library_path = [self importedGamesDirectory];

  std::error_code ec;
  std::filesystem::create_directories(library_path, ec);
  if (ec) {
    if (error) {
      *error = [NSError
          errorWithDomain:@"XeniaIOSImport"
                     code:1001
                 userInfo:@{
                   NSLocalizedDescriptionKey : [NSString
                       stringWithFormat:@"Failed creating library folder: %s", ec.message().c_str()]
                 }];
    }
    return {};
  }

  auto weak_source = std::filesystem::weakly_canonical(source_path, ec);
  auto weak_library = std::filesystem::weakly_canonical(library_path, ec);
  if (!ec && weak_source.native().rfind(weak_library.native(), 0) == 0) {
    return weak_source;
  }

  std::filesystem::path destination = library_path / source_path.filename();
  std::filesystem::path stem = destination.stem();
  std::filesystem::path extension = destination.extension();
  for (int attempt = 2; std::filesystem::exists(destination); ++attempt) {
    destination =
        library_path / std::filesystem::path(stem.string() + " (" + std::to_string(attempt) + ")" +
                                             extension.string());
  }

  NSString* source_ns = source_url.path;
  NSString* destination_ns = ToNSString(destination.string());
  if (![[NSFileManager defaultManager] copyItemAtPath:source_ns
                                               toPath:destination_ns
                                                error:error]) {
    return {};
  }

  if (HasContentSidecarDataDirectory(source_path)) {
    std::filesystem::path source_sidecar = source_path;
    source_sidecar += ".data";
    std::filesystem::path destination_sidecar = destination;
    destination_sidecar += ".data";

    std::string error_message;
    if (!xe_copy_directory_recursive(source_sidecar, destination_sidecar, &error_message)) {
      std::error_code cleanup_error;
      std::filesystem::remove(destination, cleanup_error);
      std::filesystem::remove_all(destination_sidecar, cleanup_error);
      if (error) {
        *error = [NSError
            errorWithDomain:@"XeniaIOSImport"
                       code:1002
                   userInfo:@{
                     NSLocalizedDescriptionKey : ToNSString(
                         error_message.empty() ? "Failed copying package sidecar." : error_message)
                   }];
      }
      return {};
    }
  }

  return destination;
}

- (void)refreshImportedGames {
  discovered_games_.clear();

  // Load cached title names populated by previous game launches.
  NSString* caches_dir =
      NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES).firstObject;
  NSString* names_path = [caches_dir stringByAppendingPathComponent:@"title-names.plist"];
  NSDictionary* title_name_cache = [[NSDictionary dictionaryWithContentsOfFile:names_path] retain];

  std::vector<std::filesystem::path> scan_roots;
  const std::filesystem::path documents_root = xe_get_ios_documents_path();
  const std::filesystem::path library_root = [self importedGamesDirectory];
  scan_roots.push_back(library_root);
  if (documents_root != library_root) {
    scan_roots.push_back(documents_root);
  }

  // Format priority for dedup: GOD (full metadata + content) > ISO (full
  // disc filesystem) > standalone XEX (single executable, missing disc files).
  auto format_priority = [](const std::filesystem::path& p) -> int {
    if (IsLikelyGodContainerFile(p)) return 0;
    if (IsISOPath(p)) return 1;
    return 2;
  };

  std::set<std::filesystem::path> seen_paths;
  std::map<uint32_t, size_t> title_id_to_index;
  for (const auto& root : scan_roots) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
      continue;
    }

    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    std::filesystem::recursive_directory_iterator end;
    while (!ec && it != end) {
      const auto& entry = *it;
      const auto filename = entry.path().filename().string();
      const auto filename_lower = ToLowerAsciiCopy(filename);
      if (entry.is_directory(ec)) {
        if (filename_lower == "cache" || filename_lower == "cache_host") {
          it.disable_recursion_pending();
        }
      } else if (entry.is_regular_file(ec) &&
                 (IsISOPath(entry.path()) || IsDefaultXexPath(entry.path()) ||
                  IsLikelyGodContainerFile(entry.path()))) {
        const std::filesystem::path canonical_path =
            std::filesystem::weakly_canonical(entry.path(), ec);
        const std::filesystem::path unique_path =
            ec ? std::filesystem::absolute(entry.path(), ec) : canonical_path;
        ec.clear();

        if (seen_paths.insert(unique_path).second) {
          IOSDiscoveredGame game;
          if (!BuildDiscoveredGameFromPath(unique_path, &game)) {
            ++it;
            continue;
          }
          if (game.title_id && title_name_cache) {
            NSString* key = XEFormatTitleIDHexLower(game.title_id);
            NSString* cached = [title_name_cache objectForKey:key];
            if (cached.length > 0) {
              game.title = NormalizeGameTitleForUI(std::string([cached UTF8String]));
            }
          }
          if (game.title_id) {
            auto existing = title_id_to_index.find(game.title_id);
            if (existing != title_id_to_index.end()) {
              int old_pri = format_priority(discovered_games_[existing->second].path);
              int new_pri = format_priority(unique_path);
              if (new_pri < old_pri) {
                discovered_games_[existing->second] = std::move(game);
              }
              ++it;
              continue;
            }
            title_id_to_index[game.title_id] = discovered_games_.size();
          }
          discovered_games_.push_back(std::move(game));
        }
      }

      ++it;
    }
  }

  [title_name_cache release];

  for (auto& game : discovered_games_) {
    if (!game.title_id) {
      continue;
    }
    std::error_code ec;
    if (std::filesystem::exists(xe_title_content_root(game.title_id), ec)) {
      game.has_installed_content = true;
    }
  }

  [self applyCompatDataToDiscoveredGames];

  SortDiscoveredGames(&discovered_games_);

  [self.importedGamesCollectionView reloadData];
  self.importedGamesEmptyLabel.hidden = !discovered_games_.empty();

  if (discovered_games_.empty()) {
    focused_game_index_ = -1;
  } else if (focused_game_index_ < 0 ||
             focused_game_index_ >= static_cast<NSInteger>(discovered_games_.size())) {
    focused_game_index_ = 0;
  }
  [self rebuildLauncherFocusGraph];
  [self applyLauncherFocusVisuals];
}

- (void)applyCompatDataToDiscoveredGames {
  for (auto& game : discovered_games_) {
    game.has_compat_info = false;
    game.compat_status.clear();
    game.compat_perf.clear();
    game.compat_notes.clear();
    if (!compat_data_ || !game.title_id) {
      continue;
    }
    NSString* key = XEFormatTitleIDHexUpper(game.title_id);
    NSDictionary* info = [compat_data_ objectForKey:key];
    if (!info) {
      continue;
    }
    NSString* title = xe_string_from_object(info[@"title"]);
    if (title.length > 0) {
      game.title = NormalizeGameTitleForUI(std::string([title UTF8String]));
    }
    NSDictionary* summary = xe_preferred_summary_from_compat_info(info);
    NSDictionary* source = summary ?: info;
    NSString* status = xe_string_from_object(source[@"status"]);
    NSString* perf = xe_string_from_object(source[@"perf"]);
    NSString* notes = xe_string_from_object(source[@"notes"]);
    if ([status isKindOfClass:[NSString class]] && status.length > 0) {
      game.has_compat_info = true;
      game.compat_status = std::string([status UTF8String]);
      game.compat_perf =
          [perf isKindOfClass:[NSString class]] ? std::string([perf UTF8String]) : "";
      game.compat_notes =
          [notes isKindOfClass:[NSString class]] ? std::string([notes UTF8String]) : "";
    }
  }
}

- (void)presentJITRequiredAlert {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:@"JIT Not Detected"
                                          message:xe_jit_not_detected_guidance_message()
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"Open Settings"
                                            style:UIAlertActionStyleDefault
                                          handler:^(__unused UIAlertAction* action) {
                                            [self openSettingsTapped:nil];
                                          }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (NSString*)displayNameForGamePath:(const std::filesystem::path&)game_path {
  for (const IOSDiscoveredGame& game : discovered_games_) {
    if (game.path == game_path) {
      return ToNSString(game.title);
    }
  }
  return nil;
}

- (BOOL)findDiscoveredGameWithTitleID:(uint32_t)title_id
                               system:(const xe::ui::IOSGameSystem*)system_filter
                                 path:(std::filesystem::path*)path_out
                          displayName:(NSString**)display_name_out {
  if (!title_id || !path_out) {
    return NO;
  }

  auto find_match = [&]() -> const IOSDiscoveredGame* {
    for (const IOSDiscoveredGame& game : discovered_games_) {
      if (game.title_id == title_id && (!system_filter || game.system == *system_filter)) {
        return &game;
      }
    }
    return nullptr;
  };

  const IOSDiscoveredGame* match = find_match();
  if (!match) {
    [self refreshImportedGames];
    match = find_match();
  }
  if (!match) {
    return NO;
  }

  *path_out = match->path;
  if (display_name_out) {
    *display_name_out = ToNSString(match->title);
  }
  return YES;
}

- (BOOL)requestAutomaticStikDebugJITHandoffForPendingLaunchPath:
    (const std::filesystem::path*)launch_path {
  if (!GetUserDefaultBool(kXeniaAutoOpenStikDebugOnLaunchPreferenceKey, false)) {
    XELOGI("iOS: Automatic StikDebug handoff skipped (disabled)");
    return NO;
  }
  if (self.gameRunning || self.gameStopInProgress) {
    XELOGI("iOS: Automatic StikDebug handoff skipped (game already running)");
    return NO;
  }
  if (self.jitAcquired || xe_check_jit_available()) {
    if (!self.jitAcquired) {
      [self onJITAcquired];
    }
    XELOGI("iOS: Automatic StikDebug handoff skipped (JIT already available)");
    return NO;
  }

  const double now = GetUnixTimeSeconds();
  const double last_attempt =
      GetUserDefaultDouble(kXeniaLastAutoStikDebugAttemptTimestampPreferenceKey, 0.0);
  if (last_attempt > 0.0 && (now - last_attempt) < kXeniaAutoStikDebugCooldownSeconds) {
    XELOGI("iOS: Skipping automatic StikDebug handoff (cooldown active)");
    return NO;
  }

  NSString* bundle_identifier = NSBundle.mainBundle.bundleIdentifier;
  NSURL* stikdebug_url = xe_stikdebug_enable_jit_url_for_bundle_identifier(bundle_identifier);
  if (!stikdebug_url) {
    XELOGW("iOS: Unable to build StikDebug JIT handoff URL");
    return NO;
  }

  UIApplication* application = [UIApplication sharedApplication];
  if (![application canOpenURL:stikdebug_url]) {
    XELOGW("iOS: StikDebug URL scheme unavailable");
    self.statusLabel.text = @"StikDebug is not installed or unavailable.";
    if (launch_path && !launch_path->empty()) {
      ClearPendingExternalLaunchPathPreference();
    }
    return NO;
  }

  if (launch_path && !launch_path->empty()) {
    StorePendingExternalLaunchPathPreference(*launch_path);
  }

  const BOOL has_pending_launch = launch_path && !launch_path->empty();
  SetUserDefaultDouble(kXeniaLastAutoStikDebugAttemptTimestampPreferenceKey, now);
  self.statusLabel.text =
      has_pending_launch ? @"Opening StikDebug to enable JIT..." : @"Opening StikDebug for JIT...";
  XELOGI("iOS: Opening StikDebug handoff URL {}", stikdebug_url.absoluteString.UTF8String);

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.15 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [application openURL:stikdebug_url
                       options:@{}
                       completionHandler:^(BOOL success) {
                         if (!success) {
                           XELOGW("iOS: Failed to open StikDebug handoff URL");
                           self.statusLabel.text = @"Failed to open StikDebug.";
                           if (has_pending_launch) {
                             ClearPendingExternalLaunchPathPreference();
                           }
                         }
                       }];
                 });
  return YES;
}

- (void)evaluateAutomaticStikDebugJITHandoffIfNeeded {
  [self requestAutomaticStikDebugJITHandoffForPendingLaunchPath:nullptr];
}

- (void)copyLaunchURLForGameAtIndex:(size_t)game_index {
  if (game_index >= discovered_games_.size()) {
    return;
  }
  const IOSDiscoveredGame& game = discovered_games_[game_index];
  if (!game.title_id) {
    self.statusLabel.text = @"Launch URL unavailable for this game.";
    return;
  }

  NSString* launch_url = xe_launch_url_for_title_id(game.title_id, game.system);
  if (!launch_url || launch_url.length == 0) {
    self.statusLabel.text = @"Failed to build launch URL.";
    return;
  }

  [UIPasteboard generalPasteboard].string = launch_url;
  NSString* game_title = ToNSString(game.title);
  self.statusLabel.text = game_title.length > 0
                              ? [NSString stringWithFormat:@"Copied launch URL for %@.", game_title]
                              : @"Copied launch URL.";
  XELOGI("iOS: Copied title-ID launch URL {}", [launch_url UTF8String]);
}

- (BOOL)respondToExternalGameInfoRequestURL:(NSURL*)url {
  NSString* callback_scheme = nil;
  if (!IsExternalGameInfoRequestURL(url, &callback_scheme)) {
    return NO;
  }

  if (!callback_scheme || callback_scheme.length == 0) {
    XELOGW("iOS: gameInfo request is missing callback scheme");
    self.statusLabel.text = @"gameInfo request is missing a callback scheme.";
    return YES;
  }

  NSString* request_scheme = NormalizeURLToken(url.scheme);
  if (request_scheme && [callback_scheme caseInsensitiveCompare:request_scheme] == NSOrderedSame) {
    XELOGW("iOS: gameInfo callback scheme {} would loop back into XeniOS",
           [callback_scheme UTF8String]);
    self.statusLabel.text = @"gameInfo callback scheme cannot point back to XeniOS.";
    return YES;
  }

  [self refreshImportedGames];

  NSMutableArray* exported_games = [NSMutableArray array];
  size_t skipped_without_title_id = 0;
  for (const IOSDiscoveredGame& game : discovered_games_) {
    if (!game.title_id) {
      ++skipped_without_title_id;
      continue;
    }

    NSString* title_id = ToNSString(FormatTitleID(game.title_id));
    NSString* title_name =
        game.title.empty() ? ToNSString(game.path.stem().string()) : ToNSString(game.title);
    NSString* icon_base64 = @"";
    if (!game.icon_data.empty()) {
      NSData* icon_data = [NSData dataWithBytes:game.icon_data.data() length:game.icon_data.size()];
      if (icon_data.length > 0) {
        icon_base64 = [icon_data base64EncodedStringWithOptions:0] ?: @"";
      }
    }

    NSDictionary* entry = @{
      @"titleName" : title_name ?: @"",
      @"version" : @"",
      @"iconData" : icon_base64 ?: @"",
      @"titleId" : title_id ?: @"",
      @"id" : title_id ?: @"",
      @"developer" : @"",
    };
    [exported_games addObject:entry];
  }

  NSError* json_error = nil;
  NSData* json_data = [NSJSONSerialization dataWithJSONObject:exported_games
                                                      options:0
                                                        error:&json_error];
  if (!json_data || json_error) {
    XELOGE("iOS: Failed serializing gameInfo payload: {}",
           json_error.localizedDescription.UTF8String);
    self.statusLabel.text = @"Failed to build gameInfo payload.";
    return YES;
  }

  NSString* games_payload = [json_data base64EncodedStringWithOptions:0];
  if (!games_payload || games_payload.length == 0) {
    XELOGW("iOS: gameInfo payload encoding produced empty data");
    self.statusLabel.text = @"Failed to encode gameInfo payload.";
    return YES;
  }

  NSURLComponents* callback_components = [[[NSURLComponents alloc] init] autorelease];
  callback_components.scheme = callback_scheme;
  callback_components.host = xe_game_info_callback_provider(url);
  callback_components.queryItems = @[ [NSURLQueryItem queryItemWithName:@"games"
                                                                  value:games_payload] ];
  NSURL* callback_url = callback_components.URL;
  if (!callback_url) {
    XELOGE("iOS: Failed building gameInfo callback URL for scheme {}",
           [callback_scheme UTF8String]);
    self.statusLabel.text = @"Failed to build gameInfo callback URL.";
    return YES;
  }

  const NSUInteger exported_count = exported_games.count;
  NSString* callback_app = callback_scheme;
  self.statusLabel.text = [NSString
      stringWithFormat:@"Sending %lu games to %@...", (unsigned long)exported_count, callback_app];
  XELOGI(
      "iOS: Returning {} games via {} (skipped {} without title IDs, payload {} bytes, URL chars "
      "{})",
      static_cast<uint32_t>(exported_count), callback_url.absoluteString.UTF8String,
      static_cast<uint32_t>(skipped_without_title_id), static_cast<uint32_t>(json_data.length),
      static_cast<uint32_t>(callback_url.absoluteString.length));

  [[UIApplication sharedApplication] openURL:callback_url
                                     options:@{}
                           completionHandler:^(BOOL success) {
                             if (success) {
                               self.statusLabel.text = [NSString
                                   stringWithFormat:@"Sent %lu games to %@.",
                                                    (unsigned long)exported_count, callback_app];
                               return;
                             }
                             XELOGW("iOS: Failed opening gameInfo callback URL {}",
                                    callback_url.absoluteString.UTF8String);
                             self.statusLabel.text = [NSString
                                 stringWithFormat:@"Failed to return library to %@.", callback_app];
                           }];
  return YES;
}

- (BOOL)handleExternalLaunchURL:(NSURL*)url {
  if ([self respondToExternalGameInfoRequestURL:url]) {
    return YES;
  }

  std::filesystem::path launch_path;
  NSString* display_name = nil;
  uint32_t title_id = 0;
  xe::ui::IOSGameSystem title_system = xe::ui::IOSGameSystem::kXbox360;
  bool title_system_present = false;
  if (ExtractLaunchPathFromExternalURL(url, &launch_path) && !launch_path.empty()) {
    display_name = [self displayNameForGamePath:launch_path];
    if (!display_name || display_name.length == 0) {
      display_name = ToNSString(launch_path.filename().string());
    }
  } else if (ExtractLaunchTitleIDFromExternalURL(url, &title_id, &title_system,
                                                 &title_system_present) &&
             title_id) {
    if (![self findDiscoveredGameWithTitleID:title_id
                                      system:title_system_present ? &title_system : nullptr
                                        path:&launch_path
                                 displayName:&display_name]) {
      XELOGW("iOS: External launch title ID {:08X} was not found in Library", title_id);
      self.statusLabel.text =
          [NSString stringWithFormat:@"Title ID %08X was not found in Library.", title_id];
      return NO;
    }
  }

  if (launch_path.empty()) {
    NSString* absolute_url = [url absoluteString];
    XELOGW("iOS: External launch URL missing valid game target: {}",
           absolute_url ? [absolute_url UTF8String] : "");
    self.statusLabel.text = @"Launch URL missing a valid game target.";
    return NO;
  }

  if (!display_name || display_name.length == 0) {
    display_name = ToNSString(launch_path.filename().string());
  }

  if (title_id) {
    XELOGI("iOS: External game launch requested by title ID {:08X}: {}", title_id,
           launch_path.string());
  } else {
    XELOGI("iOS: External game launch requested: {}", launch_path.string());
  }
  if (!self.jitAcquired) {
    pending_external_launch_path_ = launch_path;
    if (![self requestAutomaticStikDebugJITHandoffForPendingLaunchPath:&launch_path]) {
      self.statusLabel.text =
          [NSString stringWithFormat:@"Waiting for JIT to launch: %@", display_name];
    }
    return YES;
  }

  [self launchGameAtPath:launch_path displayName:display_name];
  return YES;
}

- (void)launchGameAtPath:(const std::filesystem::path&)game_path
             displayName:(NSString*)display_name {
  NSString* path_ns = ToNSString(game_path.string());
  NSString* fallback_name = ToNSString(game_path.filename().string());
  NSString* game_label = display_name.length ? display_name : fallback_name;

  if (IsLikelyGodContainerFile(game_path)) {
    auto header = xe::vfs::XContentContainerDevice::ReadContainerHeader(game_path);
    if (header && header->content_metadata.data_file_count > 0 &&
        !HasContentSidecarDataDirectory(game_path)) {
      self.statusLabel.text = @"Selected game is missing its .data folder.";
      XEPresentOKAlert(self, @"Missing Game Data",
                       @"This package needs its matching .data folder before it can be launched.");
      return;
    }
  }

  if (!self.jitAcquired) {
    [self presentJITRequiredAlert];
    return;
  }

  if (self.gameStopInProgress || self.gameRunning) {
    if (self.appContext) {
      self.statusLabel.text =
          [NSString stringWithFormat:@"Stopping current game; queued %@.", game_label];
      self.appContext->LaunchGame(std::string([path_ns UTF8String]));
    } else {
      self.statusLabel.text = @"Unable to queue launch (app context unavailable).";
    }
    return;
  }

  self.statusLabel.text = [NSString stringWithFormat:@"Loading: %@", game_label];
  self.gameRunning = YES;

  xe_request_landscape_orientation(self);
  [UIView animateWithDuration:0.3
      animations:^{
        self.launcherOverlay.alpha = 0.0;
      }
      completion:^(__unused BOOL finished) {
        self.launcherOverlay.hidden = YES;
      }];

  if (self.appContext) {
    self.appContext->LaunchGame(std::string([path_ns UTF8String]));
  } else {
    self.statusLabel.text = @"Unable to launch game (app context unavailable).";
    self.launcherOverlay.hidden = NO;
    self.launcherOverlay.alpha = 1.0;
  }
}

- (BOOL)installTitleUpdateAtPath:(NSString*)path
                          status:(NSString**)status_out
                  notTitleUpdate:(BOOL*)not_title_update_out {
  if (status_out) {
    *status_out = nil;
  }
  if (not_title_update_out) {
    *not_title_update_out = NO;
  }
  if (!self.appContext) {
    if (status_out) {
      *status_out = @"App context unavailable.";
    }
    return NO;
  }

  std::string status;
  bool not_title_update = false;
  BOOL success = self.appContext->InstallTitleUpdate(std::string([path UTF8String]), &status,
                                                     &not_title_update);
  if (status_out && !status.empty()) {
    *status_out = ToNSString(status);
  }
  if (not_title_update_out) {
    *not_title_update_out = not_title_update;
  }
  return success;
}

- (void)presentCompatibilitySheetForIndex:(size_t)game_index {
  if (game_index >= discovered_games_.size()) {
    return;
  }

  const IOSDiscoveredGame& game = discovered_games_[game_index];
  if (!game.title_id) {
    XEPresentOKAlert(self, @"Unavailable",
                     @"This item does not expose a title ID, so compatibility details "
                     @"cannot be loaded.");
    return;
  }

  NSDictionary* compat_data = [compat_data_ objectForKey:XEFormatTitleIDHexUpper(game.title_id)];
  NSString* game_title =
      game.title.empty() ? ToNSString(game.path.stem().string()) : ToNSString(game.title);
  UIImage* hero_artwork = xe_cached_game_art(game.title_id);
  if (!hero_artwork && !game.icon_data.empty()) {
    NSData* data = [NSData dataWithBytes:game.icon_data.data() length:game.icon_data.size()];
    hero_artwork = [UIImage imageWithData:data];
  }
  if (!hero_artwork && self.importedGamesCollectionView) {
    NSIndexPath* index_path = [NSIndexPath indexPathForItem:(NSInteger)game_index inSection:0];
    XeniaGameTileCell* tile =
        (XeniaGameTileCell*)[self.importedGamesCollectionView cellForItemAtIndexPath:index_path];
    if ([tile isKindOfClass:[XeniaGameTileCell class]]) {
      hero_artwork = tile.iconView.image;
    }
  }
  XeniaGameCompatibilityViewController* compatibility_controller =
      [[XeniaGameCompatibilityViewController alloc] initWithTitleID:game.title_id
                                                              title:game_title
                                                         compatData:compat_data];
  if (hero_artwork) {
    [compatibility_controller setHeroArtwork:hero_artwork];
  }
  XeniaLandscapeNavigationController* navigation_controller =
      [[XeniaLandscapeNavigationController alloc]
          initWithRootViewController:compatibility_controller];
  navigation_controller.view.backgroundColor = [XeniaTheme bgPrimary];
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
    navigation_controller.modalPresentationStyle = UIModalPresentationFormSheet;
    if (@available(iOS 15.0, *)) {
      UISheetPresentationController* sheet = navigation_controller.sheetPresentationController;
      sheet.detents = @[
        [UISheetPresentationControllerDetent mediumDetent],
        [UISheetPresentationControllerDetent largeDetent]
      ];
      sheet.prefersGrabberVisible = YES;
    }
  } else {
    navigation_controller.modalPresentationStyle = UIModalPresentationFullScreen;
  }
  [self presentViewController:navigation_controller animated:YES completion:nil];
  [navigation_controller release];
  [compatibility_controller release];
}

- (void)presentManageContentSheetForIndex:(size_t)game_index {
  if (game_index >= discovered_games_.size()) {
    return;
  }

  const IOSDiscoveredGame& game = discovered_games_[game_index];
  if (!game.title_id) {
    XEPresentOKAlert(
        self, @"Unavailable",
        @"This item does not expose a title ID, so installed content cannot be managed.");
    return;
  }

  XeniaGameContentViewController* content_controller = [[XeniaGameContentViewController alloc]
      initWithTitleID:game.title_id
                title:(game.title.empty() ? ToNSString(game.path.stem().string())
                                          : ToNSString(game.title))host:self];
  XeniaLandscapeNavigationController* navigation_controller =
      [[XeniaLandscapeNavigationController alloc] initWithRootViewController:content_controller];
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
    navigation_controller.modalPresentationStyle = UIModalPresentationFormSheet;
    if (@available(iOS 15.0, *)) {
      UISheetPresentationController* sheet = navigation_controller.sheetPresentationController;
      sheet.detents = @[
        [UISheetPresentationControllerDetent mediumDetent],
        [UISheetPresentationControllerDetent largeDetent]
      ];
      sheet.prefersGrabberVisible = YES;
    }
  } else {
    navigation_controller.modalPresentationStyle = UIModalPresentationFullScreen;
  }
  [self presentViewController:navigation_controller animated:YES completion:nil];
  [navigation_controller release];
  [content_controller release];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView* __unused)collectionView
     numberOfItemsInSection:(NSInteger)__unused section {
  return static_cast<NSInteger>(discovered_games_.size());
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  XeniaGameTileCell* cell =
      [collectionView dequeueReusableCellWithReuseIdentifier:@"ImportedGameCell"
                                                forIndexPath:indexPath];
  if (indexPath.item < 0 || static_cast<size_t>(indexPath.item) >= discovered_games_.size()) {
    cell.titleLabel.text = @"";
    cell.iconView.image = nil;
    cell.controllerFocused = NO;
    return cell;
  }

  const IOSDiscoveredGame& game = discovered_games_[static_cast<size_t>(indexPath.item)];
  NSString* title =
      game.title.empty() ? ToNSString(game.path.stem().string()) : ToNSString(game.title);
  cell.titleLabel.text = title;
  if (game.has_compat_info) {
    NSString* status = ToNSString(game.compat_status);
    UIColor* pill_color = xe_compat_status_color(status);
    cell.compatPill.text = xe_compat_status_label(status);
    cell.compatPill.textColor = pill_color;
    cell.compatPill.backgroundColor = [pill_color colorWithAlphaComponent:0.1];
    cell.compatPill.hidden = NO;
  } else {
    cell.compatPill.text = @"";
    cell.compatPill.hidden = YES;
  }
  cell.controllerFocused = controller_navigation_was_enabled_ && launcher_library_focus_active_ &&
                           focused_game_index_ == indexPath.item;

  // Priority: cached remote art → async fetch → embedded icon → placeholder.
  // Remote tile.png is much higher resolution than embedded 64x64 icons.
  UIImage* icon =
      xe_game_system_supports_remote_art(game.system) ? xe_cached_game_art(game.title_id) : nil;
  if (icon) {
    cell.iconView.image = icon;
  } else {
    // Show embedded icon (or placeholder) while fetching high-res art.
    UIImage* fallback = nil;
    if (!game.icon_data.empty()) {
      NSData* data = [NSData dataWithBytes:game.icon_data.data() length:game.icon_data.size()];
      fallback = [UIImage imageWithData:data];
    }
    if (!fallback) fallback = [UIImage imageNamed:@"128"];
    if (!fallback) fallback = [UIImage systemImageNamed:@"gamecontroller.fill"];
    cell.iconView.image = fallback;
    if (game.title_id && xe_game_system_supports_remote_art(game.system)) {
      uint32_t fetch_title_id = game.title_id;
      xe::ui::IOSGameSystem fetch_system = game.system;
      // No __weak under MRC — collectionView is owned by self and won't
      // be deallocated while the launcher overlay is visible.
      UICollectionView* cv = collectionView;
      xe_fetch_game_art(fetch_title_id, ^(UIImage* fetched) {
        if (!fetched || !cv) return;
        NSMutableArray* reload_paths = [NSMutableArray array];
        for (size_t i = 0; i < self->discovered_games_.size(); ++i) {
          if (self->discovered_games_[i].title_id == fetch_title_id &&
              self->discovered_games_[i].system == fetch_system) {
            [reload_paths addObject:[NSIndexPath indexPathForItem:static_cast<NSInteger>(i)
                                                        inSection:0]];
          }
        }
        if (reload_paths.count > 0) {
          [cv reloadItemsAtIndexPaths:reload_paths];
        }
      });
    }
  }
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [collectionView deselectItemAtIndexPath:indexPath animated:YES];
  if (indexPath.item < 0 || static_cast<size_t>(indexPath.item) >= discovered_games_.size()) {
    return;
  }
  [self setFocusedGameIndex:indexPath.item scroll:NO];
  const IOSDiscoveredGame& game = discovered_games_[static_cast<size_t>(indexPath.item)];
  [self launchGameAtPath:game.path displayName:ToNSString(game.title)];
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  (void)collectionView;
  (void)point;
  if (indexPath.item < 0 || static_cast<size_t>(indexPath.item) >= discovered_games_.size()) {
    return nil;
  }

  const size_t game_index = static_cast<size_t>(indexPath.item);
  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu*(NSArray<UIMenuElement*>* __unused suggested_actions) {
                     const IOSDiscoveredGame& game = self->discovered_games_[game_index];
                     const std::filesystem::path game_path = game.path;
                     NSString* game_title = ToNSString(game.title);
                     const BOOL can_manage_content =
                         game.title_id != 0 && xe_game_system_supports_manage_content(game.system);
                     const BOOL can_view_compatibility =
                         game.title_id != 0 && xe_game_system_supports_compatibility(game.system);
                     const BOOL can_copy_launch_url = game.title_id != 0;
                     UIAction* play_action = [UIAction
                         actionWithTitle:@"Play"
                                   image:[UIImage systemImageNamed:@"play.fill"]
                              identifier:nil
                                 handler:^(__unused UIAction* action) {
                                   [self launchGameAtPath:game_path displayName:game_title];
                                 }];
                     UIAction* compatibility_action =
                         [UIAction actionWithTitle:@"Compatibility"
                                             image:[UIImage systemImageNamed:@"checkmark.shield"]
                                        identifier:nil
                                           handler:^(__unused UIAction* action) {
                                             [self presentCompatibilitySheetForIndex:game_index];
                                           }];
                     UIAction* content_action =
                         [UIAction actionWithTitle:@"Manage Content"
                                             image:[UIImage systemImageNamed:@"square.stack.3d.up"]
                                        identifier:nil
                                           handler:^(__unused UIAction* action) {
                                             [self presentManageContentSheetForIndex:game_index];
                                           }];
                     UIAction* copy_launch_url_action =
                         [UIAction actionWithTitle:@"Copy Launch URL"
                                             image:[UIImage systemImageNamed:@"link"]
                                        identifier:nil
                                           handler:^(__unused UIAction* action) {
                                             [self copyLaunchURLForGameAtIndex:game_index];
                                           }];
                     if (!can_view_compatibility) {
                       compatibility_action.attributes = UIMenuElementAttributesDisabled;
                     }
                     if (!can_manage_content) {
                       content_action.attributes = UIMenuElementAttributesDisabled;
                     }
                     if (!can_copy_launch_url) {
                       copy_launch_url_action.attributes = UIMenuElementAttributesDisabled;
                     }
                     return [UIMenu menuWithTitle:@""
                                         children:@[
                                           play_action, compatibility_action, content_action,
                                           copy_launch_url_action
                                         ]];
                   }];
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout* __unused)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath* __unused)indexPath {
  NSInteger columns = [self launcherGridColumnCountForContentSize:collectionView.bounds.size];
  CGFloat spacing = [self launcherGridInteritemSpacingForCollectionView:collectionView];
  CGFloat tile_width = [self launcherGridTileWidthForCollectionView:collectionView
                                                            columns:columns
                                                   interitemSpacing:spacing];
  // Cover art is ~219x300 (~1:1.37). Reserve enough room for a readable
  // two-line title strip and a compat pill on its own row.
  CGFloat image_height = ceil(tile_width * 300.0f / 219.0f);
  return CGSizeMake(tile_width,
                    image_height + [self launcherGridTitleHeightForCollectionView:collectionView]);
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                      layout:(UICollectionViewLayout* __unused)collectionViewLayout
    minimumInteritemSpacingForSectionAtIndex:(NSInteger)__unused section {
  return [self launcherGridInteritemSpacingForCollectionView:collectionView];
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                 layout:(UICollectionViewLayout* __unused)collectionViewLayout
    minimumLineSpacingForSectionAtIndex:(NSInteger)__unused section {
  return [self launcherGridLineSpacingForCollectionView:collectionView];
}

- (UIEdgeInsets)collectionView:(UICollectionView*)collectionView
                        layout:(UICollectionViewLayout* __unused)collectionViewLayout
        insetForSectionAtIndex:(NSInteger)section {
  (void)section;
  NSInteger columns = [self launcherGridColumnCountForContentSize:collectionView.bounds.size];
  CGFloat interitem_spacing = [self launcherGridInteritemSpacingForCollectionView:collectionView];
  CGFloat tile_width = [self launcherGridTileWidthForCollectionView:collectionView
                                                            columns:columns
                                                   interitemSpacing:interitem_spacing];
  CGFloat consumed_width = tile_width * columns + interitem_spacing * MAX(columns - 1, 0);
  CGFloat remainder = MAX(collectionView.bounds.size.width - consumed_width, 0.0f);
  CGFloat screen_scale =
      collectionView.window.screen ? collectionView.window.screen.scale : UIScreen.mainScreen.scale;
  CGFloat left_inset = floor((remainder * 0.5f) * screen_scale) / screen_scale;
  CGFloat right_inset = MAX(remainder - left_inset, 0.0f);
  return UIEdgeInsetsMake(0.0f, left_inset, 0.0f, right_inset);
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  CGSize collection_size = self.importedGamesCollectionView.bounds.size;
  if (!CGSizeEqualToSize(collection_size, last_collection_layout_size_)) {
    last_collection_layout_size_ = collection_size;
    [self.importedGamesCollectionView.collectionViewLayout invalidateLayout];
  }
  // Notify the app context that the layout changed, so the window and
  // presenter can update for rotation, split-view, or safe-area changes.
  if (self.appContext) {
    self.appContext->NotifyLayoutChanged();
  }
}

- (void)openGameTapped:(UIButton*)sender {
  if (self.gameStopInProgress) {
    self.statusLabel.text = @"Stopping game... Please wait.";
    return;
  }
  NSArray<UTType*>* contentTypes = @[
    [UTType typeWithFilenameExtension:@"iso"],
    [UTType typeWithFilenameExtension:@"xex"],
    UTTypeData,
  ];

  UIDocumentPickerViewController* picker =
      [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:contentTypes];
  picker.delegate = self;
  picker.allowsMultipleSelection = NO;
  picker.shouldShowFileExtensions = YES;
  [self presentViewController:picker animated:YES completion:nil];
}

- (void)openSettingsTapped:(UIButton*)sender {
  (void)sender;
  XeniaConfigViewController* settings_vc =
      [[XeniaConfigViewController alloc] initWithStyle:UITableViewStyleInsetGrouped];
  XeniaLandscapeNavigationController* nav =
      [[XeniaLandscapeNavigationController alloc] initWithRootViewController:settings_vc];
  BOOL landscape_presentation =
      CGRectGetWidth(self.view.bounds) > CGRectGetHeight(self.view.bounds);
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
    nav.modalPresentationStyle = UIModalPresentationFormSheet;
    if (@available(iOS 15.0, *)) {
      UISheetPresentationController* sheet = nav.sheetPresentationController;
      sheet.detents =
          landscape_presentation ? @[ [UISheetPresentationControllerDetent largeDetent] ] : @[
            [UISheetPresentationControllerDetent mediumDetent],
            [UISheetPresentationControllerDetent largeDetent]
          ];
      sheet.prefersGrabberVisible = YES;
      sheet.prefersScrollingExpandsWhenScrolledToEdge = YES;
    }
  } else {
    nav.modalPresentationStyle = UIModalPresentationFullScreen;
  }
  [self presentViewController:nav animated:YES completion:nil];
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController* __unused)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (urls.count == 0) return;

  NSURL* url = urls[0];
  BOOL access_granted = [url startAccessingSecurityScopedResource];
  XELOGI("iOS: User selected game file: {} (security-scoped: {})", [url.path UTF8String],
         access_granted ? "yes" : "no");

  void (^import_selected_game)(void) = ^{
    NSError* import_error = nil;
    std::filesystem::path imported_path = [self importGameIntoLibrary:url error:&import_error];
    if (access_granted) {
      [url stopAccessingSecurityScopedResource];
    }

    if (imported_path.empty()) {
      NSString* message = import_error.localizedDescription ?: @"Failed to import selected game.";
      XEPresentOKAlert(self, @"Import Failed", message);
      return;
    }

    [self refreshImportedGames];
    NSString* imported_name = ToNSString(imported_path.filename().string());
    if (self.jitAcquired) {
      [self launchGameAtPath:imported_path displayName:imported_name];
    } else {
      self.statusLabel.text =
          [NSString stringWithFormat:@"Imported %@. Waiting for JIT.", imported_name];
    }
  };

  const std::filesystem::path selected_path([url.path UTF8String]);
  const BOOL likely_direct_game = IsISOPath(selected_path) || IsDefaultXexPath(selected_path);
  IOSSelectedContentPackage package_info;
  const BOOL has_content_package_info =
      xe_read_selected_content_package(selected_path, &package_info, nullptr);
  const BOOL is_launchable_package =
      has_content_package_info && (package_info.content_type == xe::XContentType::kXbox360Title ||
                                   package_info.content_type == xe::XContentType::kInstalledGame);
  const BOOL should_try_title_update_install = cvars::ios_async_import_ui && self.appContext &&
                                               !likely_direct_game && !is_launchable_package;
  if (should_try_title_update_install) {
    self.statusLabel.text = @"Checking content package...";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      std::string status;
      bool not_title_update = false;
      bool success = self.appContext->InstallTitleUpdate(std::string([url.path UTF8String]),
                                                         &status, &not_title_update);

      dispatch_async(dispatch_get_main_queue(), ^{
        if (success) {
          if (access_granted) {
            [url stopAccessingSecurityScopedResource];
          }
          NSString* message = status.empty() ? @"Installed title update." : ToNSString(status);
          self.statusLabel.text = message;
          [self refreshImportedGames];
          XEPresentOKAlert(self, @"Title Update Installed", message);
          return;
        }

        if (!not_title_update) {
          if (access_granted) {
            [url stopAccessingSecurityScopedResource];
          }
          NSString* message =
              status.empty() ? @"Title update installation failed." : ToNSString(status);
          self.statusLabel.text = message;
          XEPresentOKAlert(self, @"Installation Failed", message);
          return;
        }

        import_selected_game();
      });
    });
    return;
  }

  import_selected_game();
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController* __unused)controller {
  XELOGI("iOS: Document picker cancelled");
}

#pragma mark - Status bar / home indicator

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  if (self.launcherOverlay.hidden) {
    return UIInterfaceOrientationMaskLandscape;
  }
  return UIInterfaceOrientationMaskAllButUpsideDown;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return xe_current_interface_orientation(self.view);
}

- (BOOL)shouldAutorotate {
  return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
  return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
  return UIRectEdgeAll;
}

#pragma mark - Public API

- (void)showLauncherOverlay {
  self.gameRunning = NO;
  self.gameStopInProgress = NO;
  [self hideInGameMenuOverlay];
  self.launcherOverlay.hidden = NO;
  self.statusLabel.text = @"";
  [self refreshImportedGames];
  [self refreshSignedInProfileUI];
  [self updateJITStatusIndicator];
  [self updateJITAvailabilityUI];
  [self rebuildLauncherFocusGraph];
  [self applyLauncherFocusVisuals];
  xe_request_current_orientation(self);
  [UIView animateWithDuration:0.3
                   animations:^{
                     self.launcherOverlay.alpha = 1.0;
                   }];
}

- (void)dealloc {
  [self.jitPollTimer invalidate];
  [self.controllerNavTimer invalidate];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [compat_data_ release];
}

@end
