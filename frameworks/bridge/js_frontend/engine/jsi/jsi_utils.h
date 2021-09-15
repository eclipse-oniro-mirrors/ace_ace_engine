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

#ifndef FOUNDATION_ACE_FRAMEWORKS_BRIDGE_JS_FRONTEND_ENGINE_JSI_UTILS_H
#define FOUNDATION_ACE_FRAMEWORKS_BRIDGE_JS_FRONTEND_ENGINE_JSI_UTILS_H

#include "frameworks/bridge/js_frontend/engine/jsi/jsi_engine.h"
#include "frameworks/bridge/js_frontend/engine/jsi/js_runtime.h"
#include "frameworks/bridge/js_frontend/engine/jsi/js_value.h"

namespace OHOS::Ace::Framework {

enum class JsErrorType {
    JS_CRASH = 0,
    FIRE_EVENT_ERROR,
    DESTROY_APP_ERROR,
    DESTROY_PAGE_ERROR,
    LOAD_JS_BUNDLE_ERROR,
    ANIMATION_START_ERROR,
    ANIMATION_FINISH_ERROR,
    ANIMATION_CANCEL_ERROR,
    ANIMATION_REPEAT_ERROR,
    ANIMATION_FRAME_ERROR,
    STRINGFY_ERROR,
};

class JsiUtils {
public:
    static void SetCurrentState(const std::shared_ptr<JsRuntime>& runtime, JsErrorType
        errorType = JsErrorType::JS_CRASH, int32_t instanceId = 0, const std::string& pageUrl = "",
            const RefPtr<JsAcePage>& page = nullptr);
    static void ReportJsErrorEvent(std::shared_ptr<JsValue> error);

private:
    static std::shared_ptr<JsRuntime> runtime_;
    static JsErrorType currentErrorType_;
    static int32_t instanceId_;
    static std::string pageUrl_;
    static RefPtr<JsAcePage> page_;

    static std::string GenerateSummaryBody(std::shared_ptr<JsValue> error, int32_t instanceId,
        const std::string& pageUrl);
    static void JsiDumpSourceFile(std::string& stackStr);
};

} // namespace OHOS::Ace::Framework

#endif // FOUNDATION_ACE_FRAMEWORKS_BRIDGE_JS_FRONTEND_ENGINE_JSI_UTILS_H
