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

#ifndef FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_V2_INSPECTOR_INSPECTOR_CONSTANTS_H
#define FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_V2_INSPECTOR_INSPECTOR_CONSTANTS_H

#include <cstdint>

#include "base/utils/macros.h"

namespace OHOS::Ace::V2 {

ACE_EXPORT extern const char ATTRS_COMMON_WIDTH[];
ACE_EXPORT extern const char ATTRS_COMMON_HEIGHT[];
ACE_EXPORT extern const char ATTRS_COMMON_PADDING[];
ACE_EXPORT extern const char ATTRS_COMMON_MARGIN[];
ACE_EXPORT extern const char ATTRS_COMMON_LAYOUT_PRIORITY[];
ACE_EXPORT extern const char ATTRS_COMMON_LAYOUT_WEIGHT[];
// position
ACE_EXPORT extern const char ATTRS_COMMON_ALIGN[];
ACE_EXPORT extern const char ATTRS_COMMON_DIRECTION[];
ACE_EXPORT extern const char ATTRS_COMMON_POSITION[];
ACE_EXPORT extern const char ATTRS_COMMON_OFFSET[];
ACE_EXPORT extern const char ATTRS_COMMON_USE_ALIGN[];
// layout
ACE_EXPORT extern const char ATTRS_COMMON_RELATE_PARENT[];
ACE_EXPORT extern const char ATTRS_COMMON_ASPECT_RATIO[];
ACE_EXPORT extern const char ATTRS_COMMON_DISPLAY_PRIORITY[];
// border
ACE_EXPORT extern const char ATTRS_COMMON_BORDER[];
// background
ACE_EXPORT extern const char ATTRS_COMMON_BACKGROUND_COLOR[];
// opacity
ACE_EXPORT extern const char ATTRS_COMMON_OPACITY[];
// visibility
ACE_EXPORT extern const char ATTRS_COMMON_VISIBILITY[];
// enable
ACE_EXPORT extern const char ATTRS_COMMON_ENABLE[];
// zindex
ACE_EXPORT extern const char ATTRS_COMMON_ZINDEX[];

// column
ACE_EXPORT extern const char COLUMN_COMPONENT_TAG[];
ACE_EXPORT extern const char COLUMN_ETS_TAG[];
ACE_EXPORT extern const char COLUMN_ATTRS_ALIGN_ITEMS[];

// text
ACE_EXPORT extern const char TEXT_COMPONENT_TAG[];
ACE_EXPORT extern const char TEXT_ETS_TAG[];
ACE_EXPORT extern const char TEXT_ATTRS_OVER_FLOW[];
ACE_EXPORT extern const char TEXT_ATTRS_MAX_LINES[];

// stack
ACE_EXPORT extern const char STACK_COMPONENT_TAG[];
ACE_EXPORT extern const char STACK_ETS_TAG[];

// swiper
ACE_EXPORT extern const char SWIPER_COMPONENT_TAG[];
ACE_EXPORT extern const char SWIPER_ETS_TAG[];

// tabs
ACE_EXPORT extern const char TABS_COMPONENT_TAG[];
ACE_EXPORT extern const char TABS_ETS_TAG[];

// tab content item
ACE_EXPORT extern const char TAB_CONTENT_ITEM_COMPONENT_TAG[];
ACE_EXPORT extern const char TAB_CONTENT_ITEM_ETS_TAG[];

// navigation view
ACE_EXPORT extern const char NAVIGATION_VIEW_COMPONENT_TAG[];
ACE_EXPORT extern const char NAVIGATION_VIEW_ETS_TAG[];

// row split
ACE_EXPORT extern const char ROW_SPLIT_COMPONENT_TAG[];
ACE_EXPORT extern const char ROW_SPLIT_ETS_TAG[];

// column split
ACE_EXPORT extern const char COLUMN_SPLIT_COMPONENT_TAG[];
ACE_EXPORT extern const char COLUMN_SPLIT_ETS_TAG[];

// counter
ACE_EXPORT extern const char COUNTER_COMPONENT_TAG[];
ACE_EXPORT extern const char COUNTER_ETS_TAG[];

// flex
ACE_EXPORT extern const char FLEX_COMPONENT_TAG[];
ACE_EXPORT extern const char FLEX_ETS_TAG[];

// grid
ACE_EXPORT extern const char GRID_COMPONENT_TAG[];
ACE_EXPORT extern const char GRID_ETS_TAG[];

// grid-item
ACE_EXPORT extern const char GRID_ITEM_COMPONENT_TAG[];
ACE_EXPORT extern const char GRID_ITEM_ETS_TAG[];

// list
ACE_EXPORT extern const char LIST_COMPONENT_TAG[];
ACE_EXPORT extern const char LIST_ETS_TAG[];

// list-item
ACE_EXPORT extern const char LIST_ITEM_COMPONENT_TAG[];
ACE_EXPORT extern const char LIST_ITEM_ETS_TAG[];

// navigator
ACE_EXPORT extern const char NAVIGATOR_COMPONENT_TAG[];
ACE_EXPORT extern const char NAVIGATOR_ETS_TAG[];

// panel
ACE_EXPORT extern const char PANEL_COMPONENT_TAG[];
ACE_EXPORT extern const char PANEL_ETS_TAG[];

// row
ACE_EXPORT extern const char ROW_COMPONENT_TAG[];
ACE_EXPORT extern const char ROW_ETS_TAG[];

// shape
ACE_EXPORT extern const char SHAPE_COMPONENT_TAG[];
ACE_EXPORT extern const char SHAPE_ETS_TAG[];

// shape container
ACE_EXPORT extern const char SHAPE_CONTAINER_COMPONENT_TAG[];
ACE_EXPORT extern const char SHAPE_CONTAINER_ETS_TAG[];

// imageAnimator
ACE_EXPORT extern const char IMAGE_ANIMATOR_COMPONENT_TAG[];
ACE_EXPORT extern const char IMAGE_ANIMATOR_ETS_TAG[];

// image
ACE_EXPORT extern const char IMAGE_COMPONENT_TAG[];
ACE_EXPORT extern const char IMAGE_ETS_TAG[];

// qrcode
ACE_EXPORT extern const char QRCODE_COMPONENT_TAG[];
ACE_EXPORT extern const char QRCODE_ETS_TAG[];

// span
ACE_EXPORT extern const char SPAN_COMPONENT_TAG[];
ACE_EXPORT extern const char SPAN_ETS_TAG[];

// text
ACE_EXPORT extern const char TEXT_COMPONENT_TAG[];
ACE_EXPORT extern const char TEXT_ETS_TAG[];

// blank
ACE_EXPORT extern const char BOX_COMPONENT_TAG[];
ACE_EXPORT extern const char BLANK_ETS_TAG[];

// button
ACE_EXPORT extern const char BUTTON_COMPONENT_TAG[];
ACE_EXPORT extern const char BUTTON_ETS_TAG[];

// divider
ACE_EXPORT extern const char DIVIDER_COMPONENT_TAG[];
ACE_EXPORT extern const char DIVIDER_ETS_TAG[];

// checkbox
ACE_EXPORT extern const char CHECKBOX_COMPONENT_TAG[];
ACE_EXPORT extern const char CHECKBOX_ETS_TAG[];

// switch
ACE_EXPORT extern const char SWITCH_COMPONENT_TAG[];
ACE_EXPORT extern const char SWITCH_ETS_TAG[];

// toggle
ACE_EXPORT extern const char TOGGLE_COMPONENT_TAG[];
ACE_EXPORT extern const char TOGGLE_ETS_TAG[];

// scroll
ACE_EXPORT extern const char SCROLL_COMPONENT_TAG[];
ACE_EXPORT extern const char SCROLL_ETS_TAG[];

// calendar
ACE_EXPORT extern const char CALENDAR_COMPONENT_TAG[];
ACE_EXPORT extern const char CALENDAR_ETS_TAG[];

// badge
ACE_EXPORT extern const char BADGE_COMPONENT_TAG[];
ACE_EXPORT extern const char BADGE_ETS_TAG[];

} // namespace OHOS::Ace::V2

#endif // FOUNDATION_ACE_FRAMEWORKS_CORE_COMPONENTS_V2_INSPECTOR_INSPECTOR_CONSTANTS_H
