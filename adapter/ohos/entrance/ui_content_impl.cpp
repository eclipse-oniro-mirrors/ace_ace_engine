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

#include "adapter/ohos/entrance/ui_content_impl.h"

#include <atomic>

#include "ability_context.h"
#include "ability_info.h"
#include "configuration.h"
#include "init_data.h"

#ifdef ENABLE_ROSEN_BACKEND
#include "render_service_client/core/ui/rs_ui_director.h"
#endif

#include "adapter/ohos/entrance/ace_application_info.h"
#include "adapter/ohos/entrance/ace_container.h"
#include "adapter/ohos/entrance/file_asset_provider.h"
#include "adapter/ohos/entrance/flutter_ace_view.h"
#include "base/log/log.h"
#include "base/utils/system_properties.h"
#include "core/common/ace_engine.h"
#include "core/common/flutter/flutter_asset_manager.h"

namespace OHOS::Ace {

static std::atomic<int32_t> gInstanceId = 0;

using ContentFinishCallback = std::function<void()>;
class ContentEventCallback final : public Platform::PlatformEventCallback {
public:
    explicit ContentEventCallback(ContentFinishCallback onFinish) : onFinish_(onFinish) {}
    ~ContentEventCallback() = default;

    virtual void OnFinish() const
    {
        LOGI("UIContent OnFinish");
        if (onFinish_) {
            onFinish_();
        }
    }

