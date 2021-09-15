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

#include "frameworks/bridge/common/inspector/js_inspector_manager.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#include "core/components_v2/inspector/inspector_composed_element.h"
#include "core/components_v2/inspector/shape_composed_element.h"
#include "frameworks/bridge/common/inspector/inspect_badge.h"
#include "frameworks/bridge/common/inspector/inspect_button.h"
#include "frameworks/bridge/common/inspector/inspect_canvas.h"
#include "frameworks/bridge/common/inspector/inspect_chart.h"
#include "frameworks/bridge/common/inspector/inspect_dialog.h"
#include "frameworks/bridge/common/inspector/inspect_div.h"
#include "frameworks/bridge/common/inspector/inspect_divider.h"
#include "frameworks/bridge/common/inspector/inspect_form.h"
#include "frameworks/bridge/common/inspector/inspect_grid_column.h"
#include "frameworks/bridge/common/inspector/inspect_grid_container.h"
#include "frameworks/bridge/common/inspector/inspect_grid_row.h"
#include "frameworks/bridge/common/inspector/inspect_image.h"
#include "frameworks/bridge/common/inspector/inspect_image_animator.h"
#include "frameworks/bridge/common/inspector/inspect_input.h"
#include "frameworks/bridge/common/inspector/inspect_label.h"
#include "frameworks/bridge/common/inspector/inspect_list.h"
#include "frameworks/bridge/common/inspector/inspect_list_item.h"
#include "frameworks/bridge/common/inspector/inspect_list_item_group.h"
#include "frameworks/bridge/common/inspector/inspect_marquee.h"
#include "frameworks/bridge/common/inspector/inspect_menu.h"
#include "frameworks/bridge/common/inspector/inspect_option.h"
#include "frameworks/bridge/common/inspector/inspect_panel.h"
#include "frameworks/bridge/common/inspector/inspect_picker.h"
#include "frameworks/bridge/common/inspector/inspect_picker_view.h"
#include "frameworks/bridge/common/inspector/inspect_piece.h"
#include "frameworks/bridge/common/inspector/inspect_popup.h"
#include "frameworks/bridge/common/inspector/inspect_progress.h"
#include "frameworks/bridge/common/inspector/inspect_qrcode.h"
#include "frameworks/bridge/common/inspector/inspect_rating.h"
#include "frameworks/bridge/common/inspector/inspect_refresh.h"
#include "frameworks/bridge/common/inspector/inspect_search.h"
#include "frameworks/bridge/common/inspector/inspect_select.h"
#include "frameworks/bridge/common/inspector/inspect_slider.h"
#include "frameworks/bridge/common/inspector/inspect_span.h"
#include "frameworks/bridge/common/inspector/inspect_stack.h"
#include "frameworks/bridge/common/inspector/inspect_stepper.h"
#include "frameworks/bridge/common/inspector/inspect_stepper_item.h"
#include "frameworks/bridge/common/inspector/inspect_swiper.h"
#include "frameworks/bridge/common/inspector/inspect_switch.h"
#include "frameworks/bridge/common/inspector/inspect_tab_bar.h"
#include "frameworks/bridge/common/inspector/inspect_tab_content.h"
#include "frameworks/bridge/common/inspector/inspect_tabs.h"
#include "frameworks/bridge/common/inspector/inspect_text.h"
#include "frameworks/bridge/common/inspector/inspect_textarea.h"
#include "frameworks/bridge/common/inspector/inspect_toggle.h"
#include "frameworks/bridge/common/inspector/inspect_toolbar.h"
#include "frameworks/bridge/common/inspector/inspect_toolbar_item.h"
#include "frameworks/bridge/common/inspector/inspect_video.h"
#include "frameworks/bridge/common/inspector/inspector_client.h"

