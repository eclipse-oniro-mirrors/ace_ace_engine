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

#include "core/components/web/resource/web_delegate.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "base/log/log.h"
#include "core/event/ace_event_helper.h"
#include "core/event/back_end_event_manager.h"
#include "frameworks/bridge/js_frontend/frontend_delegate_impl.h"

namespace OHOS::Ace {

namespace {

constexpr char WEB_METHOD_RELOAD[] = "reload";
constexpr char WEB_METHOD_ROUTER_BACK[] = "routerBack";
constexpr char WEB_METHOD_UPDATEURL[] = "updateUrl";
constexpr char WEB_METHOD_CHANGE_PAGE_URL[] = "changePageUrl";
constexpr char WEB_METHOD_PAGE_PATH_INVALID[] = "pagePathInvalid";
constexpr char WEB_EVENT_PAGESTART[] = "onPageStarted";
constexpr char WEB_EVENT_PAGEFINISH[] = "onPageFinished";
constexpr char WEB_EVENT_PAGEERROR[] = "onPageError";
constexpr char WEB_EVENT_ONMESSAGE[] = "onMessage";
constexpr char WEB_EVENT_ROUTERPUSH[] = "routerPush";

constexpr char WEB_CREATE[] = "web";
constexpr char NTC_PARAM_WEB[] = "web";
constexpr char NTC_PARAM_WIDTH[] = "width";
constexpr char NTC_PARAM_HEIGHT[] = "height";
constexpr char NTC_PARAM_LEFT[] = "left";
constexpr char NTC_PARAM_TOP[] = "top";
constexpr char NTC_ERROR[] = "create error";
constexpr char NTC_PARAM_SRC[] = "src";
constexpr char NTC_PARAM_ERROR_CODE[] = "errorCode";
constexpr char NTC_PARAM_URL[] = "url";
constexpr char NTC_PARAM_PAGE_URL[] = "pageUrl";
constexpr char NTC_PARAM_PAGE_INVALID[] = "pageInvalid";
constexpr char NTC_PARAM_DESCRIPTION[] = "description";
constexpr char WEB_ERROR_CODE_CREATEFAIL[] = "error-web-delegate-000001";
constexpr char WEB_ERROR_MSG_CREATEFAIL[] = "create web_delegate failed.";

} // namespace

WebDelegate::~WebDelegate()
{
    ReleasePlatformResource();
}

void WebDelegate::ReleasePlatformResource()
{
    auto delegate = WeakClaim(this).Upgrade();
    if (delegate) {
        delegate->Stop();
        delegate->Release();
    }
}

void WebDelegate::Stop()
{
    auto context = context_.Upgrade();
    if (!context) {
        LOGE("fail to get context");
        return;
    }
    auto platformTaskExecutor = SingleTaskExecutor::Make(context->GetTaskExecutor(),
        TaskExecutor::TaskType::PLATFORM);
    if (platformTaskExecutor.IsRunOnCurrentThread()) {
        UnregisterEvent();
    } else {
        platformTaskExecutor.PostTask([weak = WeakClaim(this)] {
            auto delegate = weak.Upgrade();
            if (delegate) {
                delegate->UnregisterEvent();
            }
        });
    }
}

void WebDelegate::UnregisterEvent()
{
    auto context = context_.Upgrade();
    if (!context) {
        LOGE("fail to get context");
        return;
    }
    auto resRegister = context->GetPlatformResRegister();
    if (resRegister == nullptr) {
        return;
    }
    resRegister->UnregisterEvent(MakeEventHash(WEB_EVENT_PAGESTART));
    resRegister->UnregisterEvent(MakeEventHash(WEB_EVENT_PAGEFINISH));
    resRegister->UnregisterEvent(MakeEventHash(WEB_EVENT_PAGEERROR));
    resRegister->UnregisterEvent(MakeEventHash(WEB_EVENT_ROUTERPUSH));
    resRegister->UnregisterEvent(MakeEventHash(WEB_EVENT_ONMESSAGE));
}

void WebDelegate::CreatePlatformResource(
    const Size& size, const Offset& position, const WeakPtr<PipelineContext>& context)
{
    ReleasePlatformResource();
    context_ = context;
    CreatePluginResource(size, position, context);

    auto reloadCallback = [weak = WeakClaim(this)]() {
        auto delegate = weak.Upgrade();
        if (!delegate) {
            return false;
        }
        delegate->Reload();
        return true;
    };
    WebClient::GetInstance().RegisterReloadCallback(reloadCallback);

    auto updateUrlCallback = [weak = WeakClaim(this)](const std::string& url) {
        auto delegate = weak.Upgrade();
        if (!delegate) {
            return false;
        }
        delegate->UpdateUrl(url);
        return true;
    };
    WebClient::GetInstance().RegisterUpdageUrlCallback(updateUrlCallback);
    InitWebEvent();
}

void WebDelegate::CreatePluginResource(
    const Size& size, const Offset& position, const WeakPtr<PipelineContext>& context)
{
    state_ = State::CREATING;

    auto webCom = webComponent_.Upgrade();
    if (!webCom) {
        state_ = State::CREATEFAILED;
        OnError(NTC_ERROR, "fail to call WebDelegate::Create due to webComponent is null");
        return;
    }

    auto pipelineContext = context.Upgrade();
    if (!pipelineContext) {
        state_ = State::CREATEFAILED;
        OnError(NTC_ERROR, "fail to call WebDelegate::Create due to context is null");
        return;
    }
    context_  = context;
    auto platformTaskExecutor = SingleTaskExecutor::Make(pipelineContext->GetTaskExecutor(),
                                                         TaskExecutor::TaskType::PLATFORM);
    auto resRegister = pipelineContext->GetPlatformResRegister();
    auto weakRes = AceType::WeakClaim(AceType::RawPtr(resRegister));
    platformTaskExecutor.PostTask([weakWeb = AceType::WeakClaim(this), weakRes, size, position] {
        auto webDelegate = weakWeb.Upgrade();
        if (webDelegate == nullptr) {
            LOGE("webDelegate is null!");
            return;
        }
        auto webCom = webDelegate->webComponent_.Upgrade();
        if (!webCom) {
            webDelegate->OnError(NTC_ERROR, "fail to call WebDelegate::SetSrc PostTask");
            return;
        }
        auto resRegister = weakRes.Upgrade();
        if (!resRegister) {
            if (webDelegate->onError_) {
                webDelegate->onError_(WEB_ERROR_CODE_CREATEFAIL, WEB_ERROR_MSG_CREATEFAIL);
            }
            return;
        }
        auto context = webDelegate->context_.Upgrade();
        if (!context) {
            LOGE("context is null");
            return;
        }

        std::string pageUrl;
        int32_t pageId;
        OHOS::Ace::Framework::DelegateClient::GetInstance().GetWebPageUrl(pageUrl, pageId);

        std::stringstream paramStream;
        paramStream << NTC_PARAM_WEB << WEB_PARAM_EQUALS << webDelegate->id_ << WEB_PARAM_AND
                    << NTC_PARAM_WIDTH << WEB_PARAM_EQUALS
                    << size.Width() * context->GetViewScale() << WEB_PARAM_AND
                    << NTC_PARAM_HEIGHT << WEB_PARAM_EQUALS
                    << size.Height() * context->GetViewScale() << WEB_PARAM_AND
                    << NTC_PARAM_LEFT << WEB_PARAM_EQUALS
                    << position.GetX() * context->GetViewScale() << WEB_PARAM_AND
                    << NTC_PARAM_TOP << WEB_PARAM_EQUALS
                    << position.GetY() * context->GetViewScale() << WEB_PARAM_AND
                    << NTC_PARAM_SRC << WEB_PARAM_EQUALS << webCom->GetSrc() << WEB_PARAM_AND
                    << NTC_PARAM_PAGE_URL << WEB_PARAM_EQUALS << pageUrl;

        std::string param = paramStream.str();
        webDelegate->id_ = resRegister->CreateResource(WEB_CREATE, param);
        if (webDelegate->id_ == INVALID_ID) {
            if (webDelegate->onError_) {
                webDelegate->onError_(WEB_ERROR_CODE_CREATEFAIL, WEB_ERROR_MSG_CREATEFAIL);
            }
            return;
        }
        webDelegate->state_ = State::CREATED;
        webDelegate->hash_ = webDelegate->MakeResourceHash();
        webDelegate->RegisterWebEvent();
        webDelegate->BindRouterBackMethod();
        webDelegate->BindPopPageSuccessMethod();
        webDelegate->BindIsPagePathInvalidMethod();
    });
}

void WebDelegate::InitWebEvent()
{
    auto webCom = webComponent_.Upgrade();
    if (!webCom) {
        state_ = State::CREATEFAILED;
        OnError(NTC_ERROR, "fail to call WebDelegate::Create due to webComponent is null");
        return;
    }
    if (!webCom->GetPageStartedEventId().IsEmpty()) {
        onPageStarted_ =
            AceAsyncEvent<void(const std::string&)>::Create(webCom->GetPageStartedEventId(), context_);
    }
    if (!webCom->GetPageFinishedEventId().IsEmpty()) {
        onPageFinished_ =
            AceAsyncEvent<void(const std::string&)>::Create(webCom->GetPageFinishedEventId(), context_);
    }
    if (!webCom->GetPageErrorEventId().IsEmpty()) {
        onPageError_ =
            AceAsyncEvent<void(const std::string&)>::Create(webCom->GetPageErrorEventId(), context_);
    }
    if (!webCom->GetMessageEventId().IsEmpty()) {
        onMessage_ =
            AceAsyncEvent<void(const std::string&)>::Create(webCom->GetMessageEventId(), context_);
    }
}

void WebDelegate::RegisterWebEvent()
{
    auto context = context_.Upgrade();
    if (!context) {
        return;
    }
    auto resRegister = context->GetPlatformResRegister();
    if (resRegister == nullptr) {
        return;
    }
    resRegister->RegisterEvent(MakeEventHash(WEB_EVENT_PAGESTART),
                               [weak = WeakClaim(this)](const std::string& param) {
                auto delegate = weak.Upgrade();
                if (delegate) {
                    delegate->OnPageStarted(param);
                }
            });
    resRegister->RegisterEvent(MakeEventHash(WEB_EVENT_PAGEFINISH),
                               [weak = WeakClaim(this)](const std::string& param) {
                auto delegate = weak.Upgrade();
                if (delegate) {
                    delegate->OnPageFinished(param);
                }
            });
    resRegister->RegisterEvent(MakeEventHash(WEB_EVENT_PAGEERROR),
                               [weak = WeakClaim(this)](const std::string& param) {
                auto delegate = weak.Upgrade();
                if (delegate) {
                    delegate->OnPageError(param);
                }
            });
    resRegister->RegisterEvent(MakeEventHash(WEB_EVENT_ROUTERPUSH),
                               [weak = WeakClaim(this)](const std::string& param) {
                auto delegate = weak.Upgrade();
                if (delegate) {
                    delegate->OnRouterPush(param);
                }
            });
    resRegister->RegisterEvent(MakeEventHash(WEB_EVENT_ONMESSAGE),
                               [weak = WeakClaim(this)](const std::string& param) {
                auto delegate = weak.Upgrade();
                if (delegate) {
                    delegate->OnMessage(param);
                }
            });
}

// upper ui componnet which inherite from WebComponent
// could implement some curtain createdCallback to customized controller interface
// eg: web.loadurl.
void WebDelegate::AddCreatedCallback(const CreatedCallback& createdCallback)
{
    ACE_DCHECK(createdCallback != nullptr);
    ACE_DCHECK(state_ != State::RELEASED);
    createdCallbacks_.emplace_back(createdCallback);
}

void WebDelegate::RemoveCreatedCallback()
{
    ACE_DCHECK(state_ != State::RELEASED);
    createdCallbacks_.pop_back();
}

void WebDelegate::AddReleasedCallback(const ReleasedCallback& releasedCallback)
{
    ACE_DCHECK(releasedCallback != nullptr && state_ != State::RELEASED);
    releasedCallbacks_.emplace_back(releasedCallback);
}

void WebDelegate::RemoveReleasedCallback()
{
    ACE_DCHECK(state_ != State::RELEASED);
    releasedCallbacks_.pop_back();
}

void WebDelegate::Reload()
{
    hash_ = MakeResourceHash();
    reloadMethod_ = MakeMethodHash(WEB_METHOD_RELOAD);
    CallResRegisterMethod(reloadMethod_, WEB_PARAM_NONE, nullptr);
}

void WebDelegate::UpdateUrl(const std::string& url)
{
    hash_ = MakeResourceHash();
    updateUrlMethod_ = MakeMethodHash(WEB_METHOD_UPDATEURL);
    std::stringstream paramStream;
    paramStream << NTC_PARAM_SRC << WEB_PARAM_EQUALS << url;
    std::string param = paramStream.str();
    CallResRegisterMethod(updateUrlMethod_, param, nullptr);
}

void WebDelegate::CallWebRouterBack()
{
    hash_ = MakeResourceHash();
    routerBackMethod_ = MakeMethodHash(WEB_METHOD_ROUTER_BACK);
    CallResRegisterMethod(routerBackMethod_, WEB_PARAM_NONE, nullptr);
}

void WebDelegate::CallPopPageSuccessPageUrl(const std::string& url)
{
    hash_ = MakeResourceHash();
    changePageUrlMethod_ = MakeMethodHash(WEB_METHOD_CHANGE_PAGE_URL);
    std::stringstream paramStream;
    paramStream << NTC_PARAM_PAGE_URL << WEB_PARAM_EQUALS << url;
    std::string param = paramStream.str();
    CallResRegisterMethod(changePageUrlMethod_, param, nullptr);
}

void WebDelegate::CallIsPagePathInvalid(const bool& isPageInvalid)
{
    hash_ = MakeResourceHash();
    isPagePathInvalidMethod_ = MakeMethodHash(WEB_METHOD_PAGE_PATH_INVALID);
    std::stringstream paramStream;
    paramStream << NTC_PARAM_PAGE_INVALID << WEB_PARAM_EQUALS << isPageInvalid;
    std::string param = paramStream.str();
    CallResRegisterMethod(isPagePathInvalidMethod_, param, nullptr);
}

void WebDelegate::OnPageStarted(const std::string& param)
{
    if (onPageStarted_) {
        std::string paramStart = std::string(R"(")").append(param).append(std::string(R"(")"));
        std::string urlParam =
            std::string(R"("pagestart",{"url":)").append(paramStart.append("},null"));
        onPageStarted_(urlParam);
    }
}

