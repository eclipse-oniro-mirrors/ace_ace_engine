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

#include "core/components/tab_bar/tabs_component.h"

#include "core/components/clip/clip_component.h"
#include "core/components/tab_bar/tabs_element.h"

namespace OHOS::Ace {
namespace {

int32_t g_tabControllerId = 0;

} // namespace

TabsComponent::TabsComponent(
    std::list<RefPtr<Component>>& children, BarPosition barPosition, const RefPtr<TabController>& controller)
    : FlexComponent(FlexDirection::COLUMN, FlexAlign::FLEX_START, FlexAlign::CENTER, children)
{
    if (!controller) {
        controller_ = TabController::GetController(++g_tabControllerId);
    } else {
        controller_ = controller;
    }
    tabBarIndicator_ = AceType::MakeRefPtr<TabBarIndicatorComponent>();
    tabBar_ = AceType::MakeRefPtr<TabBarComponent>(tabBarChildren_, controller_, tabBarIndicator_);
    tabBar_->SetBarPosition(barPosition);
    tabContent_ = AceType::MakeRefPtr<TabContentComponent>(tabContentChildren_, controller_);
    flexItem_ = AceType::MakeRefPtr<FlexItemComponent>(0, 1, 0);
    flexItem_->SetChild(tabContent_);
    auto box = AceType::MakeRefPtr<BoxComponent>();
    box->SetChild(tabBar_);
    if (barPosition == BarPosition::END) {
        AppendChildDirectly(flexItem_);
        AppendChildDirectly(box);
    } else {
        AppendChildDirectly(box);
        AppendChildDirectly(flexItem_);
    }
}

RefPtr<Element> TabsComponent::CreateElement()
{
    return AceType::MakeRefPtr<TabsElement>();
}

void TabsComponent::AppendChild(const RefPtr<Component>& child)
{
    auto clip = AceType::MakeRefPtr<ClipComponent>(child);
    clip->SetFollowChild(false);
    tabContent_->AppendChildDirectly(clip);
}

void TabsComponent::RemoveChild(const RefPtr<Component>& child)
{
    auto clip = AceType::DynamicCast<ClipComponent>(child);
    tabContent_->RemoveChildDirectly(clip);
}

} // namespace OHOS::Ace