    virtual void OnStatusBarBgColorChanged(uint32_t color)
    {
        LOGI("UIContent OnStatusBarBgColorChanged");
    }

private:
    ContentFinishCallback onFinish_;
};

extern "C" ACE_EXPORT void* OHOS_ACE_CreateUIContent(void* context, void* runtime)
{
    LOGI("Ace lib loaded, CreateUIContent.");
    return new UIContentImpl(reinterpret_cast<OHOS::AbilityRuntime::Context*>(context), runtime);
}

UIContentImpl::UIContentImpl(OHOS::AbilityRuntime::Context* context, void* runtime)
    : context_(context), runtime_(runtime)
{
    LOGI("Create UIContentImpl.");
}

void UIContentImpl::Initialize(OHOS::Rosen::Window* window, const std::string& url, NativeValue* storage)
{
    window_ = window;
    startUrl_ = url;
    if (!window_) {
        LOGE("Null window, can't initialize UI content");
        return;
    }
    if (!context_) {
        LOGE("Null ability, can't initialize UI content");
        return;
    }
    LOGI("Initialize UIContentImpl start.");
    SetHwIcuDirectory();

    std::unique_ptr<Global::Resource::ResConfig> resConfig(Global::Resource::CreateResConfig());
    auto resourceManager = context_->GetResourceManager();
    if (resourceManager != nullptr) {
        resourceManager->GetResConfig(*resConfig);
        auto localeInfo = resConfig->GetLocaleInfo();
        Platform::AceApplicationInfoImpl::GetInstance().SetResourceManager(resourceManager);
        if (localeInfo != nullptr) {
            auto language = localeInfo->getLanguage();
            auto region = localeInfo->getCountry();
            auto script = localeInfo->getScript();
            AceApplicationInfo::GetInstance().SetLocale((language == nullptr) ? "" : language,
                (region == nullptr) ? "" : region, (script == nullptr) ? "" : script, "");
        }
    }

    auto packagePathStr = context_->GetBundleCodePath();
    auto moduleInfo = context_->GetHapModuleInfo();
    if (moduleInfo != nullptr) {
        packagePathStr += "/" + moduleInfo->name + "/";
    }
    auto abilityContext = static_cast<OHOS::AbilityRuntime::AbilityContext*>(context_);
    auto info = abilityContext->GetAbilityInfo();
    std::string srcPath = "";
    if (info != nullptr && !info->srcPath.empty()) {
        srcPath = info->srcPath;
    }

    RefPtr<FlutterAssetManager> flutterAssetManager = Referenced::MakeRefPtr<FlutterAssetManager>();
    auto assetBasePathStr = { "assets/js/" + (srcPath.empty() ? "default" : srcPath) + "/",
        std::string("assets/js/share/") };

    if (flutterAssetManager && !packagePathStr.empty()) {
        auto assetProvider = AceType::MakeRefPtr<FileAssetProvider>();
        if (assetProvider->Initialize(packagePathStr, assetBasePathStr)) {
            LOGI("Push AssetProvider to queue.");
            flutterAssetManager->PushBack(std::move(assetProvider));
        }
    }

    std::string moduleName = info->moduleName;
    std::shared_ptr<OHOS::AppExecFwk::ApplicationInfo> appInfo = context_->GetApplicationInfo();
    std::vector<OHOS::AppExecFwk::ModuleInfo> moduleList = appInfo->moduleInfos;

    std::string resPath;
    for (const auto& module : moduleList) {
        if (module.moduleName == moduleName) {
            resPath = module.moduleSourceDir + "/assets/" + module.moduleName + "/";
            break;
        }
    }

    // create container
    instanceId_ = gInstanceId.fetch_add(1, std::memory_order_relaxed);
    auto container = AceType::MakeRefPtr<Platform::AceContainer>(instanceId_, FrontendType::DECLARATIVE_JS, true,
        context_, std::make_unique<ContentEventCallback>([context = context_] {
            if (context) {
                auto abilityContext = static_cast<OHOS::AbilityRuntime::AbilityContext*>(context);
                abilityContext->TerminateSelf();
            }
        }));
    AceEngine::Get().AddContainer(instanceId_, container);
    container->GetSettings().SetUsingSharedRuntime(true);
    container->SetSharedRuntime(runtime_);
    container->Initialize();
    auto front = container->GetFrontend();
    if (front) {
        front->UpdateState(Frontend::State::ON_CREATE);
        front->SetJsMessageDispatcher(container);
    }
    auto aceResCfg = container->GetResourceConfiguration();
    aceResCfg.SetOrientation(SystemProperties::GetDevcieOrientation());
    aceResCfg.SetDensity(SystemProperties::GetResolution());
    aceResCfg.SetDeviceType(SystemProperties::GetDeviceType());
    container->SetResourceConfiguration(aceResCfg);
    container->SetPackagePathStr(resPath);
    container->SetAssetManager(flutterAssetManager);

    // create ace_view
    auto flutterAceView =
        Platform::FlutterAceView::CreateView(instanceId_, container->GetSettings().usePlatformAsUIThread);
    Platform::FlutterAceView::SurfaceCreated(flutterAceView, window_);

    int32_t width = window_->GetRect().width_;
    int32_t height = window_->GetRect().height_;
    LOGI("UIContent Initialize: width: %{public}d, height: %{public}d", width, height);

    // set view
    Platform::AceContainer::SetView(flutterAceView, config_.Density(), width, height);
    Platform::FlutterAceView::SurfaceChanged(flutterAceView, width, height, config_.Orientation());

#ifdef ENABLE_ROSEN_BACKEND
    if (SystemProperties::GetRosenBackendEnabled()) {
        auto rsUiDirector = OHOS::Rosen::RSUIDirector::Create();
        if (rsUiDirector != nullptr) {
            rsUiDirector->SetRSSurfaceNode(window->GetSurfaceNode());
            rsUiDirector->SetSurfaceNodeSize(width, height);
            rsUiDirector->SetUITaskRunner(
                [taskExecutor = container->GetTaskExecutor()]
                    (const std::function<void()>& task) {
                        taskExecutor->PostTask(task, TaskExecutor::TaskType::UI);
                    });
            auto context = container->GetPipelineContext();
            if (context != nullptr) {
                context->SetRSUIDirector(rsUiDirector);
            }
            rsUiDirector->Init();
            LOGI("UIContent Init Rosen Backend");
        }
    }
#endif

    // run page.
    Platform::AceContainer::RunPage(
        instanceId_, Platform::AceContainer::GetContainer(instanceId_)->GeneratePageId(), startUrl_, "");
    LOGI("Initialize UIContentImpl done.");
}

void UIContentImpl::Foreground()
{
    LOGI("Show UIContent");
    Platform::AceContainer::OnShow(instanceId_);
}

void UIContentImpl::Background()
{
    LOGI("Hide UIContent");
    Platform::AceContainer::OnHide(instanceId_);
}

void UIContentImpl::Focus()
{
    LOGI("Active UIContent");
    Platform::AceContainer::OnActive(instanceId_);
}

void UIContentImpl::UnFocus()
{
    LOGI("Inactive UIContent");
    Platform::AceContainer::OnInactive(instanceId_);
}

void UIContentImpl::Destroy()
{
    LOGI("Destroy UIContent");
    Platform::AceContainer::DestroyContainer(instanceId_);
}

void UIContentImpl::Restore(OHOS::Rosen::Window* window, const std::string& contentInfo, NativeValue* storage)
{
    LOGI("UIContent Restore: mock %{public}s", contentInfo.c_str());
}

const std::string& UIContentImpl::GetContentInfo() const
{
    LOGI("UIContent GetContentInfo: mock");
    return "contentInfo";
}

bool UIContentImpl::ProcessBackPressed()
{
    LOGI("UIContent ProcessBackPressed");
    return Platform::AceContainer::OnBackPressed(instanceId_);
}

bool UIContentImpl::ProcessPointerEvent(const std::shared_ptr<OHOS::MMI::PointerEvent>& pointerEvent)
{
    LOGI("UIContent ProcessPointerEvent");
    auto container = Platform::AceContainer::GetContainer(instanceId_);
    if (container) {
        auto aceView = static_cast<Platform::FlutterAceView*>(container->GetAceView());
        Platform::FlutterAceView::DispatchTouchEvent(aceView, pointerEvent);
        return true;
    }
    return false;
}

bool UIContentImpl::ProcessKeyEvent(const std::shared_ptr<OHOS::MMI::KeyEvent>& touchEvent)
{
    LOGI("UIContent ProcessKeyEvent");
    return false;
}

bool UIContentImpl::ProcessAxisEvent(const std::shared_ptr<OHOS::MMI::AxisEvent>& axisEvent)
{
    LOGI("UIContent ProcessAxisEvent");
    return false;
}

bool UIContentImpl::ProcessVsyncEvent(uint64_t timeStampNanos)
{
    LOGI("UIContent ProcessVsyncEvent");
    return false;
}

void UIContentImpl::UpdateConfiguration(const std::shared_ptr<OHOS::AppExecFwk::Configuration>& config)
{
    if (!config) {
        LOGE("UIContent null config");
        return;
    }
    LOGI("UIContent UpdateConfiguration %{public}s", config->GetName().c_str());
}

void UIContentImpl::UpdateViewportConfig(const ViewportConfig& config)
{
    LOGI("UIContent UpdateViewportConfig %{public}s", config.ToString().c_str());
    auto container = Platform::AceContainer::GetContainer(instanceId_);
    if (container) {
#ifdef ENABLE_ROSEN_BACKEND
        auto context = container->GetPipelineContext();
        if (context) {
            auto rsUIDirector = context->GetRSUIDirector();
            if (rsUIDirector) {
                rsUIDirector->SetSurfaceNodeSize(config.Width(), config.Height());
            } else {
                LOGE("UpdateViewportConfig rsUIDirector is null.");
            }
        } else {
            LOGE("UpdateViewportConfig pipline context is null.");
        }
#endif

        auto aceView = static_cast<Platform::FlutterAceView*>(container->GetAceView());
        flutter::ViewportMetrics metrics;
        metrics.physical_width = config.Width();
        metrics.physical_height = config.Height();
        metrics.device_pixel_ratio = 2.0f; // temporary use 2.0 for debug device, should get from window
        Platform::FlutterAceView::SetViewportMetrics(aceView, metrics);
        Platform::FlutterAceView::SurfaceChanged(aceView, config.Width(), config.Height(), config.Orientation());
    }
    config_ = config;
}

} // namespace OHOS::Ace
