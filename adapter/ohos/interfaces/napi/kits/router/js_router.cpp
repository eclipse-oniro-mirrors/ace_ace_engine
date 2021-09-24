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

#include <string>

#include "napi/native_api.h"
#include "napi/native_engine/native_value.h"
#include "napi/native_node_api.h"

#include "frameworks/base/log/log.h"
#include "frameworks/bridge/js_frontend/engine/common/js_engine.h"

namespace OHOS::Ace::Napi {

static void ParseUri(napi_env env, napi_value uriNApi, std::string& uriString)
{
    auto nativeUri = reinterpret_cast<NativeValue*>(uriNApi);
    auto resultUri = nativeUri->ToString();
    auto nativeStringUri = reinterpret_cast<NativeString*>(resultUri->GetInterface(NativeString::INTERFACE_ID));
    size_t uriLen = nativeStringUri->GetLength();
    std::unique_ptr<char[]> uri = std::make_unique<char[]>(uriLen);
    size_t retLen = 0;
    napi_get_value_string_utf8(env, uriNApi, uri.get(), uriLen, &retLen);
    uriString = uri.get();
}

static void ParseParams(napi_env env, napi_value params, napi_value dontOverwrite, std::string& paramsString)
{
    bool dontOverwriteBool = false;
    int32_t dontOverwriteNum = 0;
    if (dontOverwrite) {
        napi_get_value_bool(env, dontOverwrite, &dontOverwriteBool);
        if (dontOverwriteBool) {
            dontOverwriteNum = 1;
        }
    }
    if (params != nullptr) {
        napi_value dontOverNum = nullptr;
        napi_create_int32(env, dontOverwriteNum, &dontOverNum);
        napi_value dontOverBool = nullptr;
        napi_coerce_to_bool(env, dontOverNum, &dontOverBool);
        napi_value routerParamsData = nullptr;
        napi_create_object(env, &routerParamsData);
        napi_set_named_property(env, routerParamsData, "paramsData", params);
        napi_set_named_property(env, routerParamsData, "dontOverwrite", dontOverBool);

        napi_value globalValue;
        napi_get_global(env, &globalValue);
        napi_value jsonValue;
        napi_get_named_property(env, globalValue, "JSON", &jsonValue);
        napi_value stringifyValue;
        napi_get_named_property(env, jsonValue, "stringify", &stringifyValue);
        napi_value funcArgv[1] = { routerParamsData };
        napi_value returnValue;
        napi_call_function(env, jsonValue, stringifyValue, 1, funcArgv, &returnValue);
        auto nativeValue = reinterpret_cast<NativeValue*>(returnValue);
        auto resultValue = nativeValue->ToString();
        auto nativeString = reinterpret_cast<NativeString*>(resultValue->GetInterface(NativeString::INTERFACE_ID));
        size_t len = nativeString->GetLength();
        std::unique_ptr<char[]> paramsChar = std::make_unique<char[]>(len);
        size_t ret = 0;
        napi_get_value_string_utf8(env, returnValue, paramsChar.get(), len, &ret);
        paramsString = paramsChar.get();
    }
}

static napi_value JSRouterPush(napi_env env, napi_callback_info info)
{
    size_t requireArgc = 1;
    size_t argc = 0;
    napi_value argv;
    napi_value thisVar = nullptr;
    void* data = nullptr;
    napi_get_cb_info(env, info, &argc, &argv, &thisVar, &data);
    if (argc < requireArgc) {
        LOGE("params number err");
        return nullptr;
    }
    napi_value uriNApi = nullptr;
    napi_value params = nullptr;
    napi_value dontOverwrite = nullptr;
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv, &valueType);
    if (valueType == napi_object) {
        napi_get_named_property(env, argv, "uri", &uriNApi);
        napi_get_named_property(env, argv, "params", &params);
        napi_get_named_property(env, argv, "dontOverwrite", &dontOverwrite);
    }
    std::string paramsString;
    ParseParams(env, params, dontOverwrite, paramsString);
    std::string uriString;
    ParseUri(env, uriNApi, uriString);

    OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
    napi_get_jsEngine(env, (void**)&jsEngine);
    jsEngine->GetFrontend()->Push(uriString, paramsString);
    return nullptr;
}

static napi_value JSRouterReplace(napi_env env, napi_callback_info info)
{
    size_t requireArgc = 1;
    size_t argc = 0;
    napi_value argv;
    napi_value thisVar = nullptr;
    void* data = nullptr;
    napi_get_cb_info(env, info, &argc, &argv, &thisVar, &data);
    if (argc < requireArgc) {
        LOGE("params number err");
        return nullptr;
    }
    napi_value uriNApi = nullptr;
    napi_value params = nullptr;
    napi_value dontOverwrite = nullptr;
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv, &valueType);
    if (valueType == napi_object) {
        napi_get_named_property(env, argv, "uri", &uriNApi);
        napi_get_named_property(env, argv, "params", &params);
        napi_get_named_property(env, argv, "dontOverwrite", &dontOverwrite);
    }
    std::string paramsString;
    ParseParams(env, params, dontOverwrite, paramsString);
    std::string uriString;
    ParseUri(env, uriNApi, uriString);

    OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
    napi_get_jsEngine(env, (void**)&jsEngine);
    jsEngine->GetFrontend()->Replace(uriString, paramsString);
    return nullptr;
}

static napi_value JSRouterBack(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value argv = nullptr;
    napi_value thisVar = nullptr;
    void* data = nullptr;
    napi_get_cb_info(env, info, &argc, &argv, &thisVar, &data);

    OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
    napi_get_jsEngine(env, (void**)&jsEngine);
    std::string uriString;
    std::string paramsString;
    napi_value uriNApi = nullptr;
    napi_value params = nullptr;
    napi_value dontOverwrite = nullptr;
    if (argc == 0) {
        jsEngine->GetFrontend()->Back(uriString, paramsString);
    } else {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv, &valueType);
        if (valueType == napi_object) {
            napi_get_named_property(env, argv, "uri", &uriNApi);
            napi_get_named_property(env, argv, "params", &params);
            napi_get_named_property(env, argv, "dontOverwrite", &dontOverwrite);
            if (uriNApi) {
                ParseParams(env, params, dontOverwrite, paramsString);
                jsEngine->GetFrontend()->Back(uriString, paramsString);
                return nullptr;
            }
        }
        ParseUri(env, uriNApi, uriString);
        ParseParams(env, params, dontOverwrite, paramsString);
        jsEngine->GetFrontend()->Back(uriString, paramsString);
    }
    return nullptr;
}

static napi_value JSRouterClear(napi_env env, napi_callback_info info)
{
    OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
    napi_get_jsEngine(env, (void**)&jsEngine);
    jsEngine->GetFrontend()->Clear();
    return nullptr;
}

static napi_value JSRouterGetLength(napi_env env, napi_callback_info info)
{
    OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
    napi_get_jsEngine(env, (void**)&jsEngine);
    int32_t routeNumber = jsEngine->GetFrontend()->GetStackSize();
    napi_value routeNApiNum = nullptr;
    napi_create_int32(env, routeNumber, &routeNApiNum);
    napi_value result = nullptr;
    napi_coerce_to_string(env, routeNApiNum, &result);
    return result;
}

static napi_value JSRouterGetState(napi_env env, napi_callback_info info)
{
    int32_t routeIndex = 0;
    std::string routeName;
    std::string routePath;
    OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
    napi_get_jsEngine(env, (void**)&jsEngine);
    jsEngine->GetFrontend()->GetState(routeIndex, routeName, routePath);
    size_t routeNameLen = routeName.length();
    size_t routePathLen = routePath.length();

    napi_value resultArray[3] = { 0 };
    napi_create_int32(env, routeIndex, &resultArray[0]);
    napi_create_string_utf8(env, routeName.c_str(), routeNameLen, &resultArray[1]);
    napi_create_string_utf8(env, routePath.c_str(), routePathLen, &resultArray[2]);

    napi_value result = nullptr;
    napi_create_object(env, &result);
    napi_set_named_property(env, result, "index", resultArray[0]);
    napi_set_named_property(env, result, "name", resultArray[1]);
    napi_set_named_property(env, result, "path", resultArray[2]);
    return result;
}

