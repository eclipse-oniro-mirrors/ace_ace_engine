/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "adapter/preview/entrance/ace_ability.h"

#include <thread>

#include "adapter/preview/entrance/ace_application_info.h"
#include "adapter/preview/entrance/ace_container.h"
#include "frameworks/bridge/common/inspector/inspector_client.h"
#include "frameworks/bridge/common/utils/utils.h"
#include "frameworks/bridge/js_frontend/js_frontend.h"

namespace OHOS::Ace::Platform {

std::atomic<bool> AceAbility::loopRunning_ = true;

namespace {

// JS frontend maintain the page ID self, so it's useless to pass page ID from platform
// layer, neither OpenHarmony or Windows, we should delete here usage when Java delete it.
constexpr int32_t UNUSED_PAGE_ID = 1;

constexpr char ASSET_PATH_SHARE[] = "share";
#ifdef WINDOWS_PLATFORM
constexpr char DELIMITER[] = "\\";
#else
constexpr char DELIMITER[] = "/";
#endif

#ifdef USE_GLFW_WINDOW
// Screen Density Coefficient/Base Density = Resolution/ppi
constexpr double SCREEN_DENSITY_COEFFICIENT_PHONE = 1080.0 * 160.0 / 480.0;
constexpr double SCREEN_DENSITY_COEFFICIENT_WATCH = 466.0 * 160 / 320.0;
constexpr double SCREEN_DENSITY_COEFFICIENT_TABLE = 2560.0 * 160.0 / 280.0;
constexpr double SCREEN_DENSITY_COEFFICIENT_TV = 3840.0 * 160.0 / 640.0;
#endif
} // namespace

std::unique_ptr<AceAbility> AceAbility::CreateInstance(AceRunArgs& runArgs)
{
    LOGI("Start create AceAbility instance");
    bool initSucceeded = FlutterDesktopInit();
    if (!initSucceeded) {
        LOGE("Could not create window; AceDesktopInit failed.");
        return nullptr;
    }

    AceApplicationInfo::GetInstance().SetLocale(runArgs.language, runArgs.region, runArgs.script, "");

    auto controller = FlutterDesktopCreateWindow(
        runArgs.deviceWidth, runArgs.deviceHeight, runArgs.windowTitle.c_str(), runArgs.onRender);
    auto aceAbility = std::make_unique<AceAbility>(runArgs);
    aceAbility->SetGlfwWindowController(controller);
    return aceAbility;
}

bool AceAbility::DispatchTouchEvent(const TouchPoint& event)
{
    auto container = AceContainer::GetContainerInstance(ACE_INSTANCE_ID);
    if (!container) {
        LOGE("container is null");
        return false;
    }

    auto aceView = container->GetAceView();
    if (!aceView) {
        LOGE("aceView is null");
        return false;
    }

    std::promise<bool> touchPromise;
    std::future<bool> touchFuture = touchPromise.get_future();
    container->GetTaskExecutor()->PostTask([aceView, event, &touchPromise]() {
        bool isHandled = aceView->HandleTouchEvent(event);
        touchPromise.set_value(isHandled);
    }, TaskExecutor::TaskType::PLATFORM);
    return touchFuture.get();
}

bool AceAbility::DispatchBackPressedEvent()
{
    LOGI("DispatchBackPressedEvent start ");
    auto container = AceContainer::GetContainerInstance(ACE_INSTANCE_ID);
    if (!container) {
        return false;
    }

    auto context = container->GetPipelineContext();
    if (!context) {
        return false;
    }

    std::promise<bool> backPromise;
    std::future<bool> backFuture = backPromise.get_future();
    auto weak = AceType::WeakClaim(AceType::RawPtr(context));
    container->GetTaskExecutor()->PostTask([weak, &backPromise]() {
        auto context = weak.Upgrade();
        if (context == nullptr) {
            LOGW("context is nullptr.");
            return;
        }
        bool canBack = false;
        if (context->IsLastPage()) {
            LOGW("Can't back because this is the last page!");
        } else {
            canBack = context->CallRouterBackToPopPage();
        }
        backPromise.set_value(canBack);
    }, TaskExecutor::TaskType::PLATFORM);
    return backFuture.get();
}

AceAbility::AceAbility(const AceRunArgs& runArgs) : runArgs_(runArgs)
{
    SystemProperties::InitDeviceInfo(runArgs_.deviceWidth, runArgs_.deviceHeight,
        runArgs_.deviceConfig.orientation == DeviceOrientation::PORTRAIT ? 0 : 1,
        runArgs_.deviceConfig.density, runArgs_.isRound);
    SystemProperties::InitDeviceType(runArgs_.deviceConfig.deviceType);
    SystemProperties::SetColorMode(runArgs_.deviceConfig.colorMode);
    SetConfigChanges(runArgs.configChanges);
    if (runArgs_.formsEnabled) {
        LOGI("CreateContainer with JS_CARD frontend");
        AceContainer::CreateContainer(ACE_INSTANCE_ID, FrontendType::JS_CARD);
    } else if (runArgs_.aceVersion == AceVersion::ACE_1_0) {
        LOGI("CreateContainer with JS frontend");
        AceContainer::CreateContainer(ACE_INSTANCE_ID, FrontendType::JS);
    } else if (runArgs_.aceVersion == AceVersion::ACE_2_0) {
        LOGI("CreateContainer with JSDECLARATIVE frontend");
        AceContainer::CreateContainer(ACE_INSTANCE_ID, FrontendType::DECLARATIVE_JS);
    } else {
        LOGE("UnKnown frontend type");
    }
}

void AceAbility::SetConfigChanges(const std::string& configChanges)
{
    if (configChanges == "") {
        return;
    }
    std::vector<std::string> configChangesSpliter;
    OHOS::Ace::StringUtils::StringSpliter(configChanges, ',', configChangesSpliter);
    for (const auto& singleConfig : configChangesSpliter) {
        if (singleConfig == "locale") {
            configChanges_.watchLocale = true;
            continue;
        } else if (singleConfig == "layout") {
            configChanges_.watchLayout = true;
            continue;
        } else if (singleConfig == "fontSize") {
            configChanges_.watchFontSize = true;
            continue;
        } else if (singleConfig == "orientation") {
            configChanges_.watchOrientation = true;
            continue;
        } else if (singleConfig == "density") {
            configChanges_.watchDensity = true;
            continue;
        } else {
            LOGW("unsupport config %{public}s", singleConfig.c_str());
        }
    }
}

AceAbility::~AceAbility()
{
    if (controller_) {
        FlutterDesktopDestroyWindow(controller_);
    }
    FlutterDesktopTerminate();
}

std::string GetCustomAssetPath(std::string assetPath)
{
    if (assetPath.empty()) {
        LOGE("AssetPath is null.");
        return std::string();
    }
    std::string customAssetPath;
    if (OHOS::Ace::Framework::EndWith(assetPath, DELIMITER)) {
        assetPath = assetPath.substr(0, assetPath.size() - 1);
    }
    customAssetPath = assetPath.substr(0, assetPath.find_last_of(DELIMITER) + 1);
    return customAssetPath;
}

void AceAbility::InitEnv()
{
    AceContainer::AddAssetPath(
        ACE_INSTANCE_ID, "", { runArgs_.assetPath, GetCustomAssetPath(runArgs_.assetPath).append(ASSET_PATH_SHARE) });

    AceContainer::SetResourcesPathAndThemeStyle(
        ACE_INSTANCE_ID, runArgs_.resourcesPath, runArgs_.themeId, runArgs_.deviceConfig.colorMode);

    auto view = new FlutterAceView(ACE_INSTANCE_ID);
    AceContainer::SetView(view, runArgs_.deviceConfig.density, runArgs_.deviceWidth, runArgs_.deviceHeight);
    AceContainer::AddRouterChangeCallback(ACE_INSTANCE_ID, runArgs_.onRouterChange);
    IdleCallback idleNoticeCallback = [view](int64_t deadline) { view->ProcessIdleEvent(deadline); };
    FlutterDesktopSetIdleCallback(controller_, idleNoticeCallback);

    // Should make it possible to update surface changes by using viewWidth and viewHeight.
    view->NotifySurfaceChanged(runArgs_.deviceWidth, runArgs_.deviceHeight);
    view->NotifyDensityChanged(runArgs_.deviceConfig.density);
}

void AceAbility::OnConfigurationChanged(const DeviceConfig& newConfig)
{
    auto container = AceContainer::GetContainerInstance(ACE_INSTANCE_ID);
    if (!container) {
        LOGE("container is null, change configuration failed.");
        return;
    }
    if (newConfig.colorMode != runArgs_.deviceConfig.colorMode) {
        container->UpdateColorMode(newConfig.colorMode);
        runArgs_.deviceConfig.colorMode = newConfig.colorMode;

        auto type = container->GetType();
        if (type == FrontendType::DECLARATIVE_JS) {
            SystemProperties::SetColorMode(runArgs_.deviceConfig.colorMode);
            container->NativeOnConfigurationUpdated(ACE_INSTANCE_ID);
            return;
        }

        auto context = container->GetPipelineContext();
        if (context == nullptr) {
            LOGE("context is null, OnConfigurationChanged failed.");
            return;
        }
    }
}

void AceAbility::Start()
{
    AceContainer::RunPage(ACE_INSTANCE_ID, UNUSED_PAGE_ID, runArgs_.url, "");
    RunEventLoop();
}

void AceAbility::Stop()
{
    auto container = AceContainer::GetContainerInstance(ACE_INSTANCE_ID);
    if (!container) {
        return;
    }

    container->GetTaskExecutor()->PostTask([]() { loopRunning_ = false; }, TaskExecutor::TaskType::PLATFORM);
}

#ifdef USE_GLFW_WINDOW
void AdaptDeviceType(AceRunArgs& runArgs)
{
    if (runArgs.deviceConfig.deviceType == DeviceType::PHONE) {
        runArgs.deviceConfig.density = runArgs.deviceWidth / SCREEN_DENSITY_COEFFICIENT_PHONE;
    } else if (runArgs.deviceConfig.deviceType == DeviceType::WATCH) {
        runArgs.deviceConfig.density = runArgs.deviceWidth / SCREEN_DENSITY_COEFFICIENT_WATCH;
    } else if (runArgs.deviceConfig.deviceType == DeviceType::TABLET) {
        runArgs.deviceConfig.density = runArgs.deviceWidth / SCREEN_DENSITY_COEFFICIENT_TABLE;
    } else if (runArgs.deviceConfig.deviceType == DeviceType::TV) {
        runArgs.deviceConfig.density = runArgs.deviceWidth / SCREEN_DENSITY_COEFFICIENT_TV;
    } else {
        LOGE("DeviceType not supported");
    }
}
#endif

void AceAbility::RunEventLoop()
{
    while (!FlutterDesktopWindowShouldClose(controller_) && loopRunning_) {
        FlutterDesktopWaitForEvents(controller_);
#ifdef USE_GLFW_WINDOW
        auto window = FlutterDesktopGetWindow(controller_);
        int width;
        int height;
        FlutterDesktopGetWindowSize(window, &width, &height);
        if (width != runArgs_.deviceWidth || height != runArgs_.deviceHeight) {
            AdaptDeviceType(runArgs_);
            SurfaceChanged(runArgs_.deviceConfig.orientation, runArgs_.deviceConfig.density, width, height);
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    loopRunning_ = true;

    // Currently exit loop is only to restart the AceContainer for real-time preivew case.
    // Previewer background thread will release the AceAbility instance and create new one,
    // then call the InitEnv() and Start() again.
    auto container = AceContainer::GetContainerInstance(ACE_INSTANCE_ID);
    if (!container) {
        LOGE("container is null");
        FlutterDesktopDestroyWindow(controller_);
        controller_ = nullptr;
        return;
    }
    auto viewPtr = container->GetAceView();
    AceContainer::DestroyContainer(ACE_INSTANCE_ID);

    FlutterDesktopDestroyWindow(controller_);
    if (viewPtr != nullptr) {
        delete viewPtr;
        viewPtr = nullptr;
    }
    controller_ = nullptr;
}

void AceAbility::SurfaceChanged(
    const DeviceOrientation& orientation, const double& resolution, int32_t& width, int32_t& height)
{
    auto container = AceContainer::GetContainerInstance(ACE_INSTANCE_ID);
    if (!container) {
        LOGE("container is null, SurfaceChanged failed.");
        return;
    }

    auto viewPtr = container->GetAceView();
    if (viewPtr == nullptr) {
        LOGE("aceView is null, SurfaceChanged failed.");
        return;
    }
    auto window = FlutterDesktopGetWindow(controller_);
    // Need to change the window resolution and then change the rendering resolution. Otherwise, the image may not adapt
    // to the new window after the window is modified.
    auto context = container->GetPipelineContext();
    if (context == nullptr) {
        LOGE("context is null, SurfaceChanged failed.");
        return;
    }
    context->GetTaskExecutor()->PostSyncTask(
        [window, &width, &height]() { FlutterDesktopSetWindowSize(window, width, height); },
        TaskExecutor::TaskType::PLATFORM);
    SystemProperties::InitDeviceInfo(
        width, height, orientation == DeviceOrientation::PORTRAIT ? 0 : 1, resolution, runArgs_.isRound);
    viewPtr->NotifyDensityChanged(resolution);
    viewPtr->NotifySurfaceChanged(width, height);
    if ((orientation != runArgs_.deviceConfig.orientation && configChanges_.watchOrientation) ||
        (resolution != runArgs_.deviceConfig.density && configChanges_.watchDensity) ||
        ((width != runArgs_.deviceWidth || height != runArgs_.deviceHeight) && configChanges_.watchLayout)) {
        container->NativeOnConfigurationUpdated(ACE_INSTANCE_ID);
    }
    runArgs_.deviceConfig.orientation = orientation;
    runArgs_.deviceConfig.density = resolution;
    runArgs_.deviceWidth = width;
    runArgs_.deviceHeight = height;
}

std::string AceAbility::GetJSONTree()
{
    std::string jsonTreeStr;
    OHOS::Ace::Framework::InspectorClient::GetInstance().AssembleJSONTreeStr(jsonTreeStr);
    return jsonTreeStr;
}

std::string AceAbility::GetDefaultJSONTree()
{
    std::string defaultJsonTreeStr;
    OHOS::Ace::Framework::InspectorClient::GetInstance().AssembleDefaultJSONTreeStr(defaultJsonTreeStr);
    return defaultJsonTreeStr;
}

void AceAbility::ReplacePage(const std::string& url, const std::string& params)
{
    auto container = AceContainer::GetContainerInstance(ACE_INSTANCE_ID);
    if (!container) {
        LOGE("container is null");
        return;
    }
    container->GetFrontend()->ReplacePage(url, params);
}

} // namespace OHOS::Ace::Platform
