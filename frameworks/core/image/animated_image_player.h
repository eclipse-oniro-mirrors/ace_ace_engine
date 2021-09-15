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

#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_IMAGE_ANIMATED_IMAGE_PLAYER_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_IMAGE_ANIMATED_IMAGE_PLAYER_H

#include "flutter/fml/memory/ref_counted.h"
#include "flutter/lib/ui/painting/image.h"
#include "third_party/skia/include/codec/SkCodec.h"

#include "base/memory/ace_type.h"
#include "core/animation/animator.h"
#include "core/animation/picture_animation.h"
#include "core/image/image_source_info.h"
#include "core/image/image_provider.h"
#include "core/pipeline/pipeline_context.h"

namespace OHOS::Ace {

class AnimatedImagePlayer : public virtual AceType {
    DECLARE_ACE_TYPE(AnimatedImagePlayer, AceType);

public:
    AnimatedImagePlayer(
        ImageSourceInfo source,
        PaintSuccessCallback successCallback,
        const WeakPtr<PipelineContext>& weakContext,
        const fml::WeakPtr<flutter::IOManager>& ioManager,
        const fml::RefPtr<flutter::SkiaUnrefQueue>& gpuQueue,
        std::unique_ptr<SkCodec> codec,
        int32_t dstWidth = -1,
        int32_t dstHeight = -1)
        : imageSource_(source), successCallback_(successCallback), context_(weakContext), ioManager_(ioManager),
          unrefQueue_(gpuQueue), codec_(std::move(codec)), frameCount_(codec_->getFrameCount()),
          repetitionCount_(codec_->getRepetitionCount()), frameInfos_(codec_->getFrameInfo()),
          dstWidth_(dstWidth), dstHeight_(dstHeight)
    {
        LOGD("animated image frameCount_ : %{public}d, repetitionCount_ : %{public}d", frameCount_, repetitionCount_);
        auto context = context_.Upgrade();
        if (context) {
            animator_ = AceType::MakeRefPtr<Animator>(context);
            auto pictureAnimation = AceType::MakeRefPtr<PictureAnimation<int32_t>>();
            float totalFrameDuration = 0.0f;
            for (int32_t index = 0; index < frameCount_; index++) {
                LOGD("frame[%{public}d] duration is %{public}d", index, frameInfos_[index].fDuration);
                // if frame duration is 0, set this frame duration as 100ms
                if (frameInfos_[index].fDuration <= 0) {
                    frameInfos_[index].fDuration = 100;
                }
                totalFrameDuration += frameInfos_[index].fDuration;
            }
            LOGD("animatied image total duration: %{public}f", totalFrameDuration);
            for (int32_t index = 0; index < frameCount_; index++) {
                pictureAnimation->AddPicture(
                    static_cast<float>(frameInfos_[index].fDuration) / totalFrameDuration, index);
            }
            pictureAnimation->AddListener([weak = WeakClaim(this)](const int32_t& index) {
                auto player = weak.Upgrade();
                if (player) {
                    player->RenderFrame(index);
                }
            });
            animator_->AddInterpolator(pictureAnimation);
            animator_->SetDuration(totalFrameDuration);
            animator_->SetIteration(
                repetitionCount_ >= 0 ? repetitionCount_ + 1 : ANIMATION_REPEAT_INFINITE);
            animator_->Play();
        }
    }

    ~AnimatedImagePlayer() override = default;

    void Pause();
    void Resume();
    void RenderFrame(const int32_t& index);

private:
    sk_sp<SkImage> DecodeFrameImage(const int32_t& index);
    static bool CopyTo(SkBitmap* dst, SkColorType dstColorType, const SkBitmap& src);

    ImageSourceInfo imageSource_;
    PaintSuccessCallback successCallback_;
    WeakPtr<PipelineContext> context_;
    fml::WeakPtr<flutter::IOManager> ioManager_;
    fml::RefPtr<flutter::SkiaUnrefQueue> unrefQueue_;
    const std::unique_ptr<SkCodec> codec_;
    const int32_t frameCount_;
    const int32_t repetitionCount_;
    std::vector<SkCodec::FrameInfo> frameInfos_;
    std::unique_ptr<SkBitmap> lastRequiredBitmap_;
    int32_t requiredFrameIndex_ = -1;
    RefPtr<Animator> animator_;
    int32_t dstWidth_ = -1;
    int32_t dstHeight_ = -1;
};

} // namespace OHOS::Ace

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_IMAGE_ANIMATED_IMAGE_PLAYER_H
