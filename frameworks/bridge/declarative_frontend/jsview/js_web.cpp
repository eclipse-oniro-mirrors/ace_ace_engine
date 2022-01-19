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

#include "frameworks/bridge/declarative_frontend/jsview/js_web.h"

#include <string>
#include "base/memory/referenced.h"
#include "bridge/declarative_frontend/jsview/js_view_common_def.h"
#include "bridge/declarative_frontend/view_stack_processor.h"
#include "core/components/web/web_component.h"
#include "core/components/web/web_event.h"
#include "frameworks/bridge/declarative_frontend/engine/js_ref_ptr.h"
#include "frameworks/bridge/declarative_frontend/jsview/js_web_controller.h"

namespace OHOS::Ace::Framework {
void JSWeb::JSBind(BindingTarget globalObj)
{
    JSClass<JSWeb>::Declare("Web");
    JSClass<JSWeb>::StaticMethod("create", &JSWeb::Create);
    JSClass<JSWeb>::StaticMethod("onPageStart", &JSWeb::OnPageStart);
    JSClass<JSWeb>::StaticMethod("onPageFinish", &JSWeb::OnPageFinish);
    JSClass<JSWeb>::StaticMethod("onRequestFocus", &JSWeb::OnRequestFocus);
    JSClass<JSWeb>::StaticMethod("onError", &JSWeb::OnError);
    JSClass<JSWeb>::StaticMethod("onMessage", &JSWeb::OnMessage);
    JSClass<JSWeb>::StaticMethod("javaScriptEnabled", &JSWeb::JsEnabled);
    JSClass<JSWeb>::StaticMethod("contentAccessEnabled", &JSWeb::ContentAccessEnabled);
    JSClass<JSWeb>::StaticMethod("fileAccessEnabled", &JSWeb::FileAccessEnabled);
    JSClass<JSWeb>::Inherit<JSViewAbstract>();
    JSClass<JSWeb>::Bind(globalObj);
}

JSRef<JSVal> LoadWebPageFinishEventToJSValue(const LoadWebPageFinishEvent& eventInfo)
{
    JSRef<JSObject> obj = JSRef<JSObject>::New();
    obj->SetProperty("url", eventInfo.GetLoadedUrl());
    return JSRef<JSVal>::Cast(obj);
}

JSRef<JSVal> LoadWebRequestFocusEventToJSValue(const LoadWebRequestFocusEvent& eventInfo)
{
    return JSRef<JSVal>::Make(ToJSValue(eventInfo.GetRequestFocus()));
}

void JSWeb::Create(const JSCallbackInfo& info)
{
    if (info.Length() < 1 || !info[0]->IsObject()) {
        LOGI("web create error, info is non-vaild");
        return;
    }
    auto paramObject = JSRef<JSObject>::Cast(info[0]);

    JSRef<JSVal> srcValue = paramObject->GetProperty("src");
    std::string webSrc;
    RefPtr<WebComponent> webComponent;
    if (ParseJsMedia(srcValue, webSrc)) {
        int np = webSrc.find_first_of("/");
        std::string tempSrc = webSrc.erase(np, 1);
        webComponent = AceType::MakeRefPtr<OHOS::Ace::WebComponent>(tempSrc);
        webComponent->SetSrc(tempSrc);
    } else {
        auto src = paramObject->GetProperty("src");
        if (!src->IsString()) {
            LOGI("web create error, src is non-vaild");
            return;
        }
        webComponent = AceType::MakeRefPtr<OHOS::Ace::WebComponent>(src->ToString());
        webComponent->SetSrc(src->ToString());
    }

    auto controllerObj = paramObject->GetProperty("controller");
    if (!controllerObj->IsObject()) {
        LOGI("web create error, controller is non-vaild");
        return;
    }
    auto controller = JSRef<JSObject>::Cast(controllerObj)->Unwrap<JSWebController>();
    if (controller) {
        webComponent->SetWebController(controller->GetController());
    }
    ViewStackProcessor::GetInstance()->Push(webComponent);
}

void JSWeb::OnPageStart(const JSCallbackInfo& args)
{
    if (!JSViewBindEvent(&WebComponent::SetOnPageStart, args)) {
        LOGW("Failed to bind start event");
    }

    args.ReturnSelf();
}

void JSWeb::OnPageFinish(const JSCallbackInfo& args)
{
    if (!args[0]->IsFunction()) {
        return;
    }
    auto jsFunc = AceType::MakeRefPtr<JsEventFunction<LoadWebPageFinishEvent, 1>>(
        JSRef<JSFunc>::Cast(args[0]), LoadWebPageFinishEventToJSValue);
    auto eventMarker = EventMarker([execCtx = args.GetExecutionContext(), func = std::move(jsFunc)]
        (const BaseEventInfo* info) {
            JAVASCRIPT_EXECUTION_SCOPE_WITH_CHECK(execCtx);
            auto eventInfo = TypeInfoHelper::DynamicCast<LoadWebPageFinishEvent>(info);
            func->Execute(*eventInfo);
        });
    auto webComponent = AceType::DynamicCast<WebComponent>(ViewStackProcessor::GetInstance()->GetMainComponent());
    webComponent->SetPageFinishedEventId(eventMarker);
}

void JSWeb::OnRequestFocus(const JSCallbackInfo& args)
{
    if (!args[0]->IsFunction()) {
        return;
    }
    auto jsFunc = AceType::MakeRefPtr<JsEventFunction<LoadWebRequestFocusEvent, 1>>(
        JSRef<JSFunc>::Cast(args[0]), LoadWebRequestFocusEventToJSValue);
    auto eventMarker = EventMarker([execCtx = args.GetExecutionContext(), func = std::move(jsFunc)]
        (const BaseEventInfo* info) {
            JAVASCRIPT_EXECUTION_SCOPE_WITH_CHECK(execCtx);
            auto eventInfo = TypeInfoHelper::DynamicCast<LoadWebRequestFocusEvent>(info);
            func->Execute(*eventInfo);
        });
    auto webComponent = AceType::DynamicCast<WebComponent>(ViewStackProcessor::GetInstance()->GetMainComponent());
    webComponent->SetRequestFocusEventId(eventMarker);
}

void JSWeb::OnError(const JSCallbackInfo& args)
{
    if (!JSViewBindEvent(&WebComponent::SetOnError, args)) {
        LOGW("Failed to bind error event");
    }

    args.ReturnSelf();
}

void JSWeb::OnMessage(const JSCallbackInfo& args)
{
    if (!JSViewBindEvent(&WebComponent::SetOnMessage, args)) {
        LOGW("Failed to bind message event");
    }

    args.ReturnSelf();
}

void JSWeb::JsEnabled(bool isJsEnabled)
{
    auto stack = ViewStackProcessor::GetInstance();
    auto webComponent = AceType::DynamicCast<WebComponent>(stack->GetMainComponent());
    if (!webComponent) {
        LOGE("JSWeb: MainComponent is null.");
        return;
    }
    webComponent->SetJsEnabled(isJsEnabled);
}

void JSWeb::ContentAccessEnabled(bool isContentAccessEnabled)
{
    auto stack = ViewStackProcessor::GetInstance();
    auto webComponent = AceType::DynamicCast<WebComponent>(stack->GetMainComponent());
    if (!webComponent) {
        LOGE("JSWeb: MainComponent is null.");
        return;
    }
    webComponent->SetContentAccessEnabled(isContentAccessEnabled);
}

void JSWeb::FileAccessEnabled(bool isFileAccessEnabled)
{
    auto stack = ViewStackProcessor::GetInstance();
    auto webComponent = AceType::DynamicCast<WebComponent>(stack->GetMainComponent());
    if (!webComponent) {
        LOGE("JSWeb: MainComponent is null.");
        return;
    }
    webComponent->SetFileAccessEnabled(isFileAccessEnabled);
}
} // namespace OHOS::Ace::Framework
