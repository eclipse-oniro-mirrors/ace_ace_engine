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

#include "frameworks/bridge/declarative_frontend/engine/jsi/jsi_declarative_utils.h"

#include "base/log/event_report.h"
#include "core/common/ace_application_info.h"
#include "core/common/ace_engine.h"

namespace OHOS::Ace::Framework {

const char JS_CRASH[] = "Js Crash";
const char DESTROY_APP_ERROR[] = "Destroy application failed";
const char DESTROY_PAGE_FAILED[] = "Destroy page instance failed";
const char LOAD_JS_BUNDLE_FAILED[] = "JS framework load js bundle failed";
const char FIRE_EVENT_FAILED[] = "Fire event failed";

std::shared_ptr<JsRuntime> JsiDeclarativeUtils::runtime_;
JsErrorType JsiDeclarativeUtils::currentErrorType_;
int32_t JsiDeclarativeUtils::instanceId_;
std::string JsiDeclarativeUtils::pageUrl_;
RefPtr<JsAcePage> JsiDeclarativeUtils::page_;

std::string GetReason(JsErrorType errorType)
{
    std::string reasonStr;
    switch (errorType) {
        case OHOS::Ace::Framework::JsErrorType::JS_CRASH:
            reasonStr = JS_CRASH;
            break;
        case OHOS::Ace::Framework::JsErrorType::DESTROY_APP_ERROR:
            reasonStr = DESTROY_APP_ERROR;
            break;
        case OHOS::Ace::Framework::JsErrorType::DESTROY_PAGE_ERROR:
            reasonStr = DESTROY_PAGE_FAILED;
            break;
        case OHOS::Ace::Framework::JsErrorType::LOAD_JS_BUNDLE_ERROR:
            reasonStr = LOAD_JS_BUNDLE_FAILED;
            break;
        case OHOS::Ace::Framework::JsErrorType::FIRE_EVENT_ERROR:
            reasonStr = FIRE_EVENT_FAILED;
            break;
        default:
            reasonStr = JS_CRASH;
            break;
    }

    return reasonStr;
}

std::string JsiDeclarativeUtils::GenerateSummaryBody(std::shared_ptr<JsValue> error, int32_t instanceId,
    const std::string& pageUrl)
{
    std::string abilityName;
    if (AceEngine::Get().GetContainer(instanceId) != nullptr) {
        abilityName = AceEngine::Get().GetContainer(instanceId)->GetHostClassName();
    }

    std::string summaryBody;
    summaryBody.append("Lifetime: ")
        .append(std::to_string(OHOS::Ace::AceApplicationInfo::GetInstance().GetLifeTime()))
        .append("s")
        .append("\n");

    if (!abilityName.empty()) {
        summaryBody.append("Ability: ").append(abilityName).append("\n");
    }

    if (!pageUrl.empty()) {
        summaryBody.append("page: ").append(pageUrl).append("\n");
    }

    summaryBody.append("Js-Engine: ark\n");
    if (!error) {
        summaryBody.append("error uncaught: error is null");
        return summaryBody;
    }
    if (!error->IsObject(runtime_) || error->IsNull(runtime_)) {
        std::string errorInfo = error->ToString(runtime_);
        summaryBody.append(errorInfo).append("\n");
        return summaryBody;
    }
    shared_ptr<JsValue> message = error->GetProperty(runtime_, "message");
    std::string messageStr = message->ToString(runtime_);
    summaryBody.append("Error message: ");
    summaryBody.append(messageStr).append("\n");

    shared_ptr<JsValue> stack = error->GetProperty(runtime_, "stack");
    std::string stackStr = stack->ToString(runtime_);
    summaryBody.append("Stacktrace: \n");
    JsiDumpSourceFile(stackStr);
    summaryBody.append(stackStr);
    return summaryBody;
}

void JsiDeclarativeUtils::JsiDumpSourceFile(std::string& stackStr)
{
    RefPtr<RevSourceMap> pageMap;
    RefPtr<RevSourceMap> appMap;
    if (page_) {
        pageMap = page_->GetPageMap();
        appMap = page_->GetAppMap();
    }
    if (pageMap || appMap) {
        const std::string closeBrace = ")";
        const std::string openBrace = "(";
        const std::string appFile = "app_.js";
        int32_t offSet = 0;
        std::string sourceInfo;
        std::string line = "";
        std::string column = "";
        MappingInfo mapInfo;

        int32_t closeBracePos = static_cast<int32_t>(stackStr.find(closeBrace));
        int32_t openBracePos = static_cast<int32_t>(stackStr.find(openBrace));
        int32_t appFlag = static_cast<int32_t>(stackStr.find(appFile));

        int32_t flag = 0;
        for (int32_t i = closeBracePos - 1; i > 0; i--) {
            if (stackStr[i] == ':') {
                flag += 1;
                continue;
            }
            if (flag == 0) {
                column = stackStr[i] + column;
            } else if (flag == 1) {
                line = stackStr[i] + line;
            } else {
                break;
            }
        }

        if (line == "" || column == "") {
            LOGI("the stack without line or column info");
            return;
        }

        if (appFlag > 0 && appMap) {
            mapInfo = appMap->Find(StringUtils::StringToInt(line) - offSet, StringUtils::StringToInt(column));
        } else {
            mapInfo = pageMap->Find(StringUtils::StringToInt(line) - offSet, StringUtils::StringToInt(column));
        }

        sourceInfo = "(" + mapInfo.sources + ":" + std::to_string(mapInfo.row) + ":" +
                     std::to_string(mapInfo.col) + ")";
        stackStr.replace(openBracePos, closeBracePos - openBracePos + 1, sourceInfo);
    }
}

void JsiDeclarativeUtils::SetCurrentState(JsErrorType errorType, int32_t instanceId, const std::string& pageUrl,
    const RefPtr<JsAcePage>& page)
{
    currentErrorType_ = errorType;
    instanceId_ = instanceId;
    pageUrl_ = pageUrl;
    page_ = page;
}

void JsiDeclarativeUtils::SetRuntime(const std::shared_ptr<JsRuntime>& runtime,
    const std::shared_ptr<JsRuntime>& oldRuntime)
{
    if (runtime_ == oldRuntime || oldRuntime == nullptr) {
        runtime_ = runtime;
    }
}

void JsiDeclarativeUtils::ReportJsErrorEvent(std::shared_ptr<JsValue> error)
{
    if (!runtime_) {
        LOGI("ReportJsErrorEvent: jsi declarative engine has been destroyed");
        return;
    }
    LOGI("ReportJsErrorEvent");
    std::string reasonStr = GetReason(currentErrorType_);
    std::string summaryBody = GenerateSummaryBody(error, instanceId_, pageUrl_);
    LOGE("reasonStr: %{public}s", reasonStr.c_str());
    LOGE("summaryBody: \n%{public}s", summaryBody.c_str());
    EventReport::JsErrReport(AceApplicationInfo::GetInstance().GetPackageName(), reasonStr, summaryBody);
}

} // namespace OHOS::Ace::Framework
