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

#ifndef FRAMEWORKS_BRIDGE_DECLARATIVE_FRONTEND_JS_VIEW_JS_VIEW_H
#define FRAMEWORKS_BRIDGE_DECLARATIVE_FRONTEND_JS_VIEW_JS_VIEW_H

#include <list>

#include "core/pipeline/base/composed_component.h"
#include "frameworks/bridge/declarative_frontend/engine/js_ref_ptr.h"
#include "frameworks/bridge/declarative_frontend/jsview/js_view_abstract.h"
#include "frameworks/bridge/declarative_frontend/jsview/js_view_functions.h"

namespace OHOS::Ace {

class ComposedElement;

} // namespace OHOS::Ace

namespace OHOS::Ace::Framework {

class JSView;

class JSView : public JSViewAbstract, public Referenced {
public:
    JSView(const std::string& viewId, JSRef<JSObject> jsObject, JSRef<JSFunc> jsRenderFunction);
    virtual ~JSView();

    RefPtr<OHOS::Ace::Component> InternalRender(const RefPtr<Component>& parent);
    void Destroy(JSView* parentCustomView);
    RefPtr<Component> CreateComponent();
    RefPtr<PageTransitionComponent> BuildPageTransitionComponent();

    void MarkNeedUpdate();
    bool NeedsUpdate()
    {
        return needsUpdate_;
    }

    /**
     * Views which do not have a state can mark static.
     * The element will be reused and re-render will be skipped.
     */
    void MarkStatic()
    {
        isStatic_ = true;
    }

    bool IsStatic()
    {
        return isStatic_;
    }

    /**
     * During render function execution, the child customview with same id will
     * be recycled if they exist already in our child map. The ones which are not
     * recycled needs to be cleaned. So After render function execution, clean the
     * abandoned child customview.
     */
    void CleanUpAbandonedChild();

    /**
     * Retries the customview child for recycling
     * always use FindChildById to be certain before calling this method
     */
    JSRefPtr<JSView> GetChildById(const std::string& viewId);

    void FindChildById(const JSCallbackInfo& info);
    void GetContext(const JSCallbackInfo& info);
    void GetContentStorage(const JSCallbackInfo& info);

    void FireOnShow()
    {
        if (jsViewFunction_) {
            ACE_SCORING_EVENT("Component[" + viewId_ + "].OnShow");
            jsViewFunction_->ExecuteShow();
        }
    }

    void FireOnHide()
    {
        if (jsViewFunction_) {
            ACE_SCORING_EVENT("Component[" + viewId_ + "].OnHide");
            jsViewFunction_->ExecuteHide();
        }
    }

    bool FireOnBackPress()
    {
        if (jsViewFunction_) {
            ACE_SCORING_EVENT("Component[" + viewId_ + "].OnBackPress");
            return jsViewFunction_->ExecuteOnBackPress();
        }
        return false;
    }

    void SetContext(const JSExecutionContext& context)
    {
        jsViewFunction_->SetContext(context);
    }

    void ExecuteUpdateWithValueParams(const std::string& jsonData)
    {
        jsViewFunction_->ExecuteUpdateWithValueParams(jsonData);
    }

    /**
     * New CustomView child will be added to the map.
     * and it can be reterieved for recycling in next render function
     * In next render call if this child is not recycled, it will be destroyed.
     */
    void AddChildById(const std::string& viewId, const JSRefPtr<JSView>& obj);

    void RemoveChildGroupById(const std::string& viewId);

    static void Create(const JSCallbackInfo& info);
    static void JSBind(BindingTarget globalObj);

    static void ConstructorCallback(const JSCallbackInfo& args);
    static void DestructorCallback(JSView* instance);

private:
    WeakPtr<OHOS::Ace::ComposedElement> GetElement()
    {
        return element_;
    }

    void DestroyChild(JSView* parentCustomView);

    /**
     * Takes care of the viewId wrt to foreach
     */
    std::string ProcessViewId(const std::string& viewId);
    /**
     * creates a set of valid viewids on a render function execution
     * its cleared after cleaning up the abandoned child.
     */
    void ChildAccessedById(const std::string& viewId);

    // view id for custom view itself
    std::string viewId_;

    WeakPtr<OHOS::Ace::ComposedElement> element_ = nullptr;
    bool needsUpdate_ = false;
    bool isStatic_ = false;

    RefPtr<ViewFunctions> jsViewFunction_;

    // hold handle to the native and javascript object to keep them alive
    // until they are abandoned
    std::unordered_map<std::string, JSRefPtr<JSView>> customViewChildren_;
    // a set of valid viewids on a renderfuntion excution
    // its cleared after cleaning up the abandoned child.
    std::unordered_set<std::string> lastAccessedViewIds_;
};

} // namespace OHOS::Ace::Framework
#endif // FRAMEWORKS_BRIDGE_DECLARATIVE_FRONTEND_JS_VIEW_JS_VIEW_H
