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

#include "core/components_v2/indexer/render_indexer.h"

#include "base/log/event_report.h"
#include "core/components/arc/render_arc.h"
#include "core/components/text/render_text.h"

namespace OHOS::Ace::V2 {
namespace {
constexpr Dimension FOCUS_PADDING = 2.0_vp;
} // namespace

std::string IndexerEventInfo::ToJSONString() const
{
    return std::string("\"indexer\",{\"selectedIndex\":")
                .append(std::to_string(selectedIndex_))
                .append("},null");
}

RefPtr<RenderNode> RenderIndexer::Create()
{
    LOGI("[indexer] Create RenderIndexer");
    return AceType::MakeRefPtr<RenderIndexer>();
}

RenderIndexer::RenderIndexer()
{
    LOGI("[indexer] touchRecognizer_ init");
    touchRecognizer_ = AceType::MakeRefPtr<RawRecognizer>();
    touchRecognizer_->SetOnTouchDown([wp = AceType::WeakClaim(this)](const TouchEventInfo& info) {
        auto sp = wp.Upgrade();
        if (sp) {
            sp->HandleTouchDown(info);
        }
    });
    touchRecognizer_->SetOnTouchUp([wp = AceType::WeakClaim(this)](const TouchEventInfo& info) {
        auto sp = wp.Upgrade();
        if (sp) {
            sp->HandleTouchUp(info);
        }
    });
    touchRecognizer_->SetOnTouchMove([wp = AceType::WeakClaim(this)](const TouchEventInfo& info) {
        auto sp = wp.Upgrade();
        if (sp) {
            sp->HandleTouchMove(info);
        }
    });

    // register this listener for consuming the drag events.
    dragRecognizer_ = AceType::MakeRefPtr<VerticalDragRecognizer>();
    dragRecognizer_->SetOnDragStart([weakIndex = AceType::WeakClaim(this)](const DragStartInfo& info) {
        auto indexer = weakIndex.Upgrade();
        if (indexer) {
            indexer->HandleDragStart();
        }
    });
    dragRecognizer_->SetOnDragEnd([weakIndex = AceType::WeakClaim(this)](const DragEndInfo& info) {
        auto indexer = weakIndex.Upgrade();
        if (indexer) {
            indexer->HandleDragEnd();
        }
    });
}

void RenderIndexer::PerformLayout()
{
    UpdateItems();
    if (itemCount_ <= 0) {
        SetLayoutSize(Size());
        return;
    }

    // calculate self and children size,
    if (NearZero(itemSize_)) {
        LOGE("[indexer] Invalid Item size:%{public}lf", itemSize_);
        return;
    }

    // calculate the size of the items
    const LayoutParam& layoutSetByParent = GetLayoutParam();
    Size sizeMax = layoutSetByParent.GetMaxSize();
    if (LessOrEqual(paddingY_ * DOUBLE + itemCount_ * itemSize_, sizeMax.Height())) {
        itemSizeRender_ = itemSize_;
    } else {
        itemSizeRender_ = (sizeMax.Height() - paddingY_ * DOUBLE) / itemCount_;
    }

    InitFocusedItem();

    LayoutParam childrenLayout;
    childrenLayout.SetMinSize(Size(0.0, 0.0));

    for (const auto& item : GetChildren()) {
        item->Layout(childrenLayout);
    }
    // then set the position of children
    Offset position;
    int32_t count = 0;
    for (const auto& item : GetChildren()) {
        if (!AceType::InstanceOf<RenderIndexerItem>(item)) {
            continue;
        }
        position.SetX(paddingX_);
        position.SetY(paddingY_ + count * itemSizeRender_);
        item->SetPosition(position);
        count++;
    }

    double indexerWidth = paddingX_ + itemSizeRender_ + paddingX_;
    double indexerHeight = paddingY_ + count * itemSizeRender_ + paddingY_;
    SetLayoutSize(Size(indexerWidth, indexerHeight));

    if (IsValidBubbleBox()) {
        Offset bubblePosition;
        if (alignStyle_ == AlignStyle::RIGHT) {
            bubblePosition.SetX(NormalizeToPx(-BUBBLE_POSITION_X) - bubbleDisplay_->GetLayoutSize().Width());
            bubblePosition.SetY(NormalizeToPx(BUBBLE_POSITION_Y));
        } else {
            bubblePosition.SetX(NormalizeToPx(BUBBLE_POSITION_X) + GetLayoutSize().Width());
            bubblePosition.SetY(NormalizeToPx(BUBBLE_POSITION_Y));
        }

        bubbleDisplay_->SetPosition(bubblePosition);
    }
}

void RenderIndexer::Update(const RefPtr<Component>& component)
{
    LOGE("[indexer] Update RenderIndexer");
    RefPtr<IndexerComponent> indexerComponent = AceType::DynamicCast<IndexerComponent>(component);
    if (!indexerComponent) {
        LOGE("[indexer] Update Get component failed");
        EventReport::SendRenderException(RenderExcepType::RENDER_COMPONENT_ERR);
        return;
    }
    nonItemCount_ = indexerComponent->GetNonItemCount();
    bubbleEnabled_ = indexerComponent->IsBubbleEnabled();
    bubbleText_ = indexerComponent->GetBubbleTextComponent();
    focusedItem_ = indexerComponent->GetSelectedIndex();
    alignStyle_ = indexerComponent->GetAlignStyle();

    // update item information
    auto context = GetContext().Upgrade();
    if (context) {
        itemSize_ = context->NormalizeToPx(indexerComponent->GetItemSize());
    }
    itemSizeRender_ = itemSize_;
    itemCount_ = indexerComponent->GetItemCount();
    LOGI("[indexer] Init data, itemSizeRender_:%{public}lf, itemCount_:%{public}d", itemSizeRender_, itemCount_);
    MarkNeedLayout();
    selectedEventFun_ = AceAsyncEvent<void(const std::shared_ptr<IndexerEventInfo>&)>::
        Create(indexerComponent->GetSelectedEvent(), context_);
}


void RenderIndexer::UpdateItems()
{
    if (nonItemCount_ + items_.size() == GetChildren().size()) {
        LOGD("[indexer] no need update Items");
        return;
    }
    items_.clear();
    for (auto item : GetChildren()) {
        if (AceType::InstanceOf<RenderIndexerItem>(item)) {
            items_.push_back(item);
        }
    }
    LOGD("[indexer] items nums : %{public}d", static_cast<int32_t>(items_.size()));
}

void RenderIndexer::InitFocusedItem()
{
    for (auto item : items_) {
        RefPtr<RenderIndexerItem> indexerItem = AceType::DynamicCast<RenderIndexerItem>(item);
        if (indexerItem) {
            indexerItem->SetClicked(false);
        }
    }

    if (focusedItem_ >= 0) {
        auto item = GetSpecificItem(focusedItem_);
        if (item) {
            item->SetClicked(true);
        }
    }
}

void RenderIndexer::HandleTouchDown(const TouchEventInfo& info)
{
    if (info.GetTouches().empty()) {
        return;
    }
    touchPostion_ = info.GetTouches().front().GetLocalLocation();
    LOGD("[indexer] item is HandleTouchDown x:%{public}lf, y:%{public}lf", touchPostion_.GetX(), touchPostion_.GetY());
    HandleTouched(touchPostion_);
    clicked_ = true;
    MarkNeedLayout();
}

void RenderIndexer::HandleTouchUp(const TouchEventInfo& info)
{
    if (!info.GetTouches().empty()) {
        touchPostion_ = info.GetTouches().front().GetLocalLocation();
    }
    LOGD("[indexer] item is HandleTouchUp x:%{public}lf, y:%{public}lf", touchPostion_.GetX(), touchPostion_.GetY());
    HandleTouched(touchPostion_);
    if (clicked_) {
        clicked_ = false;
        MarkNeedLayout();
    }
}

void RenderIndexer::HandleTouchMove(const TouchEventInfo& info)
{
    if (info.GetTouches().empty()) {
        return;
    }
    touchPostion_ = info.GetTouches().front().GetLocalLocation();
    LOGD("[indexer] item is HandleTouchMove x:%{public}lf, y:%{public}lf", touchPostion_.GetX(), touchPostion_.GetY());
    HandleTouched(touchPostion_);
    if (clicked_) {
        clicked_ = true;
        MarkNeedLayout();
    }
}

void RenderIndexer::HandleDragStart()
{
    LOGD("[indexer] HandleDragstart");
}

void RenderIndexer::HandleDragEnd()
{
    LOGD("[indexer] HandleDragEnd");
}

bool RenderIndexer::TouchTest(const Point& globalPoint, const Point& parentLocalPoint,
    const TouchRestrict& touchRestrict, TouchTestResult& result)
{
    if (GetDisableTouchEvent() || disabled_) {
        return false;
    }
    auto focusedNode = GetSpecificItem(focusedItem_);
    if (focusedNode) {
        focusedNode->SetFocused(false);
    }
    // Since the paintRect is relative to parent, use parent local point to perform touch test.
    if (GetPaintRect().IsInRegion(parentLocalPoint)) {
        // Calculates the local point location and coordinate offset in this node.
        const auto localPoint = parentLocalPoint - GetPaintRect().GetOffset();
        const auto coordinateOffset = globalPoint - localPoint;
        globalPoint_ = globalPoint;
        OnTouchTestHit(coordinateOffset, touchRestrict, result);
        return true;
    } else {
        if (IsValidBubbleBox() && bubbleController_ && bubbleController_->IsRunning()) {
            bubbleController_->Finish();
        }
    }
    return false;
}

void RenderIndexer::OnTouchTestHit(
    const Offset& coordinateOffset, const TouchRestrict& touchRestrict, TouchTestResult& result)
{
    if (dragRecognizer_) {
        dragRecognizer_->SetCoordinateOffset(coordinateOffset);
        result.emplace_back(dragRecognizer_);
    }

    if (touchRecognizer_) {
        touchRecognizer_->SetCoordinateOffset(coordinateOffset);
        result.emplace_back(touchRecognizer_);
    }
}

void RenderIndexer::HandleTouched(const Offset& touchPosition)
{
    int32_t index = GetTouchedItemIndex(touchPosition);
    if (index > -1) {
        MoveSectionWithIndexer(index);
    }
}

int32_t RenderIndexer::GetTouchedItemIndex(const Offset& touchPosition)
{
    double position = touchPosition.GetY();
    if (position < paddingY_) {
        return -1;
    }

    if (NearZero(itemSizeRender_)) {
        LOGE("[indexer] Invalid Item size:%{public}lf", itemSizeRender_);
        return -1;
    }

    int32_t index = static_cast<int32_t>((position - paddingY_) / itemSizeRender_);
    LOGI("[indexer] HandleTouched section index:%{public}d", index);
    return GetItemIndex(index);
}

void RenderIndexer::UpdateBubbleText()
{
    if (IsValidBubbleBox() && !bubbleBox_->GetChildren().empty()) {
        auto text = AceType::DynamicCast<RenderText>(bubbleBox_->GetChildren().front());
        auto item = GetSpecificItem(focusedItem_);
        if (bubbleText_ && text && item) {
            bubbleText_->SetData(item->GetSectionText());
            text->Update(bubbleText_);
            text->PerformLayout();
            BeginBubbleAnimation();
        }
    }
}

void RenderIndexer::BuildBubbleAnimation()
{
    if (!IsValidBubbleBox()) {
        LOGD("bubble box is invalid");
        return;
    }
    if (!bubbleController_) {
        bubbleController_ = AceType::MakeRefPtr<Animator>(GetContext());
    }
    bubbleController_->ClearInterpolators();
    bubbleController_->ClearAllListeners();
    auto weak = AceType::WeakClaim(this);
    bubbleController_->AddStopListener([weak]() {
        auto indexer = weak.Upgrade();
        if (indexer && indexer->IsValidBubbleBox()) {
            indexer->bubbleDisplay_->UpdateOpacity(ZERO_OPACITY);
        }
    });

    // build and start animation
    auto animation = AceType::MakeRefPtr<KeyframeAnimation<uint8_t>>();
    auto startFrame = AceType::MakeRefPtr<Keyframe<uint8_t>>(KEYFRAME_BEGIN, ZERO_OPACITY);
    auto midFrame = AceType::MakeRefPtr<Keyframe<uint8_t>>(KEYFRAME_HALF, DEFAULT_OPACITY);
    auto endFrame = AceType::MakeRefPtr<Keyframe<uint8_t>>(KEYFRAME_END, ZERO_OPACITY);
    midFrame->SetCurve(Curves::DECELE);
    endFrame->SetCurve(Curves::DECELE);
    animation->AddKeyframe(startFrame);
    animation->AddKeyframe(midFrame);
    animation->AddKeyframe(endFrame);
    animation->AddListener([weak](uint8_t value) {
        auto indexer = weak.Upgrade();
        if (indexer && indexer->IsValidBubbleBox()) {
            indexer->bubbleDisplay_->UpdateOpacity(value);
        }
    });

    bubbleController_->AddInterpolator(animation);
    bubbleController_->SetDuration(INDEXER_BUBBLE_ANIMATION_DURATION);
}

void RenderIndexer::BeginBubbleAnimation()
{
    if (!IsValidBubbleBox()) {
        LOGD("bubble box is invalid");
        return;
    }
    if (!bubbleController_) {
        BuildBubbleAnimation();
    }
    if (bubbleController_->IsRunning()) {
        bubbleController_->Finish();
    }
    bubbleController_->Play();
}

bool RenderIndexer::IsValidBubbleBox()
{
    if (!bubbleEnabled_ || GetChildren().empty()) {
        return false;
    }
    if (!bubbleDisplay_) {
        bubbleDisplay_ = AceType::DynamicCast<RenderDisplay>(GetChildren().front());
        if (!bubbleDisplay_ || bubbleDisplay_->GetChildren().empty()) {
            return false;
        }
        bubbleBox_ = AceType::DynamicCast<RenderBox>(bubbleDisplay_->GetChildren().front());
        if (!bubbleBox_) {
            return false;
        }
    }
    return true;
}

int32_t RenderIndexer::GetItemIndex(int32_t index)
{
    if (items_.empty()) {
        return -1;
    }
    int32_t itemIndexInList = -1;
    RefPtr<RenderNode> itemNode;
    auto iter = items_.begin();
    std::advance(iter, index);
    if (iter == items_.end()) {
        itemNode = items_.back();
    } else {
        itemNode = *iter;
    }
    RefPtr<RenderIndexerItem> indexerItem = AceType::DynamicCast<RenderIndexerItem>(itemNode);
    if (indexerItem) {
        itemIndexInList = indexerItem->GetSectionIndex();
    }
    LOGI("[indexer] GetItemIndex index:%{public}d indexInList:%{public}d", index, itemIndexInList);
    return itemIndexInList;
}

void RenderIndexer::HandleFocusAnimation(const Size& size, const Offset& offset)
{
    auto context = context_.Upgrade();
    if (!context) {
        LOGE("Pipeline context upgrade fail!");
        return;
    }
    if (!context->GetRenderFocusAnimation()) {
        LOGE("focusAnimation is null!");
        return;
    }

    double focusPadding = NormalizeToPx(FOCUS_PADDING);
    context->ShowFocusAnimation(
        RRect::MakeRRect(Rect(Offset(), size - Size(focusPadding, focusPadding) * 2), focusPadding, focusPadding),
        Color::BLUE, offset + Offset(focusPadding, focusPadding));
}

void RenderIndexer::MoveSectionWithIndexer(int32_t curSection)
{
    if (focusedItem_ == curSection) {
        LOGD("Current focused item already is:%{public}d", curSection);
        return;
    }

    RefPtr<RenderIndexerItem> curItem = GetSpecificItem(curSection);
    if (!curItem || !NeedProcess(curItem)) {
        LOGW("[indexer] invalid indexer item");
        return;
    }

    // change to correct index
    BeginFocusAnimation(focusedItem_, curSection);
    curItem->SetClicked(true); // Make cur clicked item focus.
    UpdateCurrentSectionItem(curSection);

    // Make pre focused item blur.
    RefPtr<RenderIndexerItem> preItem = GetSpecificItem(focusedItem_);
    if (preItem) {
        preItem->SetClicked(false);
    }
    focusedItem_ = curSection;
    MoveList(curItem->GetSectionIndex());
    UpdateBubbleText();
}

void RenderIndexer::MoveList(int32_t index)
{
    if (index < 0) {
        LOGE("[indexer] invalid item indexer");
        return;
    }
    OnSelected(index);
}

RefPtr<RenderIndexerItem> RenderIndexer::GetSpecificItem(int32_t index) const
{
    auto iter = items_.begin();
    std::advance(iter, index);
    if (iter != items_.end()) {
        return AceType::DynamicCast<RenderIndexerItem>(*iter);
    }
    return nullptr;
}

void RenderIndexer::OnSelected(int32_t selected) const
{
    if (selectedEventFun_) {
        auto event = std::make_shared<IndexerEventInfo>(selected);
        if (event) {
            selectedEventFun_(event);
        }
    }
}
} // namespace OHOS::Ace