struct RouterAsyncContext {
    napi_env env = nullptr;
    napi_async_work work = nullptr;
    napi_value message_napi = nullptr;
    napi_ref callbackSuccess = nullptr;
    napi_ref callbackFail = nullptr;
    napi_ref callbackComplete = nullptr;
    std::string messageString;
    std::string fail;
    std::string success;
    std::string complete;
};

napi_value GetReturnObj(napi_env env, std::string callbackString)
{
    napi_value result = nullptr;
    napi_value returnObj = nullptr;
    napi_create_object(env, &returnObj);
    napi_create_string_utf8(env, callbackString.c_str(), NAPI_AUTO_LENGTH, &result);
    napi_set_named_property(env, returnObj, "errMsg", result);
    return returnObj;
}

static napi_value JSRouterEnableAlertBeforeBackPage(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value argv = nullptr;
    napi_value thisVar = nullptr;
    void* data = nullptr;
    napi_get_cb_info(env, info, &argc, &argv, &thisVar, &data);
    auto routerAsyncContext = new RouterAsyncContext();
    routerAsyncContext->env = env;

    napi_value successFunc = nullptr;
    napi_value failFunc = nullptr;
    napi_value completeFunc = nullptr;
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv, &valueType);
    if (valueType == napi_object) {
        napi_get_named_property(env, argv, "message", &routerAsyncContext->message_napi);
        napi_get_named_property(env, argv, "success", &successFunc);
        napi_get_named_property(env, argv, "fail", &failFunc);
        napi_get_named_property(env, argv, "complete", &completeFunc);
        napi_typeof(env, routerAsyncContext->message_napi, &valueType);
        if (routerAsyncContext->message_napi == nullptr) {
            routerAsyncContext->fail = "enableAlertBeforeBackPage:massage is null";
            routerAsyncContext->complete = "enableAlertBeforeBackPage:massage is null";
        }
        if (valueType == napi_string) {
            auto nativeValue = reinterpret_cast<NativeValue*>(routerAsyncContext->message_napi);
            auto resultValue = nativeValue->ToString();
            auto nativeString = reinterpret_cast<NativeString*>(resultValue->GetInterface(NativeString::INTERFACE_ID));
            size_t len = nativeString->GetLength();
            std::unique_ptr<char[]> messageChar = std::make_unique<char[]>(len);
            size_t ret = 0;
            napi_get_value_string_utf8(env, routerAsyncContext->message_napi, messageChar.get(), len, &ret);
            routerAsyncContext->messageString = messageChar.get();
        }
        napi_typeof(env, successFunc, &valueType);
        if (valueType == napi_function) {
            napi_create_reference(env, successFunc, 1, &routerAsyncContext->callbackSuccess);
        }
        napi_typeof(env, failFunc, &valueType);
        if (valueType == napi_function) {
            napi_create_reference(env, failFunc, 1, &routerAsyncContext->callbackFail);
        }
        napi_typeof(env, completeFunc, &valueType);
        if (valueType == napi_function) {
            napi_create_reference(env, completeFunc, 1, &routerAsyncContext->callbackComplete);
        }
    }
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSRouterEnableAlertBeforeBackPage", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource, [](napi_env env, void* data) {},
        [](napi_env env, napi_status status, void* data) {
            RouterAsyncContext* asyncContext = (RouterAsyncContext*)data;
            OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
            napi_get_jsEngine(env, (void**)&jsEngine);
            napi_value ret;
            napi_value callback = nullptr;
            if (!asyncContext->fail.empty()) {
                if (asyncContext->callbackFail) {
                    napi_value returnObj = GetReturnObj(env, asyncContext->fail);
                    napi_get_reference_value(env, asyncContext->callbackFail, &callback);
                    napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                    napi_delete_reference(env, asyncContext->callbackFail);
                }
                if (asyncContext->callbackComplete) {
                    napi_value returnObj = GetReturnObj(env, asyncContext->complete);
                    napi_get_reference_value(env, asyncContext->callbackComplete, &callback);
                    napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                    napi_delete_reference(env, asyncContext->callbackComplete);
                }
            }
            auto callBack = [env, asyncContext](int32_t callbackType) {
                napi_value ret;
                napi_value callback = nullptr;
                switch (callbackType) {
                    case 1:
                        if (asyncContext->callbackSuccess) {
                            asyncContext->success = "enableAlertBeforeBackPage:ok";
                            napi_value returnObj = GetReturnObj(env, asyncContext->success);
                            napi_get_reference_value(env, asyncContext->callbackSuccess, &callback);
                            napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                            napi_delete_reference(env, asyncContext->callbackSuccess);
                        }

                        if (asyncContext->callbackComplete) {
                            asyncContext->complete = "enableAlertBeforeBackPage:ok";
                            napi_value returnObj = GetReturnObj(env, asyncContext->complete);
                            napi_get_reference_value(env, asyncContext->callbackComplete, &callback);
                            napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                            napi_delete_reference(env, asyncContext->callbackComplete);
                        }

                        break;
                    case 0:
                        if (asyncContext->callbackFail) {
                            asyncContext->fail = "enableAlertBeforeBackPage:fail cancel";
                            napi_value returnObj = GetReturnObj(env, asyncContext->fail);
                            napi_get_reference_value(env, asyncContext->callbackFail, &callback);
                            napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                            napi_delete_reference(env, asyncContext->callbackFail);
                        }
                        if (asyncContext->callbackComplete) {
                            asyncContext->complete = "enableAlertBeforeBackPage:fail cancel";
                            napi_value returnObj = GetReturnObj(env, asyncContext->complete);
                            napi_get_reference_value(env, asyncContext->callbackComplete, &callback);
                            napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                            napi_delete_reference(env, asyncContext->callbackComplete);
                        }
                        break;
                    default:
                        LOGE("callbackType is invalid");
                        break;
                }
            };
            jsEngine->GetFrontend()->EnableAlertBeforeBackPage(asyncContext->messageString, std::move(callBack));
        },
        (void*)routerAsyncContext, &routerAsyncContext->work);
    napi_queue_async_work(env, routerAsyncContext->work);
    return nullptr;
}

