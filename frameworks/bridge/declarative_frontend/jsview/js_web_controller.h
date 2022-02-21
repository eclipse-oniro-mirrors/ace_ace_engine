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

#ifndef FRAMEWORKS_BRIDGE_DECLARATIVE_FRONTEND_JS_VIEW_JS_WEB_CONTROLLER_H
#define FRAMEWORKS_BRIDGE_DECLARATIVE_FRONTEND_JS_VIEW_JS_WEB_CONTROLLER_H

#include "base/memory/referenced.h"
#include "bridge/declarative_frontend/engine/bindings.h"
#include "core/components/declaration/web/web_declaration.h"
#include "core/components/web/web_component.h"
#include "frameworks/bridge/declarative_frontend/jsview/js_web.h"

namespace OHOS::Ace::Framework {
class JSWebController : public Referenced {
public:
    JSWebController() = default;
    ~JSWebController() override = default;

    static void JSBind(BindingTarget globalObj);
    static void Constructor(const JSCallbackInfo& args);
    static void Destructor(JSWebController* webController);

    void Reload() const;
    void LoadUrl(const JSCallbackInfo& args);
    void ExecuteTypeScript(const JSCallbackInfo& args);
    void LoadDataWithBaseUrl(const JSCallbackInfo& args);

    // Backward and Forward for web page
    void Backward(const JSCallbackInfo& args);
    void Forward(const JSCallbackInfo& args);
    void AccessStep(const JSCallbackInfo& args);
    void AccessBackward(const JSCallbackInfo& args);
    void AccessForward(const JSCallbackInfo& args);

    const RefPtr<WebController>& GetController() const
    {
        return webController_;
    }

    void SetController(const RefPtr<WebController>& webController)
    {
        webController_ = webController;
    }
private:
    RefPtr<WebController> webController_;
    ACE_DISALLOW_COPY_AND_MOVE(JSWebController);
};
} // namespace OHOS::Ace::Framework
#endif // FRAMEWORKS_BRIDGE_DECLARATIVE_FRONTEND_JS_VIEW_JS_WEB_CONTROLLER_H