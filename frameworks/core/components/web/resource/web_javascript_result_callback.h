/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_WEB_RESOURCE_WEB_JAVASCRIPT_RESULT_CALLBACK_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_WEB_RESOURCE_WEB_JAVASCRIPT_RESULT_CALLBACK_H

#include <string>
#include <vector>
#include <memory>
#include <condition_variable>

#include "webview_javascript_result_callback.h"
#include "webview_value.h"
#include "core/components/web/resource/web_javascript_value.h"
#include "core/pipeline/pipeline_context.h"
#include "base/memory/ace_type.h"

namespace OHOS::Ace {
using namespace OHOS::WebView;
class WebJavaScriptResultCallBack : public WebViewJavaScriptResultCallBack {
public:
    WebJavaScriptResultCallBack(OHOS::Ace::WeakPtr<OHOS::Ace::PipelineContext> context, int32_t instanceId)
        : context_(context), instanceId_(instanceId) {}
    ~WebJavaScriptResultCallBack() = default;

    std::shared_ptr<WebViewValue> GetJavaScriptResult(std::vector<std::shared_ptr<WebViewValue>> args,
                                                       const std::string& method,
                                                       const std::string& object_name) override;

    using JavaScriptCallBackImpl = std::function<std::shared_ptr<WebJSValue>(
        const std::string& objectName,
        const std::string& objectMethod,
        const std::vector<std::shared_ptr<WebJSValue>>& args)>;
    void SetJavaScriptCallBack(const JavaScriptCallBackImpl&& javaScriptCallBackImpl)
    {
        javaScriptCallBackImpl_ = javaScriptCallBackImpl;
    }

protected:
    JavaScriptCallBackImpl javaScriptCallBackImpl_;

private:
    OHOS::Ace::WeakPtr<OHOS::Ace::PipelineContext> context_;
    static std::condition_variable initCv_;
    std::mutex initMtx_;
    int32_t instanceId_ = -1;
};
} // namespace OHOS::Ace

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_WEB_RESOURCE_WEB_JAVASCRIPT_RESULT_CALLBACK_H