struct DisableRouterAsyncContext {
    napi_env env = nullptr;
    napi_async_work work = nullptr;
    napi_ref callbackSuccess = nullptr;
    napi_ref callbackFail = nullptr;
    napi_ref callbackComplete = nullptr;
    std::string fail;
    std::string success;
    std::string complete;
};

static napi_value JSRouterDisableAlertBeforeBackPage(napi_env env, napi_callback_info info)
{
    auto disableRouterAsyncContext = new DisableRouterAsyncContext();
    disableRouterAsyncContext->env = env;
    size_t argc = 0;
    napi_value argv = nullptr;
    napi_value thisVar = nullptr;
    void* data = nullptr;
    napi_get_cb_info(env, info, &argc, &argv, &thisVar, &data);
    napi_value successFunc = nullptr;
    napi_value failFunc = nullptr;
    napi_value completeFunc = nullptr;
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv, &valueType);
    if (valueType == napi_object) {
        napi_get_named_property(env, argv, "success", &successFunc);
        napi_get_named_property(env, argv, "fail", &failFunc);
        napi_get_named_property(env, argv, "complete", &completeFunc);
        napi_typeof(env, successFunc, &valueType);
        if (valueType == napi_function) {
            napi_create_reference(env, successFunc, 1, &disableRouterAsyncContext->callbackSuccess);
        }
        napi_typeof(env, failFunc, &valueType);
        if (valueType == napi_function) {
            napi_create_reference(env, failFunc, 1, &disableRouterAsyncContext->callbackFail);
        }
        napi_typeof(env, completeFunc, &valueType);
        if (valueType == napi_function) {
            napi_create_reference(env, completeFunc, 1, &disableRouterAsyncContext->callbackComplete);
        }
    }
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "JSRouterDisableAlertBeforeBackPage", NAPI_AUTO_LENGTH, &resource);
    napi_create_async_work(
        env, nullptr, resource, [](napi_env env, void* data) {},
        [](napi_env env, napi_status status, void* data) {
            DisableRouterAsyncContext* asyncContext = (DisableRouterAsyncContext*)data;
            OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
            napi_get_jsEngine(env, (void**)&jsEngine);
            jsEngine->GetFrontend()->DisableAlertBeforeBackPage();
            napi_value ret;
            napi_value callback = nullptr;
            asyncContext->success = "disableAlertBeforeBackPage:ok";
            asyncContext->complete = "disableAlertBeforeBackPage:ok";
            if (asyncContext->callbackSuccess) {
                napi_value returnObj = GetReturnObj(env, asyncContext->success);
                napi_get_reference_value(env, asyncContext->callbackSuccess, &callback);
                napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                napi_delete_reference(env, asyncContext->callbackSuccess);
            }
            if (asyncContext->callbackComplete) {
                napi_value returnObj = GetReturnObj(env, asyncContext->complete);
                napi_get_reference_value(env, asyncContext->callbackComplete, &callback);
                napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                napi_delete_reference(env, asyncContext->callbackComplete);
            }
            if (asyncContext->callbackFail) {
                napi_value returnObj = GetReturnObj(env, asyncContext->fail);
                napi_get_reference_value(env, asyncContext->callbackFail, &callback);
                napi_call_function(env, nullptr, callback, 1, &returnObj, &ret);
                napi_delete_reference(env, asyncContext->callbackFail);
            }
        },
        (void*)disableRouterAsyncContext, &disableRouterAsyncContext->work);
    napi_queue_async_work(env, disableRouterAsyncContext->work);
    return nullptr;
}