void WebDelegate::OnPageFinished(const std::string& param)
{
    if (onPageFinished_) {
        std::string paramFinish = std::string(R"(")").append(param).append(std::string(R"(")"));
        std::string urlParam =
            std::string(R"("pagefinish",{"url":)").append(paramFinish.append("},null"));
        onPageFinished_(urlParam);
    }
}

void WebDelegate::OnPageError(const std::string& param)
{
    if (onPageError_) {
        int32_t errorCode = GetIntParam(param, NTC_PARAM_ERROR_CODE);
        std::string url = GetUrlStringParam(param, NTC_PARAM_URL);
        std::string description = GetStringParam(param, NTC_PARAM_DESCRIPTION);

        std::string paramUrl = std::string(R"(")").append(url)
                                                  .append(std::string(R"(")"))
                                                  .append(",");

        std::string paramErrorCode = std::string(R"(")").append(NTC_PARAM_ERROR_CODE)
                                                        .append(std::string(R"(")"))
                                                        .append(":")
                                                        .append(std::to_string(errorCode))
                                                        .append(",");

        std::string paramDesc = std::string(R"(")").append(NTC_PARAM_DESCRIPTION)
                                                   .append(std::string(R"(")"))
                                                   .append(":")
                                                   .append(std::string(R"(")")
                                                   .append(description)
                                                   .append(std::string(R"(")")));
        std::string errorParam = std::string(R"("error",{"url":)")
                                           .append((paramUrl + paramErrorCode + paramDesc)
                                           .append("},null"));
        onPageError_(errorParam);
    }
}

void WebDelegate::OnMessage(const std::string& param)
{
    std::string removeQuotes;
    removeQuotes = param;
    removeQuotes.erase(std::remove(removeQuotes.begin(), removeQuotes.end(), '\"'),
                       removeQuotes.end());
    if (onMessage_) {
        std::string paramMessage = std::string(R"(")").append(removeQuotes).append(std::string(R"(")"));
        std::string messageParam =
                std::string(R"("message",{"message":)").append(paramMessage.append("},null"));
        onMessage_(messageParam);
    }
}

void WebDelegate::OnRouterPush(const std::string& param)
{
    OHOS::Ace::Framework::DelegateClient::GetInstance().RouterPush(param);
}

std::string WebDelegate::GetUrlStringParam(const std::string& param, const std::string& name) const
{
    size_t len = name.length();
    size_t posErrorCode = param.find(NTC_PARAM_ERROR_CODE);
    size_t pos = param.find(name);
    std::string result;

    if (pos != std::string::npos && posErrorCode != std::string::npos) {
        std::stringstream ss;

        ss << param.substr(pos + 1 + len, posErrorCode - 5);
        ss >> result;
    }
    return result;
}

void WebDelegate::BindRouterBackMethod()
{
    auto context = context_.Upgrade();
    if (context) {
        context->SetRouterBackEventHandler([weak = WeakClaim(this)] {
            auto delegate = weak.Upgrade();
            if (delegate) {
                delegate->CallWebRouterBack();
            }
        });
    }
}

void WebDelegate::BindPopPageSuccessMethod()
{
    auto context = context_.Upgrade();
    if (context) {
        context->SetPopPageSuccessEventHandler(
            [weak = WeakClaim(this)](const std::string& pageUrl, const int32_t pageId) {
                std::string url = pageUrl.substr(0, pageUrl.length() - 3);
                auto delegate = weak.Upgrade();
                if (delegate) {
                    delegate->CallPopPageSuccessPageUrl(url);
                }
        });
    }
}

void WebDelegate::BindIsPagePathInvalidMethod()
{
    auto context = context_.Upgrade();
    if (context) {
        context->SetIsPagePathInvalidEventHandler([weak = WeakClaim(this)](bool& isPageInvalid) {
            auto delegate = weak.Upgrade();
            if (delegate) {
                delegate->CallIsPagePathInvalid(isPageInvalid);
            }
        });
    }
}

} // namespace OHOS::Ace
