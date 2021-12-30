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

#include "core/pipeline/base/rosen_render_context.h"

#include "render_service_client/core/ui/rs_node.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace OHOS::Ace {
namespace {

inline bool ShouldPaint(const RefPtr<RenderNode>& node)
{
    return node != nullptr && node->GetVisible() && !node->GetHidden();
}

} // namespace

RosenRenderContext::~RosenRenderContext()
{
    StopRecordingIfNeeded();
}

void RosenRenderContext::Repaint(const RefPtr<RenderNode>& node)
{
    if (!ShouldPaint(node) || !node->NeedRender() || node->GetRSNode() == nullptr) {
        LOGD("Node is not need to paint");
        return;
    }

    auto rsNode = node->GetRSNode();
    auto offset =
        node->GetTransitionPaintRect().GetOffset() -
        Offset(rsNode->GetStagingProperties().GetFramePositionX(), rsNode->GetStagingProperties().GetFramePositionY());
    InitContext(rsNode, node->GetRectWithShadow(), offset);
    node->RenderWithContext(*this, offset);
}

void RosenRenderContext::PaintChild(const RefPtr<RenderNode>& child, const Offset& offset)
{
    if (!ShouldPaint(child)) {
        LOGD("Node is not need to paint");
        return;
    }

    Rect rect = child->GetTransitionPaintRect() + offset;
    if (!child->IsPaintOutOfParent() && !estimatedRect_.IsIntersectWith(rect)) {
#if defined(WINDOWS_PLATFORM) || defined(MAC_PLATFORM)
        child->ClearAccessibilityRect();
#endif
        return;
    }

    auto childRSNode = child->GetRSNode();
    if (childRSNode && childRSNode != rsNode_) {
        rsNode_->AddChild(childRSNode, -1);
        if (child->NeedRender()) {
            RosenRenderContext context;
            auto pipelineContext = child->GetContext().Upgrade();
            LOGI("Hole: child canvas render");
            auto transparentHole = pipelineContext->GetTransparentHole();
            if (transparentHole.IsValid() && child->GetNeedClip()) {
                Offset childOffset = rect.GetOffset();
                Rect hole = transparentHole - childOffset;
                context.SetClipHole(hole);
            }
            context.Repaint(child);
        } else {
            // No need to repaint, notify to update AccessibilityNode info.
            child->NotifyPaintFinish();
        }
    } else {
        child->RenderWithContext(*this, rect.GetOffset());
    }
}

void RosenRenderContext::StartRecording()
{
    recorder_ = new SkPictureRecorder();
    recordingCanvas_ = recorder_->beginRecording(
        SkRect::MakeXYWH(estimatedRect_.Left(), estimatedRect_.Top(), estimatedRect_.Width(), estimatedRect_.Height()));
    if (clipHole_.IsValid()) {
        recordingCanvas_->save();
        needRestoreHole_ = true;
        recordingCanvas_->clipRect(SkRect::MakeXYWH(clipHole_.Left(), clipHole_.Top(),
            clipHole_.Right(), clipHole_.Bottom()), SkClipOp::kDifference, true);
    }
}

void RosenRenderContext::StopRecordingIfNeeded()
{
    if (!IsRecording()) {
        return;
    }

    if (needRestoreHole_) {
        recordingCanvas_->restore();
        needRestoreHole_ = false;
    }

    delete recorder_;
    recorder_ = nullptr;
    recordingCanvas_ = nullptr;
}

bool RosenRenderContext::IsIntersectWith(const RefPtr<RenderNode>& child, Offset& offset)
{
    if (!ShouldPaint(child)) {
        LOGD("Node is not need to paint");
        return false;
    }

    Rect rect = child->GetTransitionPaintRect() + offset;
    if (!estimatedRect_.IsIntersectWith(rect)) {
#if defined(WINDOWS_PLATFORM) || defined(MAC_PLATFORM)
        child->ClearAccessibilityRect();
#endif
        return false;
    }

    offset = rect.GetOffset();
    return true;
}

void RosenRenderContext::InitContext(
    const std::shared_ptr<RSNode>& rsNode, const Rect& rect, const Offset& initialOffset)
{
    LOGD("InitContext with width %{public}lf height %{public}lf", rect.Width(), rect.Height());
    rsNode_ = rsNode;
    estimatedRect_ = rect + initialOffset;
    if (rsNode_) {
        rsNode_->ClearChildren();
        rosenCanvas_ = rsNode_->BeginRecording(
            rsNode_->GetStagingProperties().GetFrameWidth(), rsNode_->GetStagingProperties().GetFrameHeight());
    }
}

SkCanvas* RosenRenderContext::GetCanvas()
{
    if (recordingCanvas_) {
        // if recording, return recording canvas
        return recordingCanvas_;
    }
    return rosenCanvas_;
}

const std::shared_ptr<RSNode>& RosenRenderContext::GetRSNode()
{
    return rsNode_;
}

sk_sp<SkPicture> RosenRenderContext::FinishRecordingAsPicture()
{
    if (!recorder_) {
        return nullptr;
    }
    return recorder_->finishRecordingAsPicture();
}

sk_sp<SkImage> RosenRenderContext::FinishRecordingAsImage()
{
    if (!recorder_) {
    return nullptr;
    }
    auto picture = recorder_->finishRecordingAsPicture();
    if (!picture) {
        return nullptr;
    }
    auto image = SkImage::MakeFromPicture(picture, { estimatedRect_.Width(), estimatedRect_.Height() }, nullptr,
        nullptr, SkImage::BitDepth::kU8, nullptr);
    return image;
}

void RosenRenderContext::Restore()
{
    auto canvas = GetCanvas();
    if (canvas != nullptr) {
        canvas->restore();
    }
}

} // namespace OHOS::Ace
