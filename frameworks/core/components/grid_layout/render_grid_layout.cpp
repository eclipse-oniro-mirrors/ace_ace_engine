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

#include "core/components/grid_layout/render_grid_layout.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <regex>

#include "base/log/event_report.h"
#include "base/log/log.h"
#include "base/utils/string_utils.h"
#include "base/utils/utils.h"
#include "core/animation/curve_animation.h"
#include "core/components/grid_layout/grid_layout_component.h"
#include "core/components/grid_layout/render_grid_layout_item.h"
#include "core/event/ace_event_helper.h"
#include "core/gestures/long_press_recognizer.h"
#include "core/gestures/pan_recognizer.h"
#include "core/gestures/sequenced_recognizer.h"

namespace OHOS::Ace {
namespace {

constexpr int32_t DEFAULT_DEPTH = 10;
constexpr bool HORIZONTAL = false;
constexpr bool VERTICAL = true;
constexpr bool FORWARD = false;
constexpr bool REVERSE = true;
constexpr double FULL_PERCENT = 100.0;
constexpr uint32_t REPEAT_MIN_SIZE = 6;
const char UNIT_PIXEL[] = "px";
const char UNIT_PERCENT[] = "%";
const char UNIT_RATIO[] = "fr";
const char UNIT_AUTO[] = "auto";
const char UNIT_AUTO_FILL[] = "auto-fill";
const char REPEAT_PREFIX[] = "repeat";
const std::regex REPEAT_NUM_REGEX(R"(^repeat\((\d+),(.+)\))", std::regex::icase); // regex for "repeat(2, 100px)"
const std::regex AUTO_REGEX(R"(^repeat\((.+),(.+)\))", std::regex::icase);        // regex for "repeat(auto-fill, 10px)"

// first bool mean if vertical, second bool mean if reverse
// false, false --> RIGHT
// false, true --> LEFT
// true, false --> DOWN
// true, true ---> UP
// This map will adapter the Grid FlexDirection with Key Direction.
const std::map<bool, std::map<bool, std::map<bool, KeyDirection>>> DIRECTION_MAP = {
    { false, // RTL is false
        { { HORIZONTAL, { { FORWARD, KeyDirection::RIGHT }, { REVERSE, KeyDirection::LEFT } } },
            { VERTICAL, { { FORWARD, KeyDirection::DOWN }, { REVERSE, KeyDirection::UP } } } } },
    { true, // RTL is true
        { { HORIZONTAL, { { FORWARD, KeyDirection::LEFT }, { REVERSE, KeyDirection::RIGHT } } },
            { VERTICAL, { { FORWARD, KeyDirection::DOWN }, { REVERSE, KeyDirection::UP } } } } }
};

} // namespace

void RenderGridLayout::OnChildAdded(const RefPtr<RenderNode>& renderNode)
{
    RenderNode::OnChildAdded(renderNode);
}

void RenderGridLayout::Update(const RefPtr<Component>& component)
{
    const RefPtr<GridLayoutComponent> grid = AceType::DynamicCast<GridLayoutComponent>(component);
    if (!grid) {
        LOGE("RenderGridLayout update failed.");
        EventReport::SendRenderException(RenderExcepType::RENDER_COMPONENT_ERR);
        return;
    }

    updateFlag_ = true;
    direction_ = grid->GetDirection();
    crossAxisAlign_ = grid->GetFlexAlign();
    gridWidth_ = grid->GetWidth();
    gridHeight_ = grid->GetHeight();
    colsArgs_ = grid->GetColumnsArgs();
    rowsArgs_ = grid->GetRowsArgs();
    userColGap_ = grid->GetColumnGap();
    userRowGap_ = grid->GetRowGap();
    rightToLeft_ = grid->GetRightToLeft();
    scrollBarWidth_ = grid->GetScrollBarWidth();
    scrollBarColor_ = grid->GetScrollBarColor();
    displayMode_ = grid->GetScrollBar();

    // update other new prop
    cellLength_ = grid->GetCellLength();
    mainCountMax_ = grid->GetMaxCount();
    mainCountMin_ = grid->GetMinCount();
    editMode_ = grid->GetEditMode();

    onGridDragStartFunc_ = grid->GetOnGridDragStartId();
    OnGridDragEnterFunc_ = grid->GetOnGridDragEnterId();
    onGridDragMoveFunc_ = grid->GetOnGridDragMoveId();
    onGridDragLeaveFunc_ = grid->GetOnGridDragLeaveId();
    onGridDropFunc_ = grid->GetOnGridDropId();

    if (((rowsArgs_.empty() && (!colsArgs_.empty())) || ((!rowsArgs_.empty()) && colsArgs_.empty())) &&
        (mainCountMax_ >= mainCountMin_) && (mainCountMin_ >= 1) && (cellLength_ > 0) && (editMode_ == true)) {
        isDynamicGrid_ = true;
    }
    LOGD("editMode: %{public}d, isDynamicGrid: %{public}d, cellLength: %{public}d, mainCountMin: %{public}d, "
         "mainCountMax: %{public}d .",
        editMode_, isDynamicGrid_, cellLength_, mainCountMin_, mainCountMax_);

    CalIsVertical();

    component_ = grid;

    if (editMode_ && grid->GetOnGridDropId()) {
        CreateDragDropRecognizer();
    }

    MarkNeedLayout();
}

void RenderGridLayout::UpdateFocusInfo(int32_t focusIndex)
{
    if (focusIndex < 0) {
        LOGW("Invalid focus index, update focus info failed.");
        return;
    }
    if (focusIndex != focusIndex_) {
        LOGD("Update focus index from %{public}d to %{public}d", focusIndex_, focusIndex);
        focusIndex_ = focusIndex;
        for (const auto& gridMap : gridMatrix_) {
            for (const auto& grid : gridMap.second) {
                if (grid.second == focusIndex) {
                    focusRow_ = gridMap.first;
                    focusCol_ = grid.first;
                }
            }
        }
    }
}

int32_t RenderGridLayout::RequestNextFocus(bool vertical, bool reverse)
{
    KeyDirection key = DIRECTION_MAP.at(rightToLeft_).at(vertical).at(reverse);
    int32_t index = focusMove(key);
    if (index < 0) {
        return index;
    }
    return focusIndex_;
}

// Handle direction key move
int32_t RenderGridLayout::focusMove(KeyDirection direction)
{
    int32_t nextRow = focusRow_;
    int32_t nextCol = focusCol_;
    int32_t next = focusIndex_;
    while (focusIndex_ == next || next < 0) {
        switch (direction) {
            case KeyDirection::UP:
                --nextRow;
                break;
            case KeyDirection::DOWN:
                ++nextRow;
                break;
            case KeyDirection::LEFT:
                --nextCol;
                break;
            case KeyDirection::RIGHT:
                ++nextCol;
                break;
            default:
                return -1;
        }
        if (nextRow < 0 || nextCol < 0 || nextRow >= rowCount_ || nextCol >= colCount_) {
            return -1;
        }
        next = GetIndexByGrid(nextRow, nextCol);
    }
    LOGI("PreFocus:%{public}d CurrentFocus:%{public}d", focusIndex_, next);
    focusRow_ = nextRow;
    focusCol_ = nextCol;
    focusIndex_ = next;
    return next;
}

int32_t RenderGridLayout::GetIndexByGrid(int32_t row, int32_t column) const
{
    LOGD("%{public}s begin. row: %{public}d, column: %{public}d", __PRETTY_FUNCTION__, row, column);
    auto rowIter = gridMatrix_.find(row);
    if (rowIter != gridMatrix_.end()) {
        auto colIter = rowIter->second.find(column);
        if (colIter != rowIter->second.end()) {
            return colIter->second;
        }
    }
    return -1;
}

LayoutParam RenderGridLayout::MakeInnerLayoutParam(int32_t row, int32_t col, int32_t rowSpan, int32_t colSpan) const
{
    LOGD("%{public}d %{public}d %{public}d %{public}d", row, col, rowSpan, colSpan);
    LayoutParam innerLayout;
    double rowLen = 0.0;
    double colLen = 0.0;
    for (int32_t i = 0; i < rowSpan; ++i) {
        rowLen += gridCells_.at(row + i).at(col).Height();
    }
    rowLen += (rowSpan - 1) * rowGap_;
    for (int32_t i = 0; i < colSpan; ++i) {
        colLen += gridCells_.at(row).at(col + i).Width();
    }
    colLen += (colSpan - 1) * colGap_;
    if (crossAxisAlign_ == FlexAlign::STRETCH) {
        innerLayout.SetMinSize(Size(colLen, rowLen));
        innerLayout.SetMaxSize(Size(colLen, rowLen));
    } else {
        innerLayout.SetMaxSize(Size(colLen, rowLen));
    }
    return innerLayout;
}

void RenderGridLayout::SetChildPosition(
    const RefPtr<RenderNode>& child, int32_t row, int32_t col, int32_t rowSpan, int32_t colSpan)
{
    LOGD("%{public}s begin. row: %{public}d, col: %{public}d, rowSpan: %{public}d, colSpan: %{public}d",
        __PRETTY_FUNCTION__, row, col, rowSpan, colSpan);
    if (focusRow_ < 0 && focusCol_ < 0) {
        // Make the first item obtain focus.
        focusRow_ = row;
        focusCol_ = col;
    }

    // Calculate the position for current child.
    double positionX = 0.0;
    double positionY = 0.0;
    for (int32_t i = 0; i < row; ++i) {
        positionY += gridCells_.at(i).at(0).Height();
    }
    positionY += row * rowGap_;
    for (int32_t i = 0; i < col; ++i) {
        positionX += gridCells_.at(0).at(i).Width();
    }
    positionX += col * colGap_;

    // Calculate the size for current child.
    double rowLen = 0.0;
    double colLen = 0.0;
    for (int32_t i = 0; i < rowSpan; ++i) {
        rowLen += gridCells_.at(row + i).at(col).Height();
    }
    rowLen += (rowSpan - 1) * rowGap_;
    for (int32_t i = 0; i < colSpan; ++i) {
        colLen += gridCells_.at(row).at(col + i).Width();
    }
    colLen += (colSpan - 1) * colGap_;

    // If RTL, place the item from right.
    if (rightToLeft_) {
        positionX = colSize_ - positionX - colLen;
    }

    double widthOffset = (colLen - child->GetLayoutSize().Width()) / 2.0;
    double heightOffset = (rowLen - child->GetLayoutSize().Height()) / 2.0;
    child->SetPosition(Offset(positionX + widthOffset, positionY + heightOffset));
}

void RenderGridLayout::DisableChild(const RefPtr<RenderNode>& child, int32_t index)
{
    LOGD("No grid space for this item[%{public}d]", index);
    LayoutParam zeroLayout;
    zeroLayout.SetMinSize(Size(0.0, 0.0));
    zeroLayout.SetMaxSize(Size(0.0, 0.0));
    child->Layout(zeroLayout);
    child->SetPosition(Offset(0.0, 0.0));
}

Size RenderGridLayout::GetTargetLayoutSize(int32_t row, int32_t col)
{
    Size size;
    if (GetChildren().empty()) {
        return size;
    }
    LayoutParam innerLayout; // Init layout param for auto item.
    innerLayout.SetMaxSize(Size(colSize_, rowSize_));
    std::vector<std::string> rows, cols;
    StringUtils::StringSpliter(rowsArgs_, ' ', rows);
    rowCount_ = rows.size() == 0 ? 1 : rows.size();
    StringUtils::StringSpliter(colsArgs_, ' ', cols);
    colCount_ = cols.size() == 0 ? 1 : cols.size();
    int32_t rowIndex = 0;
    int32_t colIndex = 0;
    int32_t itemIndex = 0;
    for (const auto& item : GetChildren()) {
        int32_t itemRow = GetItemRowIndex(item);
        int32_t itemCol = GetItemColumnIndex(item);
        int32_t itemRowSpan = GetItemSpan(item, true);
        int32_t itemColSpan = GetItemSpan(item, false);
        if (itemRow >= 0 && itemRow < rowCount_ && itemCol >= 0 && itemCol < colCount_ &&
            CheckGridPlaced(itemIndex, itemRow, itemCol, itemRowSpan, itemColSpan)) {
            if (itemRow == row && itemCol == col) {
                item->Layout(innerLayout);
                return item->GetLayoutSize();
            }
        } else {
            while (!CheckGridPlaced(itemIndex, rowIndex, colIndex, itemRowSpan, itemColSpan)) {
                GetNextGrid(rowIndex, colIndex);
                if (rowIndex >= rowCount_ || colIndex >= colCount_) {
                    break;
                }
            }
            if (rowIndex == row && colIndex == col) {
                item->Layout(innerLayout);
                return item->GetLayoutSize();
            }
        }
        ++itemIndex;
    }
    return size;
}

std::string RenderGridLayout::PreParseRows()
{
    if (rowsArgs_.empty() || rowsArgs_.find(UNIT_AUTO) == std::string::npos) {
        return rowsArgs_;
    }
    std::string rowsArgs;
    std::vector<std::string> strs;
    StringUtils::StringSpliter(rowsArgs_, ' ', strs);
    std::string current;
    int32_t rowArgSize = strs.size();
    for (int32_t i = 0; i < rowArgSize; ++i) {
        current = strs[i];
        if (strs[i] == std::string(UNIT_AUTO)) {
            Size layoutSize = GetTargetLayoutSize(i, 0);
            current = StringUtils::DoubleToString(layoutSize.Height()) + std::string(UNIT_PIXEL);
            gridMatrix_.clear();
        }
        rowsArgs += ' ' + current;
    }
    return rowsArgs;
}

std::string RenderGridLayout::PreParseCols()
{
    if (colsArgs_.empty() || colsArgs_.find(UNIT_AUTO) == std::string::npos) {
        return colsArgs_;
    }
    std::string colsArgs;
    std::vector<std::string> strs;
    StringUtils::StringSpliter(colsArgs_, ' ', strs);
    std::string current;
    int32_t colArgSize = strs.size();
    for (int32_t i = 0; i < colArgSize; ++i) {
        current = strs[i];
        if (strs[i] == std::string(UNIT_AUTO)) {
            Size layoutSize = GetTargetLayoutSize(0, i);
            current = StringUtils::DoubleToString(layoutSize.Width()) + std::string(UNIT_PIXEL);
            gridMatrix_.clear();
        }
        colsArgs += ' ' + current;
    }
    return colsArgs;
}

void RenderGridLayout::InitialGridProp()
{
    // Not first time layout after update, no need to initial.
    if (!updateFlag_) {
        return;
    }
    rowGap_ = NormalizePercentToPx(userRowGap_, true);
    colGap_ = NormalizePercentToPx(userColGap_, false);

    rowSize_ = ((gridHeight_ > 0.0) && (gridHeight_ < GetLayoutParam().GetMaxSize().Height())) ? gridHeight_
        : GetLayoutParam().GetMaxSize().Height();
    colSize_ = ((gridWidth_ > 0.0) && (gridWidth_ < GetLayoutParam().GetMaxSize().Width())) ? gridWidth_
        : GetLayoutParam().GetMaxSize().Width();
    if (NearEqual(rowSize_, Size::INFINITE_SIZE) &&
        (rowsArgs_.find(UNIT_PERCENT) != std::string::npos || rowsArgs_.find(UNIT_RATIO) != std::string::npos)) {
        rowSize_ = viewPort_.Height();
    }
    if (NearEqual(colSize_, Size::INFINITE_SIZE) &&
        (colsArgs_.find(UNIT_PERCENT) != std::string::npos || colsArgs_.find(UNIT_RATIO) != std::string::npos)) {
        colSize_ = viewPort_.Width();
    }
    LOGD("Row[%{public}s]: %{public}lf %{public}lf", rowsArgs_.c_str(), rowSize_, rowGap_);
    LOGD("Col[%{public}s]: %{public}lf %{public}lf", colsArgs_.c_str(), colSize_, colGap_);
    std::vector<double> rows = ParseArgs(PreParseRows(), rowSize_, rowGap_);
    std::vector<double> cols = ParseArgs(PreParseCols(), colSize_, colGap_);
    if (rows.empty()) {
        rows.push_back(rowSize_);
    }
    if (cols.empty()) {
        cols.push_back(colSize_);
    }
    if (NearEqual(rowSize_, Size::INFINITE_SIZE)) {
        rowSize_ = std::accumulate(rows.begin(), rows.end(), (rows.size() - 1) * rowGap_);
    }
    if (NearEqual(colSize_, Size::INFINITE_SIZE)) {
        colSize_ = std::accumulate(cols.begin(), cols.end(), (cols.size() - 1) * colGap_);
    }
    // Initialize the columnCount and rowCount, default is 1
    colCount_ = cols.size();
    rowCount_ = rows.size();
    itemCountMax_ = colCount_ * rowCount_;
    gridCells_.clear();
    int32_t row = 0;
    for (auto height : rows) {
        int32_t col = 0;
        for (auto width : cols) {
            gridCells_[row][col] = Size(width, height);
            ++col;
        }
        ++row;
    }
    UpdateAccessibilityAttr();
    LOGD("GridLayout: %{public}lf %{public}lf %{public}d %{public}d", colSize_, rowSize_, colCount_, rowCount_);
}

void RenderGridLayout::UpdateAccessibilityAttr()
{
    auto refPtr = accessibilityNode_.Upgrade();
    if (!refPtr) {
        LOGE("GetAccessibilityNode failed.");
        return;
    }
    auto collectionInfo = refPtr->GetCollectionInfo();
    collectionInfo.rows = rowCount_;
    collectionInfo.columns = colCount_;
    refPtr->SetCollectionInfo(collectionInfo);
}

// Support five ways below:
// (1) 50px 100px 60px
// (2) 1fr 1fr 2fr
// (3) 30% 20% 50%
// (4) repeat(2,100px 20%) -- will be prebuild by JS Engine to --- 100px 20% 100px 20%
// (5) repeat(auto-fill, 100px 300px)  -- will be prebuild by JS Engine to --- auto-fill 100px 300px
std::vector<double> RenderGridLayout::ParseArgs(const std::string& agrs, double size, double gap)
{
    std::vector<double> lens;
    if (agrs.empty()) {
        return lens;
    }
    double pxSum = 0.0; // First priority: such as 50px
    double peSum = 0.0; // Second priority: such as 20%
    double frSum = 0.0; // Third priority: such as 2fr
    std::vector<std::string> strs;
    std::string handledArg = agrs;
    ConvertRepeatArgs(handledArg);
    StringUtils::StringSpliter(handledArg, ' ', strs);
    if (!strs.empty() && strs[0] == UNIT_AUTO_FILL) {
        return ParseAutoFill(strs, size, gap);
    }
    // first loop calculate all type sums.
    for (auto str : strs) {
        if (str.find(UNIT_PIXEL) != std::string::npos) {
            pxSum += StringUtils::StringToDouble(str);
        } else if (str.find(UNIT_PERCENT) != std::string::npos) {
            peSum += StringUtils::StringToDouble(str);
        } else if (str.find(UNIT_RATIO) != std::string::npos) {
            frSum += StringUtils::StringToDouble(str);
        } else {
            LOGE("Unsupported type: %{public}s, and use 0.0", str.c_str());
        }
    }
    if (GreatOrEqual(peSum, FULL_PERCENT)) {
        peSum = FULL_PERCENT;
    }
    // Second loop calculate actual width or height.
    double sizeLeft = size - (strs.size() - 1) * gap;
    double prSumLeft = FULL_PERCENT;
    double frSizeSum = size * (FULL_PERCENT - peSum) / FULL_PERCENT - (strs.size() - 1) * gap - pxSum;
    for (const auto& str : strs) {
        double num = StringUtils::StringToDouble(str);
        if (str.find(UNIT_PIXEL) != std::string::npos) {
            lens.push_back(sizeLeft < 0.0 ? 0.0 : std::clamp(num, 0.0, sizeLeft));
            sizeLeft -= num;
        } else if (str.find(UNIT_PERCENT) != std::string::npos) {
            num = prSumLeft < num ? prSumLeft : num;
            auto prSize = size * num / FULL_PERCENT;
            lens.push_back(prSize);
            prSumLeft -= num;
            sizeLeft -= prSize;
        } else if (str.find(UNIT_RATIO) != std::string::npos) {
            lens.push_back(NearZero(frSum) ? 0.0 : frSizeSum / frSum * num);
        } else {
            lens.push_back(0.0);
        }
    }
    return lens;
}

void RenderGridLayout::ConvertRepeatArgs(std::string& handledArg)
{
    if (handledArg.find(REPEAT_PREFIX) == std::string::npos) {
        return;
    }
    handledArg.erase(0, handledArg.find_first_not_of(" ")); // trim the input str
    std::smatch matches;
    if (handledArg.find(UNIT_AUTO_FILL) != std::string::npos) {
        if (handledArg.size() > REPEAT_MIN_SIZE && std::regex_match(handledArg, matches, AUTO_REGEX)) {
            handledArg = matches[1].str() + matches[2].str();
        }
    } else {
        if (handledArg.size() > REPEAT_MIN_SIZE && std::regex_match(handledArg, matches, REPEAT_NUM_REGEX)) {
            auto count = StringUtils::StringToInt(matches[1].str());
            std::string repeatString = matches[2].str();
            while (count > 1) {
                repeatString.append(std::string(matches[2].str()));
                --count;
            }
            handledArg = repeatString;
        }
    }
}

std::vector<double> RenderGridLayout::ParseAutoFill(const std::vector<std::string>& strs, double size, double gap)
{
    std::vector<double> lens;
    if (strs.size() <= 1) {
        return lens;
    }
    auto allocatedSize = size - (strs.size() - 2) * gap; // size() - 2 means 'auto-fill' should be erased.
    double pxSum = 0.0;
    double peSum = 0.0;
    for (const auto& str : strs) {
        auto num = StringUtils::StringToDouble(str);
        if (str.find(UNIT_PIXEL) != std::string::npos) {
            num = pxSum > allocatedSize ? 0.0 : num;
            pxSum += num;
            lens.emplace_back(num);
        } else if (str.find(UNIT_PERCENT) != std::string::npos) {
            // adjust invalid percent
            num = peSum >= FULL_PERCENT ? 0.0 : num;
            peSum += num;
            pxSum += num / FULL_PERCENT * size;
            lens.emplace_back(num / FULL_PERCENT * size);
        } else {
            LOGD("Unsupported type: %{public}s, and use 0.0", str.c_str());
        }
    }
    allocatedSize -= pxSum;
    if (LessOrEqual(allocatedSize, 0.0)) {
        return lens;
    }
    pxSum += lens.size() * gap;
    int32_t repeatCount = allocatedSize / pxSum;
    std::vector<double> newLens;
    for (int32_t i = 0; i < repeatCount + 1; i++) {
        newLens.insert(newLens.end(), lens.begin(), lens.end());
    }
    allocatedSize -= pxSum * repeatCount;
    for (auto lenIter = lens.begin(); lenIter != lens.end(); lenIter++) {
        allocatedSize -= *lenIter + gap;
        if (LessNotEqual(allocatedSize, 0.0)) {
            break;
        }
        newLens.emplace_back(*lenIter);
    }
    return newLens;
}

void RenderGridLayout::SetItemIndex(const RefPtr<RenderNode>& child, int32_t index)
{
    int32_t depth = DEFAULT_DEPTH;
    auto item = child;
    auto gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
    while (!gridLayoutItem && depth > 0) {
        if (!item || item->GetChildren().empty()) {
            return;
        }
        item = item->GetChildren().front();
        gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
        --depth;
    }
    if (gridLayoutItem) {
        gridLayoutItem->SetIndex(index);
    }
}

int32_t RenderGridLayout::GetItemRowIndex(const RefPtr<RenderNode>& child) const
{
    int32_t depth = DEFAULT_DEPTH;
    int32_t rowIndex = -1;
    auto item = child;
    auto gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
    while (!gridLayoutItem && depth > 0) {
        if (!item || item->GetChildren().empty()) {
            return rowIndex;
        }
        item = item->GetChildren().front();
        gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
        --depth;
    }
    if (gridLayoutItem) {
        rowIndex = gridLayoutItem->GetRowIndex();
    }
    return rowIndex;
}

int32_t RenderGridLayout::GetItemColumnIndex(const RefPtr<RenderNode>& child) const
{
    int32_t depth = DEFAULT_DEPTH;
    int32_t columnIndex = -1;
    auto item = child;
    auto gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
    while (!gridLayoutItem && depth > 0) {
        if (!item || item->GetChildren().empty()) {
            return columnIndex;
        }
        item = item->GetChildren().front();
        gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
        --depth;
    }
    if (gridLayoutItem) {
        columnIndex = gridLayoutItem->GetColumnIndex();
    }
    return columnIndex;
}

int32_t RenderGridLayout::GetItemSpan(const RefPtr<RenderNode>& child, bool isRow) const
{
    int32_t depth = DEFAULT_DEPTH;
    int32_t span = -1;
    auto item = child;
    auto gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
    while (!gridLayoutItem && depth > 0) {
        if (!item || item->GetChildren().empty()) {
            return span;
        }
        item = item->GetChildren().front();
        gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
        --depth;
    }
    if (gridLayoutItem) {
        span = isRow ? gridLayoutItem->GetRowSpan() : gridLayoutItem->GetColumnSpan();
    }
    return span < 1 ? 1 : span;
}

void RenderGridLayout::GetNextGrid(int32_t& curRow, int32_t& curCol) const
{
    LOGD("%{public}s begin. curRow: %{public}d, curCol: %{public}d", __PRETTY_FUNCTION__, curRow, curCol);
    if (isVertical_) {
        ++curCol;
        if (curCol >= colCount_) {
            curCol = 0;
            ++curRow;
        }
    } else {
        ++curRow;
        if (curRow >= rowCount_) {
            curRow = 0;
            ++curCol;
        }
    }
}

void RenderGridLayout::GetPreviousGird(int32_t& curRow, int32_t& curCol) const
{
    LOGD("%{public}s begin. curRow: %{public}d, curCol: %{public}d", __PRETTY_FUNCTION__, curRow, curCol);
    if (isVertical_) {
        --curCol;
        if (curCol < 0) {
            curCol = colCount_ - 1;
            --curRow;
        }
    } else {
        --curRow;
        if (curRow < 0) {
            curRow = rowCount_ - 1;
            --curCol;
        }
    }
}

bool RenderGridLayout::CheckGridPlaced(int32_t index, int32_t row, int32_t col, int32_t& rowSpan, int32_t& colSpan)
{
    auto rowIter = gridMatrix_.find(row);
    if (rowIter != gridMatrix_.end()) {
        auto colIter = rowIter->second.find(col);
        if (colIter != rowIter->second.end()) {
            return false;
        }
    }
    rowSpan = std::min(rowCount_ - row, rowSpan);
    colSpan = std::min(colCount_ - col, colSpan);
    int32_t rSpan = 0;
    int32_t cSpan = 0;
    int32_t retColSpan = 1;
    while (rSpan < rowSpan) {
        rowIter = gridMatrix_.find(rSpan + row);
        if (rowIter != gridMatrix_.end()) {
            cSpan = 0;
            while (cSpan < colSpan) {
                if (rowIter->second.find(cSpan + col) != rowIter->second.end()) {
                    colSpan = cSpan;
                    break;
                }
                ++cSpan;
            }
        } else {
            cSpan = colSpan;
        }
        if (retColSpan > cSpan) {
            break;
        }
        retColSpan = cSpan;
        ++rSpan;
    }
    rowSpan = rSpan;
    colSpan = retColSpan;
    for (int32_t i = row; i < row + rowSpan; ++i) {
        std::map<int32_t, int32_t> rowMap;
        auto iter = gridMatrix_.find(i);
        if (iter != gridMatrix_.end()) {
            rowMap = iter->second;
        }
        for (int32_t j = col; j < col + colSpan; ++j) {
            rowMap.emplace(std::make_pair(j, index));
        }
        gridMatrix_[i] = rowMap;
    }
    LOGD("%{public}d %{public}d %{public}d %{public}d %{public}d", index, row, col, rowSpan, colSpan);
    return true;
}

void RenderGridLayout::PerformLayout()
{
    if (isDragChangeLayout_ && !needRestoreScene_) {
        isDragChangeLayout_ = false;
        return;
    }
    needRestoreScene_ = false;
    isDragChangeLayout_ = false;
    gridMatrix_.clear();
    if (GetChildren().empty()) {
        return;
    }

    // register the item seleceted callback
    if (editMode_) {
        RegisterLongPressedForItems();
    }
    // Initialize the the grid layout prop
    if (isDynamicGrid_) {
        InitialDynamicGridProp();
    } else {
        InitialGridProp();
    }
    if (editMode_) {
        PerformLayoutForEditGrid();
    } else {
        int32_t rowIndex = 0;
        int32_t colIndex = 0;
        int32_t itemIndex = 0;
        for (const auto& item : GetChildren()) {
            int32_t itemRow = GetItemRowIndex(item);
            int32_t itemCol = GetItemColumnIndex(item);
            int32_t itemRowSpan = GetItemSpan(item, true);
            int32_t itemColSpan = GetItemSpan(item, false);
            if (itemRow >= 0 && itemRow < rowCount_ && itemCol >= 0 && itemCol < colCount_ &&
                CheckGridPlaced(itemIndex, itemRow, itemCol, itemRowSpan, itemColSpan)) {
                item->Layout(MakeInnerLayoutParam(itemRow, itemCol, itemRowSpan, itemColSpan));
                SetChildPosition(item, itemRow, itemCol, itemRowSpan, itemColSpan);
            } else {
                while (!CheckGridPlaced(itemIndex, rowIndex, colIndex, itemRowSpan, itemColSpan)) {
                    GetNextGrid(rowIndex, colIndex);
                    if (rowIndex >= rowCount_ || colIndex >= colCount_) {
                        break;
                    }
                }
                if (rowIndex >= rowCount_ || colIndex >= colCount_) {
                    DisableChild(item, itemIndex);
                    continue;
                }
                item->Layout(MakeInnerLayoutParam(rowIndex, colIndex, itemRowSpan, itemColSpan));
                SetChildPosition(item, rowIndex, colIndex, itemRowSpan, itemColSpan);
            }
            SetItemIndex(item, itemIndex); // Set index for focus adjust.
            ++itemIndex;
            LOGD("%{public}d %{public}d %{public}d %{public}d", rowIndex, colIndex, itemRowSpan, itemColSpan);
        }
    }
    SetLayoutSize(GetLayoutParam().Constrain(Size(colSize_, rowSize_)));
}

bool RenderGridLayout::CouldBeInserted()
{
    int32_t itemCount = CountItemInGrid();
    if (itemCount >= itemCountMax_) {
        return false;
    }
    return true;
}

bool RenderGridLayout::NeedBeLarger()
{
    int32_t itemCount = CountItemInGrid();
    if (curItemCountMax_ >= (itemCount + 1)) {
        return false;
    }
    return true;
}

bool RenderGridLayout::NeedBeSmaller()
{
    int32_t itemCount = CountItemInGrid();
    int32_t crossItemCount = 0;
    if (isVertical_) {
        crossItemCount = colCount_;
    } else {
        crossItemCount = rowCount_;
    }
    if ((curItemCountMax_ - crossItemCount) < itemCount) {
        return false;
    }
    return true;
}

void RenderGridLayout::BackGridMatrix()
{
    gridMatrixBack_.clear();
    gridMatrixBack_ = gridMatrix_;
}

void RenderGridLayout::RestoreScene()
{
    // Until the moving animation is done, only performlayout needs to be triggered here to retrieve the original data
    // for layout
    needRestoreScene_ = true;
}

void RenderGridLayout::OnTouchTestHit(
    const Offset& coordinateOffset, const TouchRestrict& touchRestrict, TouchTestResult& result)
{
    if (dragDropGesture_) {
        result.emplace_back(dragDropGesture_);
    }
}

void RenderGridLayout::ClearDragInfo()
{
    curInsertRowIndex_ = -1;
    curInsertColumnIndex_ = -1;
    dragPosRowIndex_ = -1;
    dragPosColumnIndex_ = -1;
    dragingItemIndex_ = -1;
    dragPosChanged_ = false;
    itemLongPressed_ = false;
    itemDragEntered_ = false;
    itemDragStarted_ = false;
    isInMainGrid_ = false;
    isMainGrid_ = false;
    reEnter_ = false;
    isDragChangeLayout_ = false;
    dragingItemRenderNode_.Reset();
    subGrid_.Reset();
}

void RenderGridLayout::CalIsVertical()
{
    if (isDynamicGrid_) {
        if (!colsArgs_.empty() && rowsArgs_.empty()) {
            isVertical_ = true;
        } else {
            isVertical_ = false;
        }
    } else {
        if (direction_ == FlexDirection::COLUMN) {
            isVertical_ = true;
        } else {
            isVertical_ = false;
        }
    }
}

void RenderGridLayout::RegisterLongPressedForItems()
{
    if (GetChildren().empty()) {
        LOGE("%{public}s. has no children", __PRETTY_FUNCTION__);
        return;
    }

    for (const auto& child : GetChildren()) {
        int32_t depth = DEFAULT_DEPTH;
        auto item = child;
        auto gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
        while (!gridLayoutItem && depth > 0) {
            if (!item || item->GetChildren().empty()) {
                LOGE("%{public}s. item has no children anymore", __PRETTY_FUNCTION__);
                break;
            }
            item = item->GetChildren().front();
            gridLayoutItem = AceType::DynamicCast<RenderGridLayoutItem>(item);
            --depth;
        }
        if (gridLayoutItem) {
            gridLayoutItem->SetOnItemLongPressed(
                [weak = WeakClaim(this)](int32_t index, const WeakPtr<RenderNode>& itemRenderNode) {
                    auto render = weak.Upgrade();
                    if (!render) {
                        LOGE("%{public}s .renderGrid is null", __PRETTY_FUNCTION__);
                        return false;
                    }
                    auto item = itemRenderNode.Upgrade();
                    if ((index < 0) || (!item)) {
                        LOGE("%{public}s .index invaild or item is null", __PRETTY_FUNCTION__);
                        return false;
                    }
                    render->dragingItemIndex_ = index;
                    render->dragingItemRenderNode_ = itemRenderNode;
                    render->itemLongPressed_ = true;
                    return true;
                });
        } else {
            LOGI("%{public}s begin child is not item.", __PRETTY_FUNCTION__);
        }
    }
}

void RenderGridLayout::CreateDragDropRecognizer()
{
    if (dragDropGesture_) {
        return;
    }
    auto longPress = AceType::MakeRefPtr<LongPressRecognizer>(GetContext(), DEFAULT_DURATION, DEFAULT_FINGERS, false);
    longPress->SetOnLongPress(
        [weak = WeakClaim<RenderGridLayout>(this), context = GetContext()](const LongPressInfo& info) {
            auto renderGrid = weak.Upgrade();
            if (renderGrid) {
                GestureEvent eventinfo;
                eventinfo.SetGlobalPoint(Point(info.GetGlobalLocation().GetX(), info.GetGlobalLocation().GetY()));
                auto targetItem = renderGrid->FindTargetRenderNode<RenderGridLayoutItem>(context.Upgrade(), eventinfo);
                if (targetItem) {
                    targetItem->OnLongPressEvent();
                    renderGrid->SetMainTargetRenderGrid(renderGrid);
                    renderGrid->SetPreTargetRenderGrid(renderGrid);
                    Point lastLongPressPoint;
                    lastLongPressPoint.SetX(eventinfo.GetGlobalPoint().GetX());
                    lastLongPressPoint.SetY(eventinfo.GetGlobalPoint().GetY());
                    renderGrid->SetLongPressPoint(lastLongPressPoint);
                }
            }
        });
    PanDirection panDirection;
    auto pan = AceType::MakeRefPtr<PanRecognizer>(GetContext(), DEFAULT_FINGERS, panDirection, DEFAULT_DISTANCE);
    pan->SetOnActionUpdate(std::bind(&RenderGridLayout::PanOnActionUpdate, this, std::placeholders::_1));
    pan->SetOnActionEnd(std::bind(&RenderGridLayout::PanOnActionEnd, this, std::placeholders::_1));
    pan->SetOnActionCancel([weak = WeakClaim<RenderGridLayout>(this)]() {
        auto renderGrid = weak.Upgrade();
        if (renderGrid) {
            renderGrid->SetPreTargetRenderGrid(nullptr);
            renderGrid->SetMainTargetRenderGrid(nullptr);
        }
    });

    std::vector<RefPtr<GestureRecognizer>> recognizers { longPress, pan };
    dragDropGesture_ = AceType::MakeRefPtr<OHOS::Ace::SequencedRecognizer>(GetContext(), recognizers);
}

void RenderGridLayout::ActionStart(const RefPtr<ItemDragInfo>& info, RefPtr<Component> customComponent)
{
    auto pipelineContext = GetContext().Upgrade();
    if (pipelineContext) {
        auto stackElement = pipelineContext->GetLastStack();
        auto positionedComponent = AceType::MakeRefPtr<PositionedComponent>(customComponent);
        positionedComponent->SetTop(Dimension(info->GetY()));
        positionedComponent->SetLeft(Dimension(info->GetX()));

        auto updatePosition = [renderGirdLayout = AceType::Claim(this)](
                                  const std::function<void(const RefPtr<ItemDragInfo>& info)>& func) {
            if (!renderGirdLayout) {
                return;
            }
            renderGirdLayout->SetUpdatePositionId(func);
        };
        positionedComponent->SetUpdatePositionFunc(updatePosition);
        stackElement->PushComponent(positionedComponent);
        stackElement->PerformBuild();
    }
}

void RenderGridLayout::PanOnActionUpdate(const GestureEvent& info)
{
    auto renderGirdLayout = AceType::Claim(this);
    if (!renderGirdLayout) {
        return;
    }
    if (renderGirdLayout->GetUpdatePositionId()) {
        Point point = info.GetGlobalPoint();
        RefPtr<ItemDragInfo> eventInfo = AceType::MakeRefPtr<ItemDragInfo>();
        eventInfo->SetX(point.GetX());
        eventInfo->SetY(point.GetY());
        renderGirdLayout->GetUpdatePositionId()(eventInfo);
    }
    RefPtr<ItemDragInfo> event = AceType::MakeRefPtr<ItemDragInfo>();
    // MMIO could not provide correct point info when touch up, so keep the last point info
    lastGlobalPoint_.SetX(info.GetGlobalPoint().GetX());
    lastGlobalPoint_.SetY(info.GetGlobalPoint().GetY());
    event->SetX(info.GetGlobalPoint().GetX());
    event->SetY(info.GetGlobalPoint().GetY());
    auto targetRenderGrid = FindTargetRenderNode<RenderGridLayout>(GetContext().Upgrade(), info);
    auto preTargetRenderGrid = GetPreTargetRenderGrid();
    auto mainTargetRenderGrid = GetMainTargetRenderGrid();

    if (!mainTargetRenderGrid) {
        return;
    }

    if (targetRenderGrid) {
        if (preTargetRenderGrid == targetRenderGrid) {
            mainTargetRenderGrid->OnDragMove(event);
            return;
        }
    }
    if (preTargetRenderGrid) {
        if (itemLongPressed_) {
            RefPtr<ItemDragInfo> eventLongPress = AceType::MakeRefPtr<ItemDragInfo>();
            eventLongPress->SetX(lastLongPressPoint_.GetX());
            eventLongPress->SetY(lastLongPressPoint_.GetY());
            mainTargetRenderGrid->OnDragMove(eventLongPress);
        }
        preTargetRenderGrid->OnDragLeave(event);
        subGrid_.Reset();
    } else if (targetRenderGrid && !itemLongPressed_) {
        subGrid_ = AceType::WeakClaim(AceType::RawPtr(targetRenderGrid));
        targetRenderGrid->OnDragEnter(event);
    } else {
        // drag out of all gridLayout
        mainTargetRenderGrid->OnDragMove(event);
    }
    SetPreTargetRenderGrid(targetRenderGrid);
}

void RenderGridLayout::PanOnActionEnd(const GestureEvent& info)
{
    auto pipelineContext = GetContext().Upgrade();
    auto gridItem = AceType::Claim(this)->dragingItemRenderNode_.Upgrade();
    if (pipelineContext && gridItem) {
        auto stackElement = pipelineContext->GetLastStack();
        stackElement->PopComponent();
    }
    RefPtr<ItemDragInfo> event = AceType::MakeRefPtr<ItemDragInfo>();
    // MMIO could not provide correct point info when touch up, so restore the last point info
    event->SetX(lastGlobalPoint_.GetX());
    event->SetY(lastGlobalPoint_.GetY());
    auto targetRenderGrid = GetPreTargetRenderGrid();
    auto mainTargetRenderGrid = GetMainTargetRenderGrid();

    if (targetRenderGrid) {
        if (mainTargetRenderGrid) {
            mainTargetRenderGrid->OnDrop(event);
            SetPreTargetRenderGrid(nullptr);
            SetMainTargetRenderGrid(nullptr);
        }
    } else {
        // drag out of all gridLayout
        if (mainTargetRenderGrid) {
            mainTargetRenderGrid->OnDrop(event);
        }
    }
}

template<typename T>
RefPtr<T> RenderGridLayout::FindTargetRenderNode(const RefPtr<PipelineContext> context, const GestureEvent& info)
{
    if (!context) {
        return nullptr;
    }

    auto pageRenderNode = context->GetLastPageRender();
    if (!pageRenderNode) {
        return nullptr;
    }

    return pageRenderNode->FindChildNodeOfClass<T>(info.GetGlobalPoint(), info.GetGlobalPoint());
}

void RenderGridLayout::OnDragEnter(const RefPtr<ItemDragInfo>& info)
{
    LOGD("%{public}s begin. itemDragStarted_ %{public}d, itemDragEntered_ %{public}d, itemLongPressed_ %{public}d, "
         "isInMainGrid_ %{public}d, isMainGrid_ %{public}d",
         __PRETTY_FUNCTION__, itemDragStarted_, itemDragEntered_, itemLongPressed_, isInMainGrid_, isMainGrid_);
    if (!editMode_) {
        LOGW("%{public}s no editMode no dragevent.", __PRETTY_FUNCTION__);
        return;
    }

    if (component_->GetOnGridDragEnterId()) {
        component_->GetOnGridDragEnterId()(info);
    }
    if (isMainGrid_) {
        if (itemDragStarted_) {
            isInMainGrid_ = true;
            reEnter_ = true;
        }
    } else {
        itemDragEntered_ = true;
        if (CouldBeInserted()) {
            BackGridMatrix();
            if (isDynamicGrid_ && NeedBeLarger()) {
                // BeLager and render
                InitialDynamicGridProp(1);
                SetLayoutSize(GetLayoutParam().Constrain(Size(colSize_, rowSize_)));
                isDragChangeLayout_ = true;
                MarkNeedLayout();
            }
        } else {
            LOGW("%{public}s couldn't be inserted.", __PRETTY_FUNCTION__);
        }
    }
}

void RenderGridLayout::OnDragLeave(const RefPtr<ItemDragInfo>& info)
{
    LOGD("%{public}s begin. itemDragStarted_ %{public}d, itemDragEntered_ %{public}d, itemLongPressed_ %{public}d, "
         "isInMainGrid_ %{public}d, isMainGrid_ %{public}d",
         __PRETTY_FUNCTION__, itemDragStarted_, itemDragEntered_, itemLongPressed_, isInMainGrid_, isMainGrid_);
    if (!editMode_) {
        LOGW("%{public}s no editMode no dragevent.", __PRETTY_FUNCTION__);
        return;
    }

    if (isMainGrid_) {
        if (itemDragStarted_) {
            isInMainGrid_ = false;
            if (component_->GetOnGridDragLeaveId()) {
                component_->GetOnGridDragLeaveId()(info, dragingItemIndex_);
            } else {
                LOGE("%{public}s no onGridGragLeave registered.", __PRETTY_FUNCTION__);
            }
            FakeRemoveDragItem();
            if (isDynamicGrid_) {
                InitialDynamicGridProp(-1);
                SetLayoutSize(GetLayoutParam().Constrain(Size(colSize_, rowSize_)));
                isDragChangeLayout_ = true;
                MarkNeedLayout();
            }
        }
    } else {
        if (itemDragEntered_) {
            itemDragEntered_ = false;
            if (component_->GetOnGridDragLeaveId()) {
                component_->GetOnGridDragLeaveId()(info, -1);
            } else {
                LOGE("%{public}s no onGridGragLeave registered.", __PRETTY_FUNCTION__);
            }
            if (isDynamicGrid_ && NeedBeSmaller()) {
                // BeSmaller
                InitialDynamicGridProp(-1);
                SetLayoutSize(GetLayoutParam().Constrain(Size(colSize_, rowSize_)));
                RestoreScene();
                isDragChangeLayout_ = true;
                MarkNeedLayout();
            }
        }
    }
}

void RenderGridLayout::OnDragMove(const RefPtr<ItemDragInfo>& info)
{
    LOGD("%{public}s begin. itemDragStarted_ %{public}d, itemDragEntered_ %{public}d, itemLongPressed_ %{public}d, "
         "isInMainGrid_ %{public}d, isMainGrid_ %{public}d",
         __PRETTY_FUNCTION__, itemDragStarted_, itemDragEntered_, itemLongPressed_, isInMainGrid_, isMainGrid_);
    if (!editMode_) {
        LOGW("%{public}s no editMode no dragevent.", __PRETTY_FUNCTION__);
        return;
    }

    if (!itemDragEntered_ && itemLongPressed_ && !itemDragStarted_) {
        ImpDragStart(info);
    }

    if ((!isInMainGrid_ && itemDragEntered_) || (itemDragStarted_ && isInMainGrid_ && isMainGrid_)) {
        if (CouldBeInserted() || itemDragStarted_) {
            if (CalDragCell(info)) {
                MoveItems();
            }
            int32_t insertIndex = CalIndexForItemByRowAndColum(curInsertRowIndex_, curInsertColumnIndex_);
            if (component_->GetOnGridDragMoveId()) {
                component_->GetOnGridDragMoveId()(info, dragingItemIndex_, insertIndex);
            } else {
                LOGE("%{public}s no onGridGragMove registered.", __PRETTY_FUNCTION__);
            }
            isDragChangeLayout_ = true;
            MarkNeedLayout();
        } else {
            if (component_->GetOnGridDragMoveId()) {
                component_->GetOnGridDragMoveId()(info, dragingItemIndex_, -1);
            } else {
                LOGE("%{public}s no onGridGragMove registered.", __PRETTY_FUNCTION__);
            }
        }
        return;
    }
    if (isMainGrid_) {
        auto subGrid = subGrid_.Upgrade();
        if (subGrid && !isInMainGrid_) {
            subGrid->OnDragMove(info);
            if (component_->GetOnGridDragMoveId()) {
                component_->GetOnGridDragMoveId()(info, dragingItemIndex_, -1);
            }
        } else if (!isInMainGrid_) {
            if (component_->GetOnGridDragMoveId()) {
                component_->GetOnGridDragMoveId()(info, dragingItemIndex_, -1);
            }
        }
    }
}

bool RenderGridLayout::ImpDropInGrid(const RefPtr<ItemDragInfo>& info)
{
    itemDragStarted_ = false;
    itemDragEntered_ = false;
    bool result = false;
    gridMatrixBack_.clear();
    if (CouldBeInserted()) {
        if (CalDragCell(info)) {
            MoveItems();
        }
        int32_t insertIndex = CalIndexForItemByRowAndColum(curInsertRowIndex_, curInsertColumnIndex_);
        if (insertIndex >= 0 && insertIndex < itemCountMax_) {
            result = true;
        }
        if (component_->GetOnGridDropId()) {
            component_->GetOnGridDropId()(info, dragingItemIndex_, insertIndex, result);
        } else {
            LOGE("%{public}s no onGridDrop registered.", __PRETTY_FUNCTION__);
        }
        result = true;
    } else {
        if (component_->GetOnGridDropId()) {
            component_->GetOnGridDropId()(info, dragingItemIndex_, -1, false);
        } else {
            LOGE("%{public}s no onGridDrop registered.", __PRETTY_FUNCTION__);
        }
    }
    dragingItemIndex_ = -1;
    dragingItemRenderNode_.Reset();
    curInsertRowIndex_ = -1;
    curInsertColumnIndex_ = -1;
    return result;
}

bool RenderGridLayout::OnDrop(const RefPtr<ItemDragInfo>& info)
{
    LOGD("%{public}s begin. itemDragStarted_ %{public}d, itemDragEntered_ %{public}d, itemLongPressed_ %{public}d, "
         "isInMainGrid_ %{public}d, isMainGrid_ %{public}d",
         __PRETTY_FUNCTION__, itemDragStarted_, itemDragEntered_, itemLongPressed_, isInMainGrid_, isMainGrid_);
    if (!editMode_) {
        LOGW("%{public}s no editMode no dragevent.", __PRETTY_FUNCTION__);
        return false;
    }

    if ((isMainGrid_ && isInMainGrid_ && itemDragStarted_) || (!isMainGrid_ && itemDragEntered_)) {
        bool ret = ImpDropInGrid(info);
        ClearDragInfo();
        return ret;
    }

    if (isMainGrid_) {
        auto subGrid = subGrid_.Upgrade();
        if (subGrid && !isInMainGrid_) {
            bool result = subGrid->OnDrop(info);
            if (component_->GetOnGridDropId()) {
                component_->GetOnGridDropId()(info, dragingItemIndex_, -1, result);
            }
            ClearDragInfo();
            if (!result) {
                RestoreScene();
                MarkNeedLayout();
            }
            return result;
        } else if (!isInMainGrid_) {
            if (component_->GetOnGridDropId()) {
                component_->GetOnGridDropId()(info, dragingItemIndex_, -1, false);
            }
            RestoreScene();
            isDragChangeLayout_ = true;
            ClearDragInfo();
            MarkNeedLayout();
            return false;
        }
    }
    ClearDragInfo();
    return false;
}

void RenderGridLayout::ImpDragStart(const RefPtr<ItemDragInfo>& info)
{
    itemDragStarted_ = true;
    itemLongPressed_ = false;
    isMainGrid_ = true;
    isInMainGrid_ = true;
    BackGridMatrix();
    auto itemRender = dragingItemRenderNode_.Upgrade();
    if (itemRender) {
        itemRender->SetVisible(false);
        DisableChild(itemRender, dragingItemIndex_);
    }
    if (component_->GetOnGridDragStartId()) {
        auto customComponent = component_->GetOnGridDragStartId()(info, dragingItemIndex_);
        ActionStart(info, customComponent);
    } else {
        LOGE("%{public}s no onGridGragStart registered.", __PRETTY_FUNCTION__);
    }
}

void RenderGridLayout::OnCallSubDragEnter(const RefPtr<ItemDragInfo>& info)
{
    auto subGrid = subGrid_.Upgrade();
    if (subGrid) {
        subGrid->OnDragEnter(info);
    }
}

void RenderGridLayout::OnCallSubDragLeave(const RefPtr<ItemDragInfo>& info)
{
    auto subGrid = subGrid_.Upgrade();
    if (subGrid) {
        subGrid->OnDragLeave(info);
    }
}

int32_t RenderGridLayout::CountItemInGrid()
{
    int32_t count = 0;
    for (int main = 0; main < rowCount_; main++) {
        auto mainIter = gridMatrix_.find(main);
        if (mainIter != gridMatrix_.end()) {
            for (int cross = 0; cross < colCount_; cross++) {
                auto crossIter = mainIter->second.find(cross);
                if (crossIter != mainIter->second.end()) {
                    int32_t index = crossIter->second;
                    if (index >= 0) {
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

void RenderGridLayout::InitialDynamicGridProp(int32_t dragLeaveOrEnter)
{
    rowGap_ = NormalizePercentToPx(userRowGap_, true);
    colGap_ = NormalizePercentToPx(userColGap_, false);

    SetGridLayoutParam();

    std::vector<double> cols;
    std::vector<double> rows;
    if (isVertical_) {
        CalculateVerticalSize(cols, rows, dragLeaveOrEnter);
        itemCountMax_ = cols.size() * mainCountMax_;
    } else {
        CalculateHorizontalSize(cols, rows, dragLeaveOrEnter);
        itemCountMax_ = rows.size() * mainCountMax_;
    }

    UpdateCollectionInfo(cols, rows);
    LOGD("GridLayout: %{public}lf %{public}lf %{public}d %{public}d", colSize_, rowSize_, colCount_, rowCount_);
}

void RenderGridLayout::SetGridLayoutParam()
{
    LayoutParam gridLayoutParam = GetLayoutParam();
    auto maxHeight = isVertical_ ? mainCountMax_ * cellLength_ + (mainCountMax_ - 1) * rowGap_
                                 : GetLayoutParam().GetMaxSize().Height();
    auto maxWidth = isVertical_ ? GetLayoutParam().GetMaxSize().Width()
                                : mainCountMax_ * cellLength_ + (mainCountMax_ - 1) * colGap_;
    auto minHeight = isVertical_ ? mainCountMin_ * cellLength_ + (mainCountMin_ - 1) * rowGap_
                                 : GetLayoutParam().GetMaxSize().Height();
    auto minWidth = isVertical_ ? GetLayoutParam().GetMaxSize().Width()
                                : mainCountMin_ * cellLength_ + (mainCountMin_ - 1) * colGap_;
    gridLayoutParam.SetMaxSize(Size(maxWidth, maxHeight));
    gridLayoutParam.SetMinSize(Size(minWidth, minHeight));
    SetLayoutParam(gridLayoutParam);
}

void RenderGridLayout::CalculateVerticalSize(
    std::vector<double>& cols, std::vector<double>& rows, int32_t dragLeaveOrEnter)
{
    colSize_ = ((gridWidth_ > 0.0) && (gridWidth_ < GetLayoutParam().GetMaxSize().Width()))
                   ? gridWidth_
                   : GetLayoutParam().GetMaxSize().Width();
    if (NearEqual(colSize_, Size::INFINITE_SIZE) &&
        (colsArgs_.find(UNIT_PERCENT) != std::string::npos || colsArgs_.find(UNIT_RATIO) != std::string::npos)) {
        colSize_ = viewPort_.Width();
    }
    // Get item width
    cols = ParseArgs(PreParseCols(), colSize_, colGap_);
    if (cols.empty()) {
        cols.push_back(colSize_);
    }
    // Get the number of items in each row
    colCount_ = cols.size();
    // Get the number of items
    auto totalNum = (int32_t)GetChildren().size() + dragLeaveOrEnter;
    // Count the number of rows
    rowCount_ =
        std::clamp((totalNum / colCount_ + (((totalNum % colCount_) == 0) ? 0 : 1)), mainCountMin_, mainCountMax_);
    rows = std::vector<double>(rowCount_, cellLength_);
    rowSize_ = rowCount_ * cellLength_ + (rowCount_ - 1) * rowGap_;
}

void RenderGridLayout::CalculateHorizontalSize(
    std::vector<double>& cols, std::vector<double>& rows, int32_t dragLeaveOrEnter)
{
    rowSize_ = ((gridHeight_ > 0.0) && (gridHeight_ < GetLayoutParam().GetMaxSize().Height()))
                   ? gridHeight_
                   : GetLayoutParam().GetMaxSize().Height();
    if (NearEqual(rowSize_, Size::INFINITE_SIZE) &&
        (rowsArgs_.find(UNIT_PERCENT) != std::string::npos || rowsArgs_.find(UNIT_RATIO) != std::string::npos)) {
        rowSize_ = viewPort_.Height();
    }
    // Get item width
    rows = ParseArgs(PreParseRows(), rowSize_, rowGap_);
    if (rows.empty()) {
        rows.push_back(rowSize_);
    }
    // Get the number of items in each col
    rowCount_ = rows.size();
    // Get the number of items
    auto totalNum = (int32_t)GetChildren().size() + dragLeaveOrEnter;
    // Count the number of cols
    colCount_ =
        std::clamp((totalNum / rowCount_ + (((totalNum % rowCount_) == 0) ? 0 : 1)), mainCountMin_, mainCountMax_);
    cols = std::vector<double>(colCount_, cellLength_);
    colSize_ = colCount_ * cellLength_ + (colCount_ - 1) * colGap_;
}

void RenderGridLayout::UpdateCollectionInfo(std::vector<double> cols, std::vector<double> rows)
{
    gridCells_.clear();
    int32_t row = 0;
    for (auto height : rows) {
        int32_t col = 0;
        for (auto width : cols) {
            gridCells_[row][col] = Size(width, height);
            ++col;
        }
        ++row;
    }
    UpdateAccessibilityAttr();
}

void RenderGridLayout::PerformLayoutForEditGrid()
{
    int32_t itemIndex = 0;
    int32_t rowIndex = 0;
    int32_t colIndex = 0;
    itemsInGrid_.clear();
    for (const auto& item : GetChildren()) {
        int32_t itemRowSpan = 1;
        int32_t itemColSpan = 1;
        if (rowIndex >= 0 && rowIndex < rowCount_ && colIndex >= 0 && colIndex < colCount_) {
            while (!CheckGridPlaced(itemIndex, rowIndex, colIndex, itemRowSpan, itemColSpan)) {
                GetNextGrid(rowIndex, colIndex);
                if (rowIndex >= rowCount_ || colIndex >= colCount_) {
                    break;
                }
            }
            if (rowIndex >= rowCount_ || colIndex >= colCount_) {
                DisableChild(item, itemIndex);
                continue;
            }
            item->Layout(MakeInnerLayoutParam(rowIndex, colIndex, itemRowSpan, itemColSpan));
            SetChildPosition(item, rowIndex, colIndex, itemRowSpan, itemColSpan);
            itemsInGrid_.push_back(item);
            SetItemIndex(item, itemIndex); // Set index for focus adjust.
            ++itemIndex;
            LOGD("%{public}d %{public}d %{public}d %{public}d", rowIndex, colIndex, itemRowSpan, itemColSpan);
        }
    }
}

bool RenderGridLayout::CalDragCell(const RefPtr<ItemDragInfo>& info)
{
    double gridPositonX = GetGlobalOffset().GetX();
    double gridPositonY = GetGlobalOffset().GetY();
    double dragRelativelyX = info->GetX() - gridPositonX;
    double dragRelativelyY = info->GetY() - gridPositonY;
    if (rightToLeft_) {
        dragRelativelyX = colSize_ - dragRelativelyX;
    }

    int32_t tmpDragPosRowIndex = -1;
    int32_t tmpDragPosColumIndex = -1;
    if (!CalDragRowIndex(dragRelativelyY, tmpDragPosRowIndex)) {
        return false;
    }

    if (!CalDragColumIndex(dragRelativelyX, tmpDragPosColumIndex)) {
        return false;
    }

    if ((dragPosRowIndex_ != tmpDragPosRowIndex) || (dragPosColumnIndex_ != tmpDragPosColumIndex)) {
        dragPosRowIndex_ = tmpDragPosRowIndex;
        dragPosColumnIndex_ = tmpDragPosColumIndex;
        dragPosChanged_ = true;
    }
    return true;
}

bool RenderGridLayout::CalDragRowIndex(double dragRelativelyY, int32_t& dragRowIndex)
{
    double rowStart = 0.0;
    double rowEnd = 0.0;
    for (int row = 0; row < rowCount_; row++) {
        rowStart = rowEnd;
        double offsetY = 0.0;
        if (row > 0 && row < (rowCount_ - 1)) {
            offsetY = rowGap_ / 2;
        } else {
            offsetY = rowGap_;
        }
        rowEnd += gridCells_.at(row).at(0).Height() + offsetY;
        if (dragRelativelyY >= rowStart && dragRelativelyY <= rowEnd) {
            dragRowIndex = row;
            return true;
        }
    }
    return false;
}

bool RenderGridLayout::CalDragColumIndex(double dragRelativelyX, int32_t& dragColIndex)
{
    double columStart = 0.0;
    double columEnd = 0.0;
    for (int col = 0; col < colCount_; col++) {
        columStart = columEnd;
        double offsetX = 0.0;
        if (col > 0 && col < (colCount_ - 1)) {
            offsetX = colGap_ / 2;
        } else {
            offsetX = colGap_;
        }
        columEnd += gridCells_.at(0).at(col).Width() + offsetX;
        if (dragRelativelyX >= columStart && dragRelativelyX <= columEnd) {
            dragColIndex = col;
            return true;
        }
    }
    return false;
}

void RenderGridLayout::MoveItems()
{
    if (!dragPosChanged_) {
        LOGI("%{public}s dragPos not change no need to move.", __PRETTY_FUNCTION__);
        return;
    }
    if (curInsertRowIndex_ >= 0 && curInsertColumnIndex_ >= 0) {
        // If there has been insert Cell now.
        MoveWhenWithInsertCell();
    } else {
        MoveWhenNoInsertCell();
    }
    dragPosChanged_ = false;
}

void RenderGridLayout::MoveWhenNoInsertCell()
{
    int32_t dragposIndex = GetIndexByGrid(dragPosRowIndex_, dragPosColumnIndex_);
    if (dragposIndex == -1) {
        // If there is no item in the cell
        MoveWhenNoInsertCellAndNoItemInDragCell();
    } else {
        MoveWhenNoInsertCellButWithItemInDragCell();
    }
}

void RenderGridLayout::MoveWhenNoInsertCellAndNoItemInDragCell()
{
    int32_t FirstEmptyCellRowIndex = -1;
    int32_t FirstEmptyCellColumIndex = -1;
    if (CalTheFirstEmptyCell(FirstEmptyCellRowIndex, FirstEmptyCellColumIndex, true)) {
        UpdateCurInsertPos(FirstEmptyCellRowIndex, FirstEmptyCellColumIndex);
    }
}

void RenderGridLayout::MoveWhenNoInsertCellButWithItemInDragCell()
{
    if (itemDragEntered_ || (itemDragStarted_ && reEnter_)) {
        MoveWhenNoInsertCellButWithItemInDragCellAndDragEnter();
    } else if (itemDragStarted_ && !reEnter_) {
        MoveWhenNoInsertCellButWithItemInDragCellAndDragStart();
    }
}

void RenderGridLayout::MoveWhenNoInsertCellButWithItemInDragCellAndDragEnter()
{
    int32_t endRow = -1;
    int32_t endColum = -1;
    if (CalTheFirstEmptyCell(endRow, endColum, true)) {
        GetPreviousGird(endRow, endColum);
        if (MoveItemsForward(dragPosRowIndex_, dragPosColumnIndex_, endRow, endColum)) {
            UpdateCurInsertPos(dragPosRowIndex_, dragPosColumnIndex_);
        }
    }
}

void RenderGridLayout::MoveWhenNoInsertCellButWithItemInDragCellAndDragStart()
{
    UpdateCurInsertPos(dragPosRowIndex_, dragPosColumnIndex_);
}

void RenderGridLayout::MoveWhenWithInsertCell()
{
    int32_t dragposIndex = GetIndexByGrid(dragPosRowIndex_, dragPosColumnIndex_);
    if (dragposIndex == -1) {
        // If there is no item in the cell
        MoveWhenWithInsertCellAndNoItemInDragCell();
    } else {
        MoveWhenWithInsertCellButWithItemInDragCell();
    }
}

void RenderGridLayout::MoveWhenWithInsertCellAndNoItemInDragCell()
{
    int32_t endRow = -1;
    int32_t endColum = -1;
    if (CalTheFirstEmptyCell(endRow, endColum, true)) {
        GetPreviousGird(endRow, endColum);
        int32_t startRow = curInsertRowIndex_;
        int32_t startColum = curInsertColumnIndex_;
        GetNextGrid(startRow, startColum);
        if (MoveItemsBackward(startRow, startColum, endRow, endColum)) {
            UpdateCurInsertPos(endRow, endColum);
        }
    }
}

void RenderGridLayout::MoveWhenWithInsertCellButWithItemInDragCell()
{
    bool dragIsBefore = false;
    bool dragIsCur =
        SortCellIndex(dragPosRowIndex_, dragPosColumnIndex_, curInsertRowIndex_, curInsertColumnIndex_, dragIsBefore);
    if (!dragIsCur && dragIsBefore) {
        MoveWhenWithInsertCellButWithItemInDragCellDragBeforeInsert();
    } else if (!dragIsCur && !dragIsBefore) {
        MoveWhenWithInsertCellButWithItemInDragCellDragAfterInsert();
    }
}

void RenderGridLayout::MoveWhenWithInsertCellButWithItemInDragCellDragBeforeInsert()
{
    int32_t endRow = curInsertRowIndex_;
    int32_t endColum = curInsertColumnIndex_;
    GetPreviousGird(endRow, endColum);
    if (MoveItemsForward(dragPosRowIndex_, dragPosColumnIndex_, endRow, endColum)) {
        UpdateCurInsertPos(dragPosRowIndex_, dragPosColumnIndex_);
    }
}

void RenderGridLayout::MoveWhenWithInsertCellButWithItemInDragCellDragAfterInsert()
{
    int32_t startRow = curInsertRowIndex_;
    int32_t startColum = curInsertColumnIndex_;
    GetNextGrid(startRow, startColum);
    if (MoveItemsBackward(startRow, startColum, dragPosRowIndex_, dragPosColumnIndex_)) {
        UpdateCurInsertPos(dragPosRowIndex_, dragPosColumnIndex_);
    }
}

void RenderGridLayout::FakeRemoveDragItem()
{
    dragPosRowIndex_ = -1;
    dragPosColumnIndex_ = -1;

    int32_t endRow = -1;
    int32_t endColum = -1;
    if (CalTheFirstEmptyCell(endRow, endColum, true)) {
        GetPreviousGird(endRow, endColum);
        int32_t startRow = curInsertRowIndex_;
        int32_t startColum = curInsertColumnIndex_;
        GetNextGrid(startRow, startColum);
        if (MoveItemsBackward(startRow, startColum, endRow, endColum)) {
            curInsertRowIndex_ = -1;
            curInsertColumnIndex_ = -1;
            UpdateMatrixByIndexStrong(-1, endRow, endColum);
        }
    }
}

bool RenderGridLayout::MoveItemsForward(int32_t fromRow, int32_t fromColum, int32_t toRow, int32_t toColum)
{
    bool vaild = false;
    if (!SortCellIndex(fromRow, fromColum, toRow, toColum, vaild) && (!vaild)) {
        return false;
    }

    int32_t curRow = toRow;
    int32_t curColum = toColum;
    int32_t targetRow = toRow;
    int32_t targetColum = toColum;
    int32_t tmpIndex = -1;

    bool equal = false;
    equal = SortCellIndex(fromRow, fromColum, curRow, curColum, vaild);

    while (vaild || equal) {
        // Get target pos
        GetNextGrid(targetRow, targetColum);

        // Get index in the curpos
        tmpIndex = GetIndexByGrid(curRow, curColum);
        if (tmpIndex < 0) {
            return false;
        }

        // Move the curpos index to the targetpos
        UpdateMatrixByIndexStrong(tmpIndex, targetRow, targetColum);

        if ((int32_t)itemsInGrid_.size() <= tmpIndex) {
            return false;
        }

        auto item = itemsInGrid_[tmpIndex];

        // Update the layout and position of the item be moved
        item->Layout(MakeInnerLayoutParam(targetRow, targetColum, 1, 1));
        SetChildPosition(item, targetRow, targetColum, 1, 1);

        // move the curpos backward
        GetPreviousGird(curRow, curColum);
        targetRow = curRow;
        targetColum = curColum;

        equal = SortCellIndex(fromRow, fromColum, curRow, curColum, vaild);
    }
    return true;
}

bool RenderGridLayout::MoveItemsBackward(int32_t fromRow, int32_t fromColum, int32_t toRow, int32_t toColum)
{
    bool vaild = false;
    if (!SortCellIndex(fromRow, fromColum, toRow, toColum, vaild) && (!vaild)) {
        return false;
    }

    int32_t curRow = fromRow;
    int32_t curColum = fromColum;
    int32_t targetRow = fromRow;
    int32_t targetColum = fromColum;
    int32_t tmpIndex = -1;

    bool equal = false;
    equal = SortCellIndex(curRow, curColum, toRow, toColum, vaild);

    while (vaild || equal) {
        // Get target pos
        GetPreviousGird(targetRow, targetColum);

        // Get index in the curpos
        tmpIndex = GetIndexByGrid(curRow, curColum);
        if (tmpIndex < 0) {
            return false;
        }

        // Move the curpos index to the targetpos
        UpdateMatrixByIndexStrong(tmpIndex, targetRow, targetColum);

        if ((int32_t)itemsInGrid_.size() <= tmpIndex) {
            return false;
        }

        auto item = itemsInGrid_[tmpIndex];

        // Update the layout and position of the item be moved
        item->Layout(MakeInnerLayoutParam(targetRow, targetColum, 1, 1));
        SetChildPosition(item, targetRow, targetColum, 1, 1);

        // move the curpos and targetpos backward
        GetNextGrid(curRow, curColum);
        targetRow = curRow;
        targetColum = curColum;

        equal = SortCellIndex(curRow, curColum, toRow, toColum, vaild);
    }

    return true;
}

void RenderGridLayout::UpdateMatrixByIndexStrong(int32_t index, int32_t row, int32_t colum)
{
    std::map<int32_t, int32_t> rowMap;

    auto rowIter = gridMatrix_.find(row);
    if (rowIter != gridMatrix_.end()) {
        rowMap = rowIter->second;
    }

    auto indexIter = rowMap.find(colum);
    if (indexIter != rowMap.end()) {
        rowMap[colum] = index;
    } else {
        rowMap.emplace(std::make_pair(colum, index));
    }

    gridMatrix_[row] = rowMap;
}

void RenderGridLayout::UpdateCurInsertPos(int32_t curInsertRow, int32_t curInsertColum)
{
    curInsertRowIndex_ = curInsertRow;
    curInsertColumnIndex_ = curInsertColum;
    UpdateMatrixByIndexStrong(-2, curInsertRowIndex_, curInsertColumnIndex_);
}

int32_t RenderGridLayout::CalIndexForItemByRowAndColum(int32_t row, int32_t colum)
{
    int32_t curRow = 0;
    int32_t curColum = 0;
    int32_t targetIndex = 0;
    if (row >= 0 && row < rowCount_ && colum >= 0 && colum < colCount_) {
        while (curRow != row || curColum != colum) {
            GetNextGrid(curRow, curColum);
            if (curRow >= rowCount_ || curColum >= colCount_) {
                targetIndex = -1;
                break;
            }
            targetIndex++;
        }
    } else {
        targetIndex = -1;
    }
    return targetIndex;
}

bool RenderGridLayout::SortCellIndex(
    int32_t rowFirst, int32_t columFirst, int32_t rowSecond, int32_t columSecond, bool& firstIsPre)
{
    if ((rowFirst == rowSecond) && (columFirst == columSecond)) {
        firstIsPre = false;
        return true;
    }

    if (isVertical_) {
        if (rowFirst > rowSecond) {
            firstIsPre = false;
        } else if (rowFirst == rowSecond) {
            if (columFirst > columSecond) {
                firstIsPre = false;
            } else {
                firstIsPre = true;
            }
        } else {
            firstIsPre = true;
        }
    } else {
        if (columFirst > columSecond) {
            firstIsPre = false;
        } else if (columFirst == columSecond) {
            if (rowFirst > rowSecond) {
                firstIsPre = false;
            } else {
                firstIsPre = true;
            }
        } else {
            firstIsPre = true;
        }
    }
    return false;
}

bool RenderGridLayout::CalTheFirstEmptyCell(int32_t& rowIndex, int32_t& columIndex, bool ignoreInsert)
{
    int32_t row = 0;
    int32_t colum = 0;
    int32_t index = -3;

    index = GetIndexByGrid(row, colum);

    while ((-1 != index) && (ignoreInsert || (-2 != index))) {
        GetNextGrid(row, colum);
        if (row >= rowCount_ || colum >= colCount_) {
            return false;
        }
        index = GetIndexByGrid(row, colum);
    }

    rowIndex = row;
    columIndex = colum;
    return true;
}

} // namespace OHOS::Ace
