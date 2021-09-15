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

#include "frameworks/core/common/ace_application_info.h"

namespace OHOS::Ace {

class MockAceApplicationInfo : public AceApplicationInfo {
    void SetLocale(const std::string& language, const std::string& countryOrRegion, const std::string& script,
        const std::string& keywordsAndValues) override {};
    void ChangeLocale(const std::string& language, const std::string& countryOrRegion) override {};
    std::vector<std::string> GetLocaleFallback(const std::vector<std::string>& localeList) const override
    {
        std::vector<std::string> vector;
        return vector;
    };

    std::vector<std::string> GetResourceFallback(const std::vector<std::string>& resourceList) const override
    {
        std::vector<std::string> vector;
        return vector;
    };

    std::vector<std::string> GetStyleResourceFallback(const std::vector<std::string>& resourceList) const override
    {
        std::vector<std::string> vector;
        return vector;
    };

    std::vector<std::string> GetDeclarativeResourceFallback(const std::set<std::string>& resourceList) const override
    {
        std::vector<std::string> vector;
        return vector;
    }

    bool GetFiles(const std::string& filePath, std::vector<std::string>& fileList) const override
    {
        return false;
    };

    bool GetBundleInfo(const std::string& packageName, AceBundleInfo& bundleInfo) override
    {
        return false;
    };

    double GetLifeTime() const override
    {
        return 0;
    }

    std::string GetJsEngineParam(const std::string& key) const override
    {
        return "";
    }

    std::string GetCurrentDeviceResTag() const override
    {
        return "";
    }

    std::string GetCurrentDeviceDeclarativeResTag() const override
    {
        return "";
    }

    double GetTargetMediaScaleRatio(const std::string& targetResTag) const override
    {
        return 0.0;
    }

    bool IsRightToLeft() const
    {
        return true;
    }

    const std::string& GetPackageName() const
    {
        return packageName_;
    }

public:
    static MockAceApplicationInfo& GetInstance()
    {
        static MockAceApplicationInfo instance;
        return instance;
    }

    void SetResourceManager(std::shared_ptr<Global::Resource::ResourceManager>& resourceManager) override {}

    std::shared_ptr<Global::Resource::ResourceManager> GetResourceManager() override { return nullptr; }

private:
    std::string packageName_;
};

AceApplicationInfo& AceApplicationInfo::GetInstance()
{
    return MockAceApplicationInfo::GetInstance();
}

} // namespace OHOS::Ace