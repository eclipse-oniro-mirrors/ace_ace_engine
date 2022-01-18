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

#include "frameworks/bridge/declarative_frontend/jsview/js_tab_content.h"

#include "base/log/ace_trace.h"
#include "frameworks/bridge/declarative_frontend/view_stack_processor.h"
#include "frameworks/core/components/padding/padding_component.h"

namespace OHOS::Ace::Framework {
namespace {

constexpr Dimension DEFAULT_SMALL_TEXT_FONT_SIZE = 10.0_fp;
constexpr Dimension DEFAULT_SMALL_IMAGE_WIDTH = 24.0_vp;
constexpr Dimension DEFAULT_SMALL_IMAGE_HEIGHT = 26.0_vp;
constexpr Dimension DEFAULT_SINGLE_TEXT_FONT_SIZE = 16.0_fp;
constexpr char DEFAULT_TAB_BAR_NAME[] = "TabBar";

} // namespace

void JSTabContent::Create()
{
    auto tabsComponent = AceType::DynamicCast<TabsComponent>(ViewStackProcessor::GetInstance()->GetMainComponent());
    if (tabsComponent) {
        auto tabBar = tabsComponent->GetTabBarChild();
        std::list<RefPtr<Component>> components;
        auto tabContentItemComponent = AceType::MakeRefPtr<TabContentItemComponent>(components);
        tabContentItemComponent->SetCrossAxisSize(CrossAxisSize::MAX);
        tabContentItemComponent->SetTabsComponent(AceType::WeakClaim(AceType::RawPtr(tabsComponent)));
        tabBar->AppendChild(CreateTabBarLabelComponent(tabBar, std::string(DEFAULT_TAB_BAR_NAME)));
        ViewStackProcessor::GetInstance()->Push(tabContentItemComponent);
    }
}

void JSTabContent::SetTabBar(const JSCallbackInfo& info)
{
    auto tabContentItemComponent =
        AceType::DynamicCast<OHOS::Ace::TabContentItemComponent>(ViewStackProcessor::GetInstance()->GetMainComponent());
    if (!tabContentItemComponent) {
        return;
    }
    auto weakTabs = tabContentItemComponent->GetTabsComponent();
    RefPtr<TabsComponent> tabs = weakTabs.Upgrade();
    if (!tabs) {
        LOGE("can not get Tabs parent component error.");
        return;
    }
    RefPtr<TabBarComponent> tabBar = tabs->GetTabBarChild();
    if (!tabBar) {
        LOGE("can not get TabBar component error.");
        return;
    }
    RefPtr<Component> tabBarChild = nullptr;
    std::string infoStr;
    if (ParseJsString(info[0], infoStr)) {
        std::string textVal = infoStr.empty() ? DEFAULT_TAB_BAR_NAME : infoStr;
        auto text = AceType::MakeRefPtr<TextComponent>(textVal);
        auto textStyle = text->GetTextStyle();
        textStyle.SetFontSize(DEFAULT_SINGLE_TEXT_FONT_SIZE);
        text->SetTextStyle(textStyle);
        tabContentItemComponent->SetBarText(textVal);
        auto defaultTabChild = tabBar->GetChildren().back();
        tabBar->RemoveChildDirectly(defaultTabChild);
        tabBar->AppendChild(text);
        return;
    }
    auto paramObject = JSRef<JSObject>::Cast(info[0]);
    JSRef<JSVal> builderFuncParam = paramObject->GetProperty("builder");
    JSRef<JSVal> textParam = paramObject->GetProperty("text");
    JSRef<JSVal> iconParam = paramObject->GetProperty("icon");
    if (builderFuncParam->IsFunction()) {
        tabBarChild = ProcessTabBarBuilderFunction(tabBar, builderFuncParam);
    } else if ((!textParam->IsEmpty()) && (!iconParam->IsEmpty())) {
        tabBarChild = ProcessTabBarTextIconPair(tabBar, textParam, iconParam);
    } else if (textParam->IsEmpty() && (!iconParam->IsEmpty())) {
        tabBarChild = ProcessTabBarLabel(tabBar, textParam);
    } else {
        LOGE("invalid parameters: expecting either builder func, text & icon pair, or label");
        return;
    }
    auto defaultTabChild = tabBar->GetChildren().back();
    ACE_DCHECK(tabBarChild != nullptr);
    ACE_DCHECK(defaultTabChild != nullptr);
    tabBar->RemoveChildDirectly(defaultTabChild);
    tabBar->AppendChild(tabBarChild);
}

RefPtr<Component> JSTabContent::ProcessTabBarBuilderFunction(
    RefPtr<TabBarComponent>& tabBar, const JSRef<JSObject> builderFunc)
{
    ScopedViewStackProcessor builderViewStackProcessor;
    JsFunction jsBuilderFunc(builderFunc);
    ACE_SCORING_EVENT("TabContent.tabBarBuilder");
    jsBuilderFunc.Execute();
    RefPtr<Component> builderGeneratedRootComponent = ViewStackProcessor::GetInstance()->Finish();
    return builderGeneratedRootComponent;
}

RefPtr<TextComponent> JSTabContent::CreateTabBarLabelComponent(
    RefPtr<TabBarComponent>& tabBar, const std::string& labelStr)
{
    auto text = AceType::MakeRefPtr<TextComponent>(labelStr);
    auto textStyle = text->GetTextStyle();
    textStyle.SetFontSize(DEFAULT_SINGLE_TEXT_FONT_SIZE);
    text->SetTextStyle(textStyle);
    return text;
}

RefPtr<TextComponent> JSTabContent::ProcessTabBarLabel(RefPtr<TabBarComponent>& tabBar, JSRef<JSVal> labelVal)
{
    std::string textStr;
    if (!ParseJsString(labelVal, textStr)) {
        textStr = DEFAULT_TAB_BAR_NAME;
    }
    LOGD("text: %s", textStr.c_str());
    return CreateTabBarLabelComponent(tabBar, textStr);
}

RefPtr<Component> JSTabContent::ProcessTabBarTextIconPair(
    RefPtr<TabBarComponent>& tabBar, JSRef<JSVal> textVal, JSRef<JSVal> iconVal)
{
    std::string iconUri;
    if (!ParseJsMedia(iconVal, iconUri)) {
        return ProcessTabBarLabel(tabBar, textVal);
    }
    std::string textStr;
    if (!ParseJsString(textVal, textStr)) {
        textStr = DEFAULT_TAB_BAR_NAME;
    }
    auto imageComponent = AceType::MakeRefPtr<ImageComponent>(iconUri);
    auto box = AceType::MakeRefPtr<BoxComponent>();
    auto padding = AceType::MakeRefPtr<PaddingComponent>();
    padding->SetPadding(Edge(0, 0, 0, 2, DimensionUnit::VP));
    padding->SetChild(imageComponent);
    box->SetChild(padding);
    box->SetWidth(DEFAULT_SMALL_IMAGE_WIDTH);
    box->SetHeight(DEFAULT_SMALL_IMAGE_HEIGHT);
    auto textComponent = AceType::MakeRefPtr<TextComponent>(textStr);
    auto textStyle = textComponent->GetTextStyle();
    textStyle.SetFontSize(DEFAULT_SMALL_TEXT_FONT_SIZE);
    textComponent->SetTextStyle(textStyle);
    std::list<RefPtr<Component>> children;
    children.emplace_back(box);
    children.emplace_back(textComponent);
    auto columnComponent = AceType::MakeRefPtr<ColumnComponent>(FlexAlign::FLEX_START, FlexAlign::CENTER, children);
    columnComponent->SetMainAxisSize(MainAxisSize::MIN);
    return columnComponent;
}

void JSTabContent::JSBind(BindingTarget globalObj)
{
    JSClass<JSTabContent>::Declare("TabContent");
    MethodOptions opt = MethodOptions::NONE;
    JSClass<JSTabContent>::StaticMethod("create", &JSTabContent::Create, opt);
    JSClass<JSTabContent>::StaticMethod("tabBar", &JSTabContent::SetTabBar, opt);
    JSClass<JSTabContent>::StaticMethod("onAppear", &JSInteractableView::JsOnAppear);
    JSClass<JSTabContent>::StaticMethod("onDisAppear", &JSInteractableView::JsOnDisAppear);
    JSClass<JSTabContent>::StaticMethod("onTouch", &JSInteractableView::JsOnTouch);
    JSClass<JSTabContent>::StaticMethod("onHover", &JSInteractableView::JsOnHover);
    JSClass<JSTabContent>::StaticMethod("onKeyEvent", &JSInteractableView::JsOnKey);
    JSClass<JSTabContent>::StaticMethod("onDeleteEvent", &JSInteractableView::JsOnDelete);
    JSClass<JSTabContent>::StaticMethod("onClick", &JSInteractableView::JsOnClick);
    JSClass<JSTabContent>::StaticMethod("width", &JSTabContent::SetTabContentWidth, opt);
    JSClass<JSTabContent>::StaticMethod("height", &JSTabContent::SetTabContentHeight, opt);
    JSClass<JSTabContent>::StaticMethod("size", &JSTabContent::SetTabContentSize, opt);
    JSClass<JSTabContent>::Inherit<JSContainerBase>();
    JSClass<JSTabContent>::Bind<>(globalObj);
}

} // namespace OHOS::Ace::Framework
