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

#include <unordered_map>

#include "core/components_v2/inspector/badge_composed_element.h"

#include "base/log/dump_log.h"

namespace OHOS::Ace::V2 {

const std::unordered_map<std::string, std::function<std::string(const BadgeComposedElement&)>> CREATE_JSON_MAP {
    { "count", [](const BadgeComposedElement& inspector) { return inspector.GetCount(); } },
    { "position", [](const BadgeComposedElement& inspector) { return inspector.GetMaxCount(); } },
    { "maxCount", [](const BadgeComposedElement& inspector) { return inspector.GetBadgePosition(); } },
    { "value", [](const BadgeComposedElement& inspector) { return inspector.GetLabel(); } }
};

void BadgeComposedElement::Dump()
{
    InspectorComposedElement::Dump();
    DumpLog::GetInstance().AddDesc(std::string("badge_composed_element"));
    DumpLog::GetInstance().AddDesc(
        std::string("count: ").append(GetCount()));
    DumpLog::GetInstance().AddDesc(
        std::string("position: ").append(GetBadgePosition()));
    DumpLog::GetInstance().AddDesc(
        std::string("maxCount: ").append(GetMaxCount()));
    DumpLog::GetInstance().AddDesc(
        std::string("value: ").append(GetLabel()));
}

std::unique_ptr<OHOS::Ace::JsonValue> BadgeComposedElement::ToJsonObject() const
{
    auto resultJson = InspectorComposedElement::ToJsonObject();
    for (const auto& value : CREATE_JSON_MAP) {
        resultJson->Put(value.first.c_str(), value.second(*this).c_str());
    }
    return resultJson;
}

std::string BadgeComposedElement::GetCount(void) const
{
    auto renderBadge = GetRenderBadge();
    int64_t count = renderBadge ? renderBadge->GetBadgeComponent()->GetMessageCount() : 0;
    return std::to_string(count);
}

std::string BadgeComposedElement::GetMaxCount(void) const
{
    auto renderBadge = GetRenderBadge();
    int64_t maxCount = renderBadge ? renderBadge->GetBadgeComponent()->GetMaxCount() : 99;
    return std::to_string(maxCount);
}

std::string BadgeComposedElement::GetBadgePosition(void) const
{
    auto renderBadge = GetRenderBadge();
    int64_t type =  renderBadge ? static_cast<int64_t>(renderBadge->GetBadgeComponent()->GetBadgePosition()) :
        static_cast<int64_t>(BadgePosition::RIGHT_TOP);
    return std::to_string(type);
}

std::string BadgeComposedElement::GetLabel(void) const
{
    auto renderBadge = GetRenderBadge();
    std::string label =  renderBadge ? renderBadge->GetBadgeComponent()->GetBadgeLabel() : "";
    return label;
}

OHOS::Ace::RefPtr<OHOS::Ace::RenderBadge> BadgeComposedElement::GetRenderBadge() const
{
    auto node = GetInspectorNode(BadgeElement::TypeId());
    if (node) {
        return AceType::DynamicCast<RenderBadge>(node);
    }
    return nullptr;
}

}