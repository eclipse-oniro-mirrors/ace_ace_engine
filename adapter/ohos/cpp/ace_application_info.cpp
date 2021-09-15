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

#include "adapter/ohos/cpp/ace_application_info.h"

#include <dirent.h>
#include <iostream>
#include <sys/stat.h>

#include "contrib/minizip/unzip.h"
#include "init_data.h"
#include "unicode/locid.h"

#include "base/i18n/localization.h"
#include "base/log/ace_trace.h"
#include "base/log/log.h"
#include "base/resource/ace_res_config.h"
#include "base/resource/ace_res_data_struct.h"
#include "core/common/ace_engine.h"

namespace OHOS::Ace::Platform {

AceApplicationInfoImpl::AceApplicationInfoImpl() {}

AceApplicationInfoImpl::~AceApplicationInfoImpl() = default;

void AceApplicationInfoImpl::SetJsEngineParam(const std::string& key, const std::string& value)
{
    jsEngineParams_[key] = value;
}

std::string AceApplicationInfoImpl::GetJsEngineParam(const std::string& key) const
{
    std::string value;
    auto iter = jsEngineParams_.find(key);
    if (iter != jsEngineParams_.end()) {
        value = iter->second;
    }
    return value;
}

void AceApplicationInfoImpl::ChangeLocale(const std::string& language, const std::string& countryOrRegion)
{
    std::string languageLower;
    std::transform(language.begin(), language.end(), languageLower.begin(), ::tolower);

    std::string countryOrRegionUpper;
    std::transform(countryOrRegion.begin(), countryOrRegion.end(), countryOrRegionUpper.begin(), ::tolower);

    std::unique_ptr<Global::Resource::ResConfig> resConfig(Global::Resource::CreateResConfig());
    resourceManager_->GetResConfig(*resConfig);

    auto localeInfo = resConfig->GetLocaleInfo();
    if (localeInfo == nullptr) {
        LOGE("get local info failed");
        return;
    }

    auto script = localeInfo->getScript();
    resConfig->SetLocaleInfo(languageLower.c_str(), script, countryOrRegionUpper.c_str());
    resourceManager_->UpdateResConfig(*resConfig);

    SetLocale(languageLower, countryOrRegionUpper, (script == nullptr) ? "" : script, "");
}

std::vector<std::string> AceApplicationInfoImpl::GetLocaleFallback(const std::vector<std::string>& localeList) const
{
    ACE_SCOPED_TRACE("GetLocaleFallback");
    std::vector<std::string> fileList;
    AceResConfig::MatchAndSortI18nConfigs(localeList, localeTag_, fileList);
    return fileList;
}

std::vector<std::string> AceApplicationInfoImpl::GetResourceFallback(const std::vector<std::string>& resourceList) const
{
    ACE_SCOPED_TRACE("GetResourceFallback");
    std::vector<std::string> fileList;
    std::string deviceConfigTag = GetCurrentDeviceResTag();
    AceResConfig::MatchAndSortResConfigs(resourceList, deviceConfigTag, fileList);
    return fileList;
}

std::string AceApplicationInfoImpl::GetCurrentDeviceResTag() const
{
    ResolutionType resolutionType = AceResConfig::GetResolutionType(SystemProperties::GetResolution());
    AceResConfig deviceResConfig = AceResConfig(SystemProperties::GetMcc(), SystemProperties::GetMnc(),
        SystemProperties::GetDevcieOrientation(), SystemProperties::GetColorMode(), SystemProperties::GetDeviceType(),
        resolutionType);
    return AceResConfig::ConvertResConfigToTag(deviceResConfig, false);
}

void AceApplicationInfoImpl::SetLocale(
    const std::string& language,
    const std::string& countryOrRegion,
    const std::string& script,
    const std::string& keywordsAndValues)
{
    language_ = language;
    countryOrRegion_ = countryOrRegion;
    script_ = script;
    keywordsAndValues_ = keywordsAndValues;

    localeTag_ = language;
    if (!script_.empty()) {
        localeTag_.append("-" + script_);
    }
    localeTag_.append("-" + countryOrRegion_);

    icu::Locale locale(language_.c_str(), countryOrRegion.c_str());
    isRightToLeft_ = locale.isRightToLeft();

    auto languageList = Localization::GetLanguageList(language_);
    if (languageList.size() == 1) {
        Localization::SetLocale(
            language_, countryOrRegion_, script, languageList.front(), keywordsAndValues_);
    } else {
        auto selectLanguage = GetLocaleFallback(languageList);
        Localization::SetLocale(
            language_, countryOrRegion_, script, selectLanguage.front(), keywordsAndValues_);
    }
}

bool AceApplicationInfoImpl::GetFiles(const std::string& filePath, std::vector<std::string>& fileList) const
{
    const auto& packagePath = AceEngine::Get().GetPackagePath();
    const auto& assetBasePathSet = AceEngine::Get().GetAssetBasePath();
    if (packagePath.empty() || assetBasePathSet.empty()) {
        LOGE("the packagePath or assetBasePathSet is empty");
        return false;
    }

    for (const auto& assetBasePath : assetBasePathSet) {
        std::string dirPath = packagePath + assetBasePath + filePath;
        DIR* dir = opendir(dirPath.c_str());
        if (dir == nullptr) {
            continue;
        }
        struct dirent* dirEntry = readdir(dir);
        while (dirEntry != nullptr) {
            std::string file = dirEntry->d_name;
            std::string fullPath = dirPath + file;
            dirEntry = readdir(dir);

            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
                continue;
            }
            fileList.emplace_back(file);
        }
        closedir(dir);
    }
    return true;
}

std::vector<std::string> AceApplicationInfoImpl::SplitLocale(const std::string& desireLocales)
{
    std::vector<std::string> ret;
    if (desireLocales.empty()) {
        ret.emplace_back("en-US");
        return ret;
    }

    size_t rpos = -1;
    auto pos = desireLocales.find(',');
    while (pos != std::string::npos) {
        ret.emplace_back(desireLocales.substr(rpos + 1, pos - rpos - 1));
        rpos = pos;
        pos = desireLocales.find(',', rpos + 1);
    }
    ret.emplace_back(desireLocales.substr(rpos + 1, desireLocales.length() - rpos - 1));

    return ret;
}

bool AceApplicationInfoImpl::GetBundleInfo(const std::string& packageName, AceBundleInfo& bundleInfo)
{
    return false;
}

AceApplicationInfoImpl& AceApplicationInfoImpl::GetInstance()
{
    static AceApplicationInfoImpl instance;
    return instance;
}

} // namespace OHOS::Ace::Platform

namespace OHOS::Ace {

AceApplicationInfo& AceApplicationInfo::GetInstance()
{
    return Platform::AceApplicationInfoImpl::GetInstance();
}

} // namespace OHOS::Ace
