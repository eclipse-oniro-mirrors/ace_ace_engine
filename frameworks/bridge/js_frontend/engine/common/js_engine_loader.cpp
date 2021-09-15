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

#include "frameworks/bridge/js_frontend/engine/common/js_engine_loader.h"

#include <dlfcn.h>

#include "base/log/log.h"
#include "base/utils/singleton.h"
#include "base/utils/system_properties.h"
#include "core/common/ace_application_info.h"

namespace OHOS::Ace::Framework {
namespace {

constexpr char JS_ENGINE_ENTRY[] = "OHOS_ACE_GetJsEngineLoader";

constexpr char QUICK_JS_ENGINE_SHARED_LIB[] = "libace_engine_qjs.z.so";
constexpr char QJS_DECLARATIVE_JS_ENGINE_SHARED_LIB[] = "libace_engine_declarative_qjs.z.so";
constexpr char ARK_JS_ENGINE_SHARED_LIB[] = "libace_engine_ark.z.so";
constexpr char ARK_DECLARATIVE_JS_ENGINE_SHARED_LIB[] = "libace_engine_declarative_ark.z.so";

using DynamicEntry = void* (*)();

class DummyJsEngineLoader final : public JsEngineLoader, public Singleton<DummyJsEngineLoader> {
    DECLARE_SINGLETON(DummyJsEngineLoader);

public:
    RefPtr<JsEngine> CreateJsEngine(int32_t instanceId) const override
    {
        return nullptr;
    }

    RefPtr<BaseCanvasBridge> CreateCanvasBridge() const override
    {
        return nullptr;
    }

    RefPtr<BaseXComponentBridge> CreateXComponentBridge() const override
    {
        return nullptr;
    }
};

DummyJsEngineLoader::DummyJsEngineLoader() = default;
DummyJsEngineLoader::~DummyJsEngineLoader() = default;

const char* GetSharedLibrary(bool isArkApp)
{
    if (isArkApp) {
        return ARK_JS_ENGINE_SHARED_LIB;
    } else {
        return QUICK_JS_ENGINE_SHARED_LIB;
    }
}

const char* GetDeclarativeSharedLibrary(bool isArkApp)
{
    if (isArkApp) {
        return ARK_DECLARATIVE_JS_ENGINE_SHARED_LIB;
    } else {
        return QJS_DECLARATIVE_JS_ENGINE_SHARED_LIB;
    }
}

JsEngineLoader& GetJsEngineLoader(const char* sharedLibrary)
{
    void* handle = dlopen(sharedLibrary, RTLD_LAZY);
    if (handle == nullptr) {
        LOGE("Failed to open shared library %{public}s, reason: %{public}sn", sharedLibrary, dlerror());
        return DummyJsEngineLoader::GetInstance();
    }

    auto entry = reinterpret_cast<DynamicEntry>(dlsym(handle, JS_ENGINE_ENTRY));
    if (entry == nullptr) {
        dlclose(handle);
        LOGE("Failed to get symbol %{public}s in %{public}s", JS_ENGINE_ENTRY, sharedLibrary);
        return DummyJsEngineLoader::GetInstance();
    }

    auto loader = reinterpret_cast<JsEngineLoader*>(entry());
    if (loader == nullptr) {
        dlclose(handle);
        LOGE("Failed to get js engine loader in %{public}s", sharedLibrary);
        return DummyJsEngineLoader::GetInstance();
    }

    return *loader;
}

} // namespace

JsEngineLoader& JsEngineLoader::Get(bool isArkApp)
{
    static JsEngineLoader& instance = GetJsEngineLoader(GetSharedLibrary(isArkApp));
    return instance;
}

JsEngineLoader& JsEngineLoader::GetDeclarative(bool isArkApp)
{
    static JsEngineLoader& instance = GetJsEngineLoader(GetDeclarativeSharedLibrary(isArkApp));
    return instance;
}

} // namespace OHOS::Ace::Framework
