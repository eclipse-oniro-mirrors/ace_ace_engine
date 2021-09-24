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

#ifndef FOUNDATION_ACE_ACE_ENGINE_ADAPTER_OHOS_CPP_ACE_ABILITY_H
#define FOUNDATION_ACE_ACE_ENGINE_ADAPTER_OHOS_CPP_ACE_ABILITY_H

#include <string>
#include <vector>
#include "ability.h"
#include "ability_loader.h"
#include "want.h"

namespace OHOS::Ace {
using DialogCallback = std::function<void(std::string event, std::string param)>;
void showDialog(OHOS::sptr<OHOS::Window> window, std::string jsBoudle, std::string param, DialogCallback callback,
    const std::string& srcPath);

class AceAbility final : public OHOS::AppExecFwk::Ability {
public:
    AceAbility()
    {
        abilityId_ = instanceId_;
        instanceId_++;
    }
    virtual ~AceAbility() = default;

    void OnStart(const OHOS::AAFwk::Want& want) override;
    void OnStop() override;
    void OnActive() override;
    void OnInactive() override;
    void OnForeground(const OHOS::AAFwk::Want& want) override;
    void OnBackground() override;
    void OnBackPressed() override;
    void OnNewWant(const OHOS::AAFwk::Want& want) override;
    void OnRestoreAbilityState(const OHOS::AppExecFwk::PacMap& inState) override;
    void OnSaveAbilityState(OHOS::AppExecFwk::PacMap& outState) override;
    void OnConfigurationUpdated(const OHOS::AppExecFwk::Configuration& configuration) override;
    void OnAbilityResult(int requestCode, int resultCode, const OHOS::AAFwk::Want& resultData) override;
    void OnRequestPermissionsFromUserResult(
        int requestCode, const std::vector<std::string> &permissions, const std::vector<int> &grantResults) override;

    bool OnStartContinuation() override;
    bool OnSaveData(OHOS::AAFwk::WantParams &saveData) override;
    bool OnRestoreData(OHOS::AAFwk::WantParams &restoreData) override;
    void OnCompleteContinuation(int result) override;
    void OnRemoteTerminated() override;

private:
    static int32_t instanceId_;
    static const std::string START_PARAMS_KEY;
    static const std::string PAGE_URI;
    static const std::string CONTINUE_PARAMS_KEY;

    int32_t abilityId_ = 0;
    float density_ = 1.0f;
    std::string remotePageUrl_;
    std::string remoteData_;
    std::string pageUrl_;
};

} // namespace OHOS::Ace

#endif // FOUNDATION_ACE_ACE_ENGINE_ADAPTER_OHOS_CPP_ACE_ABILITY_H
