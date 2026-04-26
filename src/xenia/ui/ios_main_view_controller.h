/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_IOS_MAIN_VIEW_CONTROLLER_H_
#define XENIA_UI_IOS_MAIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <cstdint>

#include "xenia/hid/input.h"
#include "xenia/ui/ios_game_content_view_controller.h"
#include "xenia/ui/ios_metal_view.h"
#include "xenia/ui/windowed_app_context_ios.h"

// Root iOS UI: hosts the XeniaMetalView (the running emulator surface) and
// the launcher overlay (game library + import + settings + profile + JIT
// status). Owns the in-game menu overlay, controller-driven focus
// navigation, JIT polling, automatic StikDebug handoff and the external
// xenios:// launch flow.
@interface XeniaViewController : UIViewController <UIDocumentPickerDelegate,
                                                   UICollectionViewDataSource,
                                                   UICollectionViewDelegateFlowLayout,
                                                   XeniaGameContentHost>
@property(nonatomic, strong) XeniaMetalView* metalView;
@property(nonatomic, strong) UIView* launcherOverlay;
@property(nonatomic, strong) UIButton* openGameButton;
@property(nonatomic, strong) UIButton* settingsButton;
@property(nonatomic, strong) UIButton* profileButton;
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UILabel* statusLabel;
@property(nonatomic, strong) UILabel* signedInProfileLabel;
@property(nonatomic, strong) UICollectionView* importedGamesCollectionView;
@property(nonatomic, strong) UILabel* importedGamesEmptyLabel;
@property(nonatomic, strong) UIView* inGameMenuOverlay;
@property(nonatomic, strong) UIButton* inGameResumeButton;
@property(nonatomic, strong) UIButton* inGameSettingsButton;
@property(nonatomic, strong) UIButton* inGameLiveLogButton;
@property(nonatomic, strong) UIButton* inGameExitButton;
@property(nonatomic, assign) BOOL gameRunning;
@property(nonatomic, assign) BOOL gameStopInProgress;
@property(nonatomic, assign) xe::ui::IOSWindowedAppContext* appContext;

// JIT status widgets.
@property(nonatomic, strong) UIView* jitWarningCard;
@property(nonatomic, strong) UIView* jitStatusDot;
@property(nonatomic, strong) UIView* jitStatusRing;
@property(nonatomic, strong) UILabel* jitStatusLabel;
@property(nonatomic, strong) UIView* jitReadyDot;
@property(nonatomic, strong) UIView* jitReadyRing;
@property(nonatomic, strong) UILabel* jitReadyLabel;
@property(nonatomic, strong) NSTimer* jitPollTimer;
@property(nonatomic, strong) NSTimer* controllerNavTimer;
@property(nonatomic, strong) UIStackView* topInfoStack;
@property(nonatomic, assign) BOOL jitAcquired;

- (void)refreshSignedInProfileUI;
- (void)showLauncherOverlay;
- (void)presentSystemSigninPromptForUserIndex:(uint32_t)user_index
                                  usersNeeded:(uint32_t)users_needed
                                   completion:(void (^)(BOOL success))completion;
- (void)presentSystemKeyboardPromptWithTitle:(NSString*)title
                                 description:(NSString*)description
                                 defaultText:(NSString*)default_text
                                  completion:(void (^)(BOOL cancelled, NSString* text))completion;
- (void)setupInGameMenuOverlay;
- (void)toggleInGameMenuTapped:(UITapGestureRecognizer*)recognizer;
- (void)resumeGameTapped:(UIButton*)sender;
- (void)inGameSettingsTapped:(UIButton*)sender;
- (void)inGameLiveLogTapped:(UIButton*)sender;
- (void)exitGameTapped:(UIButton*)sender;
- (void)hideInGameMenuOverlay;
- (void)pollControllerNavigation:(NSTimer*)timer;
- (BOOL)readNativeControllerState:(xe::hid::X_INPUT_STATE*)out_state;
- (BOOL)handleExternalLaunchURL:(NSURL*)url;
- (BOOL)respondToExternalGameInfoRequestURL:(NSURL*)url;
- (void)evaluateAutomaticStikDebugJITHandoffIfNeeded;
- (void)startCompatFetchIfNeeded;
- (void)applyCompatDataToDiscoveredGames;
- (void)presentCompatibilitySheetForIndex:(size_t)game_index;
- (void)presentManageContentSheetForIndex:(size_t)game_index;
@end

#endif  // XENIA_UI_IOS_MAIN_VIEW_CONTROLLER_H_