namespace OHOS::Ace::Framework {
namespace {

const char INSPECTOR_CURRENT_VERSION[] = "1.0";
const char INSPECTOR_DEVICE_TYPE[] = "deviceType";
const char INSPECTOR_DEFAULT_VALUE[] = "defaultValue";
const char INSPECTOR_TYPE[] = "$type";
const char INSPECTOR_ROOT[] = "root";
const char INSPECTOR_VERSION[] = "version";
const char INSPECTOR_WIDTH[] = "width";
const char INSPECTOR_HEIGHT[] = "height";
const char INSPECTOR_RESOLUTION[] = "$resolution";
const char INSPECTOR_CHILDREN[] = "$children";
const char INSPECTOR_ID[] = "$ID";
const char INSPECTOR_RECT[] = "$rect";
const char INSPECTOR_Z_INDEX[] = "$z-index";
const char INSPECTOR_ATTRS[] = "$attrs";
const char INSPECTOR_STYLES[] = "$styles";
const char INSPECTOR_INNER_DEBUGLINE[] = "debugLine";
const char INSPECTOR_DEBUGLINE[] = "$debugLine";

} // namespace


template<class T>
RefPtr<InspectNode> InspectNodeCreator(NodeId nodeId, const std::string& tag)
{
    return AceType::MakeRefPtr<T>(nodeId, tag);
}

void JsInspectorManager::InitializeCallback()
{
    auto assembleJSONTreeCallback = [weak = WeakClaim(this)](std::string &jsonTreeStr) {
        auto jsInspectorManager = weak.Upgrade();
        if (!jsInspectorManager) {
            return false;
        }
        jsInspectorManager->AssembleJSONTree(jsonTreeStr);
        return true;
    };
    InspectorClient::GetInstance().RegisterJSONTreeCallback(assembleJSONTreeCallback);
    auto assembleDefaultJSONTreeCallback = [weak = WeakClaim(this)](std::string &jsonTreeStr) {
        auto jsInspectorManager = weak.Upgrade();
        if (!jsInspectorManager) {
            return false;
        }
        jsInspectorManager->AssembleDefaultJSONTree(jsonTreeStr);
        return true;
    };
    InspectorClient::GetInstance().RegisterDefaultJSONTreeCallback(assembleDefaultJSONTreeCallback);
}

// assemble the JSON tree using all depth -1 nodes and root nodes.
void JsInspectorManager::AssembleJSONTree(std::string& jsonStr)
{
    auto jsonNode = JsonUtil::Create(true);
    auto jsonNodeArray = JsonUtil::CreateArray(true);
    GetNodeJSONStrMap();

    jsonNode->Put(INSPECTOR_TYPE, INSPECTOR_ROOT);
    auto context = GetPipelineContext().Upgrade();
    if (context) {
        float scale = context->GetViewScale();
        double rootHeight = context->GetRootHeight();
        double rootWidth = context->GetRootWidth();
        jsonNode->Put(INSPECTOR_WIDTH, std::to_string(rootWidth * scale).c_str());
        jsonNode->Put(INSPECTOR_HEIGHT, std::to_string(rootHeight * scale).c_str());
    }
    jsonNode->Put(INSPECTOR_RESOLUTION, std::to_string(SystemProperties::GetResolution()).c_str());
    auto firstDepthNodeVec = nodeJSONInfoMap_[1];
    for (auto nodeJSONInfo : firstDepthNodeVec) {
        auto nodeJSONValue = JsonUtil::ParseJsonString(nodeJSONInfo.second.c_str());
        jsonNodeArray->Put(nodeJSONValue);
    }
    jsonNode->Put(INSPECTOR_CHILDREN, jsonNodeArray);
    jsonStr = jsonNode->ToString();
}

std::string GetDeviceTypeStr(const DeviceType &deviceType)
{
    std::string deviceName = "";
    if (deviceType == DeviceType::TV) {
        deviceName = "TV";
    } else if (deviceType == DeviceType::WATCH) {
        deviceName = "Watch";
    } else if (deviceType == DeviceType::CAR) {
        deviceName = "Car";
    } else {
        deviceName = "Phone";
    }
    return deviceName;
}

// assemble the default attrs and styles for all components
void JsInspectorManager::AssembleDefaultJSONTree(std::string& jsonStr)
{
    auto jsonNode = JsonUtil::Create(true);
    std::string deviceName = GetDeviceTypeStr(SystemProperties::GetDeviceType());

    jsonNode->Put(INSPECTOR_VERSION, INSPECTOR_CURRENT_VERSION);
    jsonNode->Put(INSPECTOR_DEVICE_TYPE, deviceName.c_str());
    static const std::vector<std::string> tagNames = {
        DOM_NODE_TAG_BADGE, DOM_NODE_TAG_BUTTON, DOM_NODE_TAG_CANVAS,
        DOM_NODE_TAG_CHART, DOM_NODE_TAG_DIALOG, DOM_NODE_TAG_DIV, DOM_NODE_TAG_DIVIDER, DOM_NODE_TAG_FORM,
        DOM_NODE_TAG_GRID_COLUMN, DOM_NODE_TAG_GRID_CONTAINER, DOM_NODE_TAG_GRID_ROW, DOM_NODE_TAG_IMAGE,
        DOM_NODE_TAG_IMAGE_ANIMATOR, DOM_NODE_TAG_INPUT, DOM_NODE_TAG_LABEL, DOM_NODE_TAG_LIST, DOM_NODE_TAG_LIST_ITEM,
        DOM_NODE_TAG_LIST_ITEM_GROUP, DOM_NODE_TAG_MARQUEE, DOM_NODE_TAG_MENU, DOM_NODE_TAG_OPTION, DOM_NODE_TAG_PANEL,
        DOM_NODE_TAG_PICKER_DIALOG, DOM_NODE_TAG_PICKER_VIEW, DOM_NODE_TAG_PIECE, DOM_NODE_TAG_POPUP,
        DOM_NODE_TAG_PROGRESS, DOM_NODE_TAG_QRCODE, DOM_NODE_TAG_RATING, DOM_NODE_TAG_REFRESH, DOM_NODE_TAG_SEARCH,
        DOM_NODE_TAG_SELECT, DOM_NODE_TAG_SLIDER, DOM_NODE_TAG_SPAN, DOM_NODE_TAG_STACK, DOM_NODE_TAG_STEPPER,
        DOM_NODE_TAG_STEPPER_ITEM, DOM_NODE_TAG_SWIPER, DOM_NODE_TAG_SWITCH, DOM_NODE_TAG_TAB_BAR,
        DOM_NODE_TAG_TAB_CONTENT, DOM_NODE_TAG_TABS, DOM_NODE_TAG_TEXT, DOM_NODE_TAG_TEXTAREA, DOM_NODE_TAG_TOGGLE,
        DOM_NODE_TAG_TOOL_BAR, DOM_NODE_TAG_TOOL_BAR_ITEM, DOM_NODE_TAG_VIDEO
    };

    static const LinearMapNode<RefPtr<InspectNode>(*)(NodeId, const std::string&)> inspectNodeCreators[] = {
        { DOM_NODE_TAG_BADGE, &InspectNodeCreator<InspectBadge> },
        { DOM_NODE_TAG_BUTTON, &InspectNodeCreator<InspectButton> },
        { DOM_NODE_TAG_CANVAS, &InspectNodeCreator<InspectCanvas> },
        { DOM_NODE_TAG_CHART, &InspectNodeCreator<InspectChart> },
        { DOM_NODE_TAG_DIALOG, &InspectNodeCreator<InspectDialog> },
        { DOM_NODE_TAG_DIV, &InspectNodeCreator<InspectDiv> },
        { DOM_NODE_TAG_DIVIDER, &InspectNodeCreator<InspectDivider> },
        { DOM_NODE_TAG_FORM, &InspectNodeCreator<InspectForm> },
        { DOM_NODE_TAG_GRID_COLUMN, &InspectNodeCreator<InspectGridColumn> },
        { DOM_NODE_TAG_GRID_CONTAINER, &InspectNodeCreator<InspectGridContainer> },
        { DOM_NODE_TAG_GRID_ROW, &InspectNodeCreator<InspectGridRow> },
        { DOM_NODE_TAG_IMAGE, &InspectNodeCreator<InspectImage> },
        { DOM_NODE_TAG_IMAGE_ANIMATOR, &InspectNodeCreator<InspectImageAnimator> },
        { DOM_NODE_TAG_INPUT, &InspectNodeCreator<InspectInput> },
        { DOM_NODE_TAG_LABEL, &InspectNodeCreator<InspectLabel> },
        { DOM_NODE_TAG_LIST, &InspectNodeCreator<InspectList> },
        { DOM_NODE_TAG_LIST_ITEM, &InspectNodeCreator<InspectListItem> },
        { DOM_NODE_TAG_LIST_ITEM_GROUP, &InspectNodeCreator<InspectListItemGroup> },
        { DOM_NODE_TAG_MARQUEE, &InspectNodeCreator<InspectMarquee> },
        { DOM_NODE_TAG_MENU, &InspectNodeCreator<InspectMenu> },
        { DOM_NODE_TAG_OPTION, &InspectNodeCreator<InspectOption> },
        { DOM_NODE_TAG_PANEL, &InspectNodeCreator<InspectPanel> },
        { DOM_NODE_TAG_PICKER_DIALOG, &InspectNodeCreator<InspectPickerDialog> },
        { DOM_NODE_TAG_PICKER_VIEW, &InspectNodeCreator<InspectPickerView> },
        { DOM_NODE_TAG_PIECE, &InspectNodeCreator<InspectPiece> },
        { DOM_NODE_TAG_POPUP, &InspectNodeCreator<InspectPopup> },
        { DOM_NODE_TAG_PROGRESS, &InspectNodeCreator<InspectProgress> },
        { DOM_NODE_TAG_QRCODE, &InspectNodeCreator<InspectQRcode> },
        { DOM_NODE_TAG_RATING, &InspectNodeCreator<InspectRating> },
        { DOM_NODE_TAG_REFRESH, &InspectNodeCreator<InspectRefresh> },
        { DOM_NODE_TAG_SEARCH, &InspectNodeCreator<InspectSearch> },
        { DOM_NODE_TAG_SELECT, &InspectNodeCreator<InspectSelect> },
        { DOM_NODE_TAG_SLIDER, &InspectNodeCreator<InspectSlider> },
        { DOM_NODE_TAG_SPAN, &InspectNodeCreator<InspectSpan> },
        { DOM_NODE_TAG_STACK, &InspectNodeCreator<InspectStack> },
        { DOM_NODE_TAG_STEPPER, &InspectNodeCreator<InspectStepper> },
        { DOM_NODE_TAG_STEPPER_ITEM, &InspectNodeCreator<InspectStepperItem> },
        { DOM_NODE_TAG_SWIPER, &InspectNodeCreator<InspectSwiper> },
        { DOM_NODE_TAG_SWITCH, &InspectNodeCreator<InspectSwitch> },
        { DOM_NODE_TAG_TAB_BAR, &InspectNodeCreator<InspectTabBar> },
        { DOM_NODE_TAG_TAB_CONTENT, &InspectNodeCreator<InspectTabContent> },
        { DOM_NODE_TAG_TABS, &InspectNodeCreator<InspectTabs> },
        { DOM_NODE_TAG_TEXT, &InspectNodeCreator<InspectText> },
        { DOM_NODE_TAG_TEXTAREA, &InspectNodeCreator<InspectTextArea> },
        { DOM_NODE_TAG_TOGGLE, &InspectNodeCreator<InspectToggle> },
        { DOM_NODE_TAG_TOOL_BAR, &InspectNodeCreator<InspectToolbar> },
        { DOM_NODE_TAG_TOOL_BAR_ITEM, &InspectNodeCreator<InspectToolbarItem> },
        { DOM_NODE_TAG_VIDEO, &InspectNodeCreator<InspectVideo> },
    };
    auto jsonDefaultValue = JsonUtil::Create(true);
    for (std::size_t i = 0; i < tagNames.size(); i++) {
        auto jsonDefaultAttrs = JsonUtil::Create(true);
        NodeId nodeId = -1;
        auto tag = tagNames[i];
        RefPtr<InspectNode> inspectNode;
        int64_t creatorIndex = BinarySearchFindIndex(inspectNodeCreators, ArraySize(inspectNodeCreators), tag.c_str());
        if (creatorIndex >= 0) {
            inspectNode = inspectNodeCreators[creatorIndex].value(nodeId, tag);
        } else {
            LOGW("node type %{public}s is invalid", tag.c_str());
            return;
        }
        inspectNode->InitCommonStyles();
        inspectNode->PackAttrAndStyle();
        inspectNode->SetAllAttr(jsonDefaultAttrs, INSPECTOR_ATTRS);
        inspectNode->SetAllStyle(jsonDefaultAttrs, INSPECTOR_STYLES);
        jsonDefaultValue->Put(tag.c_str(), jsonDefaultAttrs);
    }
    jsonNode->Put(INSPECTOR_DEFAULT_VALUE, jsonDefaultValue);
    jsonStr = jsonNode->ToString();
}

void JsInspectorManager::GetNodeJSONStrMap()
{
    ClearContainer();
    DumpNodeTreeInfo(0, 0);

    if (depthNodeIdVec_.empty()) {
        LOGE("page is empty");
        return;
    }

    for (auto depthNodeId : depthNodeIdVec_) {
        depthNodeIdMap_[depthNodeId.first].push_back(depthNodeId.second);
    }

    auto maxItem =  std::max_element(depthNodeIdVec_.begin(), depthNodeIdVec_.end());
    for (int depth = maxItem->first; depth > 0; depth--) {
        auto depthNodeId = depthNodeIdMap_[depth];
        for (auto nodeId : depthNodeId) {
            auto node = GetAccessibilityNodeFromPage(nodeId);
            if (node == nullptr) {
                LOGE("GetAccessibilityNodeFromPage is null, nodeId: %{public}d", nodeId);
                continue;
            }
            auto jsonNode = JsonUtil::Create(true);
            auto jsonNodeArray = JsonUtil::CreateArray(true);
            jsonNode->Put(INSPECTOR_TYPE, node->GetTag().c_str());
            jsonNode->Put(INSPECTOR_ID, node->GetNodeId());
            jsonNode->Put(INSPECTOR_Z_INDEX, node->GetZIndex());
            if (GetVersion() == AccessibilityVersion::JS_VERSION) {
                jsonNode->Put(INSPECTOR_RECT, UpdateNodeRectStrInfo(node).c_str());
                GetAttrsAndStyles(jsonNode, node);
            } else {
                jsonNode->Put(INSPECTOR_RECT, UpdateNodeRectStrInfoV2(node).c_str());
                GetAttrsAndStylesV2(jsonNode, node);
            }
            if (node->GetChildList().size() > 0) {
                GetChildrenJSONArray(depth, node, jsonNodeArray);
                jsonNode->Put(INSPECTOR_CHILDREN, jsonNodeArray);
            }
            nodeJSONInfoMap_[depth].emplace_back(nodeId, jsonNode->ToString());
        }
    }
}

// get attrs and styles from AccessibilityNode to JsonValue
void JsInspectorManager::GetAttrsAndStyles(std::unique_ptr<JsonValue>& jsonNode, const RefPtr<AccessibilityNode>& node)
{
    auto attrJsonNode = JsonUtil::Create(true);
    for (auto attr : node->GetAttrs()) {
        // this attr is wrong in API5,will delete in API7
        if (attr.first.find("clickEffect") != std::string::npos) {
            attr.first = ConvertStrToPropertyType(attr.first);
        }
        attrJsonNode->Put(attr.first.c_str(), attr.second.c_str());
    }
    // change debugLine to $debugLine and move out of attrs
    std::string debugLine = attrJsonNode->GetString(INSPECTOR_INNER_DEBUGLINE);
    jsonNode->Put(INSPECTOR_DEBUGLINE, debugLine.c_str());
    attrJsonNode->Delete(INSPECTOR_INNER_DEBUGLINE);
    jsonNode->Put(INSPECTOR_ATTRS, attrJsonNode);

    auto styleJsonNode = JsonUtil::Create(true);
    for (auto style : node->GetStyles()) {
        styleJsonNode->Put(ConvertStrToPropertyType(style.first).c_str(), style.second.c_str());
    }
    jsonNode->Put(INSPECTOR_STYLES, styleJsonNode);
}

void JsInspectorManager::GetAttrsAndStylesV2(std::unique_ptr<JsonValue>& jsonNode,
                                             const RefPtr<AccessibilityNode>& node)
{
    auto attrJsonNode = JsonUtil::Create(true);
    auto weakComposedElement = GetComposedElementFromPage(node->GetNodeId());
    auto composedElement = weakComposedElement.Upgrade();
    if (!composedElement) {
        LOGE("return");
        return;
    }
    std::string debugLine = composedElement->GetDebugLine();
    jsonNode->Put(INSPECTOR_DEBUGLINE, debugLine.c_str());
    auto inspectorElement = AceType::DynamicCast<V2::InspectorComposedElement>(composedElement);
    if (inspectorElement) {
        auto jsonObject = inspectorElement->ToJsonObject();
        jsonNode->Put(INSPECTOR_ATTRS, jsonObject);
    }
    auto shapeComposedElement = AceType::DynamicCast<V2::ShapeComposedElement>(inspectorElement);
    if (shapeComposedElement) {
        int type = StringUtils::StringToInt(shapeComposedElement->GetShapeType());
        jsonNode->Replace(INSPECTOR_TYPE, SHAPE_TYPE_STRINGS[type]);
    }
}

// clear the memory occupied by each item in map and vector.
void JsInspectorManager::ClearContainer()
{
    std::vector<std::pair<int32_t, int32_t>>().swap(depthNodeIdVec_);
    std::unordered_map<int32_t, std::vector<std::pair<int32_t, std::string>>>().swap(nodeJSONInfoMap_);
    nodeJSONInfoMap_.clear();
    std::unordered_map<int32_t, std::vector<int32_t>>().swap(depthNodeIdMap_);
    depthNodeIdMap_.clear();
}

std::string JsInspectorManager::UpdateNodeRectStrInfo(const RefPtr<AccessibilityNode> node)
{
    PositionInfo positionInfo = {0, 0, 0, 0};
    if (node->GetTag() == DOM_NODE_TAG_SPAN) {
        positionInfo = {
                node->GetParentNode()->GetWidth(), node->GetParentNode()->GetHeight(),
                node->GetParentNode()->GetLeft(), node->GetParentNode()->GetTop()
        };
    } else {
        positionInfo = {node->GetWidth(), node->GetHeight(), node->GetLeft(), node->GetTop()};
    }
    if (!node->GetVisible()) {
        positionInfo = {0, 0, 0, 0};
    }
    // the dialog node is hidden, while the position and size of the node are not cleared.
    if (node->GetClearRectInfoFlag() == true) {
        positionInfo = {0, 0, 0, 0};
    }
    std::string strRec = std::to_string(positionInfo.left).append(",").
            append(std::to_string(positionInfo.top)).
            append(",").append(std::to_string(positionInfo.width)).
            append(",").append(std::to_string(positionInfo.height));
    return strRec;
}

std::string JsInspectorManager::UpdateNodeRectStrInfoV2(const RefPtr<AccessibilityNode> node)
{
    std::string strRec;
    auto weakComposedElement = GetComposedElementFromPage(node->GetNodeId());
    auto composedElement = weakComposedElement.Upgrade();
    if (!composedElement) {
        return strRec;
    }
    auto inspectorElement = AceType::DynamicCast<V2::InspectorComposedElement>(composedElement);
    if (inspectorElement) {
        strRec = inspectorElement->GetRect();
        return strRec;
    }
    return strRec;
}

void JsInspectorManager::DumpNodeTreeInfo(int32_t depth, NodeId nodeID)
{
    auto node = GetAccessibilityNodeFromPage(nodeID);
    if (!node) {
        LOGE("JsInspectorManager::DumpNodeTreeInfo return");
        return;
    }

    // get the vector of nodeID per depth
    depthNodeIdVec_.emplace_back(depth, nodeID);
    for (const auto& item : node->GetChildList()) {
        DumpNodeTreeInfo(depth + 1, item->GetNodeId());
    }
}

// find children of the current node and combine them with this node to form a JSON array object.
void JsInspectorManager::GetChildrenJSONArray(
    int32_t depth, RefPtr<AccessibilityNode> node, std::unique_ptr<JsonValue>& childJSONArray)
{
    auto  childNodeJSONVec = nodeJSONInfoMap_[depth + 1];
    for (auto item : node->GetChildList()) {
        auto childVec = std::find_if(std::begin(childNodeJSONVec), std::end(childNodeJSONVec),
            [item](const std::pair<int32_t, std::string>& upper) {
                return upper.first == item->GetNodeId();
            });
        if (childVec != std::end(childNodeJSONVec)) {
            auto childJSONValue = JsonUtil::ParseJsonString(childVec->second.c_str());
            childJSONArray->Put(childJSONValue);
        }
    }
}

std::string JsInspectorManager::ConvertStrToPropertyType(const std::string& typeValue)
{
    std::string dstStr;
    std::regex regex("([A-Z])");
    dstStr = regex_replace(typeValue, regex, "-$1");
    std::transform(dstStr.begin(), dstStr.end(), dstStr.begin(), ::tolower);
    return dstStr;
}
} // namespace OHOS::Ace::Framework