static napi_value JSRouterGetParams(napi_env env, napi_callback_info info)
{
    OHOS::Ace::Framework::JsEngine* jsEngine = nullptr;
    napi_get_jsEngine(env, (void**)&jsEngine);
    std::string paramsStr = jsEngine->GetFrontend()->GetParams();
    LOGI("PageGetParams params is null");
    if (paramsStr.empty()) {
        return nullptr;
    }
    napi_value globalValue;
    napi_get_global(env, &globalValue);
    napi_value jsonValue;
    napi_get_named_property(env, globalValue, "JSON", &jsonValue);
    napi_value parseValue;
    napi_get_named_property(env, jsonValue, "parse", &parseValue);
    napi_value routerParamsNApi;
    napi_create_string_utf8(env, paramsStr.c_str(), NAPI_AUTO_LENGTH, &routerParamsNApi);
    napi_value funcArgv[1] = { routerParamsNApi };
    napi_value result;
    napi_call_function(env, jsonValue, parseValue, 1, funcArgv, &result);
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, result, &valueType);
    if (valueType != napi_object) {
        LOGE("parse result fail");
        return nullptr;
    }
    return result;
}

static napi_value RouterExport(napi_env env, napi_value exports)
{
    static napi_property_descriptor routerDesc[] = {
        DECLARE_NAPI_FUNCTION("push", JSRouterPush),
        DECLARE_NAPI_FUNCTION("replace", JSRouterReplace),
        DECLARE_NAPI_FUNCTION("back", JSRouterBack),
        DECLARE_NAPI_FUNCTION("clear", JSRouterClear),
        DECLARE_NAPI_FUNCTION("getLength", JSRouterGetLength),
        DECLARE_NAPI_FUNCTION("getState", JSRouterGetState),
        DECLARE_NAPI_FUNCTION("enableAlertBeforeBackPage", JSRouterEnableAlertBeforeBackPage),
        DECLARE_NAPI_FUNCTION("disableAlertBeforeBackPage", JSRouterDisableAlertBeforeBackPage),
        DECLARE_NAPI_FUNCTION("getParams", JSRouterGetParams),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(routerDesc) / sizeof(routerDesc[0]), routerDesc));
    return exports;
}

static napi_module routerModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = RouterExport,
    .nm_modname = "router",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RouterRegister()
{
    napi_module_register(&routerModule);
}

} // namespace OHOS::Ace::Napi
