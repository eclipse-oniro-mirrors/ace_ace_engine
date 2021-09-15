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

#include "core/components_v2/inspector/button_composed_element.h"

#include "base/log/dump_log.h"
#include "core/components/button/button_element.h"
#include "core/components/common/layout/constants.h"
#include "core/components_v2/inspector/utils.h"

namespace OHOS::Ace::V2 {

const std::unordered_map<std::string, std::function<std::string(const ButtonComposedElement&)>> CREATE_JSON_MAP {
    { "BottonType", [](const ButtonComposedElement& inspector) { return inspector.GetBottonType(); } },
    { "StateEffect", [](const ButtonComposedElement& inspector) { return inspector.GetStateEffect(); } },
};

void ButtonComposedElement::Dump()
{
    InspectorComposedElement::Dump();
    DumpLog::GetInstance().AddDesc(std::string("button_composed_element"));
    DumpLog::GetInstance().AddDesc(
        std::string("BottonType: ").append(GetBottonType()));
    DumpLog::GetInstance().AddDesc(
        std::string("StateEffect: ").append(GetStateEffect()));
}

std::unique_ptr<JsonValue> ButtonComposedElement::ToJsonObject() const
{
    auto resultJson = InspectorComposedElement::ToJsonObject();
    for (const auto& value : CREATE_JSON_MAP) {
        resultJson->Put(value.first.c_str(), value.second(*this).c_str());
    }
    return resultJson;
}

std::string ButtonComposedElement::GetBottonType() const
{
    auto renderButton = GetRenderButton();
    auto type = renderButton ? renderButton->GetButtonType() : ButtonType::CAPSULE;
    return ConvertButtonTypeToString(type);
}

std::string ButtonComposedElement::GetStateEffect() const
{
    auto renderButton = GetRenderButton();
    auto stateEffect = renderButton ? renderButton->GetStateEffect() : true;
    return ConvertBoolToString(stateEffect);
}

RefPtr<RenderButton> ButtonComposedElement::GetRenderButton() const
{
    auto node = GetInspectorNode(ButtonElement::TypeId());
    if (node) {
        return AceType::DynamicCast<RenderButton>(node);
    }
    return nullptr;
}

std::string ButtonComposedElement::ConvertButtonTypeToString(ButtonType buttonType) const
{
    std::string result = "";
    switch (buttonType) {
        case ButtonType::NORMAL:
            result = "ButtonType.Normal";
            break;
        case ButtonType::CAPSULE:
            result = "ButtonType.Capsule";
            break;
        case ButtonType::CIRCLE:
            result = "ButtonType.Circle";
            break;
        case ButtonType::ARC:
            result = "ButtonType.Arc";
            break;
        default:
            LOGD("input do not match any ButtonType");
    }
    return result;
}

} // namespace OHOS::Ace::V2