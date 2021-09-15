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

#include "core/common/ace_engine.h"

#include "base/log/log.h"
#include "base/memory/memory_monitor.h"
#include "base/thread/background_task_executor.h"
#include "core/common/ace_page.h"
#include "core/image/image_cache.h"
#include "unistd.h"

namespace OHOS::Ace {
namespace {

std::unique_ptr<AceEngine> g_aceEngine;

}

AceEngine::AceEngine()
{
    watchDog_ = AceType::MakeRefPtr<WatchDog>();
}

AceEngine& AceEngine::Get()
{
    if (!g_aceEngine) {
        LOGI("AceEngine initialized in first time");
        g_aceEngine.reset(new AceEngine());
    }
    return *g_aceEngine;
}

void AceEngine::AddContainer(int32_t instanceId, const RefPtr<Container>& container)
{
    LOGI("AddContainer %{public}d", instanceId);
    std::lock_guard<std::mutex> lock(mutex_);
    const auto result = containerMap_.try_emplace(instanceId, container);
    if (!result.second) {
        LOGW("already have container of this instance id: %{public}d", instanceId);
    }
}

void AceEngine::RemoveContainer(int32_t instanceId)
{
    LOGI("RemoveContainer %{public}d", instanceId);
    size_t num = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        num = containerMap_.erase(instanceId);
    }
    if (num == 0) {
        LOGW("container not found with instance id: %{public}d", instanceId);
    }
    watchDog_->Unregister(instanceId);
}

void AceEngine::Dump(const std::vector<std::string>& params) const
{
    std::unordered_map<int32_t, RefPtr<Container>> copied;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        copied = containerMap_;
    }
    for (const auto& container : copied) {
        auto pipelineContext = container.second->GetPipelineContext();
        pipelineContext->GetTaskExecutor()->PostSyncTask(
            [params, container = container.second]() { container->Dump(params); }, TaskExecutor::TaskType::UI);
    }
}

RefPtr<Container> AceEngine::GetContainer(int32_t instanceId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto container = containerMap_.find(instanceId);
    if (container != containerMap_.end()) {
        return container->second;
    } else {
        return nullptr;
    }
}

void AceEngine::RegisterToWatchDog(int32_t instanceId, const RefPtr<TaskExecutor>& taskExecutor)
{
    watchDog_->Register(instanceId, taskExecutor);
}

void AceEngine::BuriedBomb(int32_t instanceId, uint64_t bombId)
{
    watchDog_->BuriedBomb(instanceId, bombId);
}

void AceEngine::DefusingBomb(int32_t instanceId)
{
    watchDog_->DefusingBomb(instanceId);
}

void AceEngine::SetPackageName(const std::string& packageName)
{
    packageName_ = packageName;
}

const std::string& AceEngine::GetPackageName() const
{
    return packageName_;
}

void AceEngine::SetPackagePath(const std::string& packagePath)
{
    packagePath_ = packagePath;
}

const std::string& AceEngine::GetPackagePath() const
{
    return packagePath_;
}

void AceEngine::SetAssetBasePath(const std::string& assetBasePath)
{
    assetBasePathSet_.insert(assetBasePath);
}

const std::set<std::string>& AceEngine::GetAssetBasePath() const
{
    return assetBasePathSet_;
}

RefPtr<Frontend> AceEngine::GetForegroundFrontend() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [first, second] : containerMap_) {
        auto frontend = second->GetFrontend();
        if (frontend && (frontend->GetType() == FrontendType::JS)) {
            if (frontend->IsForeground()) {
                return frontend;
            }
        }
    }
    LOGW("container not found foreground frontend");
    return nullptr;
}

void AceEngine::SetUid(int32_t uid)
{
    uid_ = uid;
}

int32_t AceEngine::GetUid() const
{
    return uid_;
}

void AceEngine::SetProcessName(const std::string& processName)
{
    processName_ = processName;
}

const std::string& AceEngine::GetProcessName() const
{
    return processName_;
}

void AceEngine::TriggerGarbageCollection()
{
    std::unordered_map<int32_t, RefPtr<Container>> copied;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (containerMap_.empty()) {
            return;
        }
        copied = containerMap_;
    }

    auto taskExecutor = copied.begin()->second->GetTaskExecutor();
    taskExecutor->PostTask([] { PurgeMallocCache(); }, TaskExecutor::TaskType::PLATFORM);
#if defined(OHOS_PLATFORM) && defined(ENABLE_NATIVE_VIEW)
    // GPU and IO thread is shared while enable native view
    taskExecutor->PostTask([] { PurgeMallocCache(); }, TaskExecutor::TaskType::GPU);
    taskExecutor->PostTask([] { PurgeMallocCache(); }, TaskExecutor::TaskType::IO);
#endif

    for (const auto& container : copied) {
        container.second->TriggerGarbageCollection();
    }

    ImageCache::Purge();
    BackgroundTaskExecutor::GetInstance().TriggerGarbageCollection();
    PurgeMallocCache();
}

void AceEngine::NotifyContainers(const std::function<void(const RefPtr<Container>&)>& callback)
{
    if (!callback) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [first, second] : containerMap_) {
        callback(second);
    }
}

const std::string AceEngine::GetAssetAbsolutePath(const std::string& path) const
{
    for (auto basePath: assetBasePathSet_) {
        std::string assetPath;
        assetPath.append(packagePath_).append(basePath).append(path);
        if (access(assetPath.c_str(), F_OK) == 0) {
            return "file://" + assetPath;
        }
    }
    return path;
}

} // namespace OHOS::Ace
