// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********
// MIT License
//
// Copyright(c) 2021-2023 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pch.h"

#include <layer.h>

#include "dispatch.h"
#include "log.h"

using namespace openxr_api_layer::log;

namespace openxr_api_layer
{

	// Auto-generated wrappers for the requested APIs.

	XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* layerName, uint32_t propertyCapacityInput, uint32_t* propertyCountOutput, XrExtensionProperties* properties)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrEnumerateInstanceExtensionProperties");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrEnumerateInstanceExtensionProperties(layerName, propertyCapacityInput, propertyCountOutput, properties);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrEnumerateInstanceExtensionProperties_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrEnumerateInstanceExtensionProperties: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrEnumerateInstanceExtensionProperties", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrEnumerateInstanceExtensionProperties failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrDestroyInstance(XrInstance instance)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrDestroyInstance");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrDestroyInstance(instance);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrDestroyInstance_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrDestroyInstance: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrDestroyInstance", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrDestroyInstance failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrPollEvent");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrPollEvent(instance, eventData);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrPollEvent_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrPollEvent: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrPollEvent", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrPollEvent failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetSystem");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrGetSystem(instance, getInfo, systemId);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetSystem_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrGetSystem: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetSystem", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrGetSystem failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateSession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrCreateSession(instance, createInfo, session);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateSession_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrCreateSession: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateSession", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrCreateSession failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrDestroySession(XrSession session)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrDestroySession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrDestroySession(session);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrDestroySession_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrDestroySession: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrDestroySession", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrDestroySession failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateReferenceSpace");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrCreateReferenceSpace(session, createInfo, space);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateReferenceSpace_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrCreateReferenceSpace: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateReferenceSpace", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrCreateReferenceSpace failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateActionSpace");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrCreateActionSpace(session, createInfo, space);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateActionSpace_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrCreateActionSpace: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateActionSpace", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrCreateActionSpace failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrLocateSpace");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrLocateSpace(space, baseSpace, time, location);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrLocateSpace_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrLocateSpace: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrLocateSpace", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrLocateSpace failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateSwapchain");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrCreateSwapchain(session, createInfo, swapchain);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateSwapchain_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrCreateSwapchain: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateSwapchain", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrCreateSwapchain failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain swapchain)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrDestroySwapchain");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrDestroySwapchain(swapchain);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrDestroySwapchain_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrDestroySwapchain: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrDestroySwapchain", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrDestroySwapchain failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrAcquireSwapchainImage");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrAcquireSwapchainImage(swapchain, acquireInfo, index);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrAcquireSwapchainImage_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrAcquireSwapchainImage: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrAcquireSwapchainImage", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrAcquireSwapchainImage failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrWaitSwapchainImage");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrWaitSwapchainImage(swapchain, waitInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrWaitSwapchainImage_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrWaitSwapchainImage: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrWaitSwapchainImage", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrWaitSwapchainImage failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrReleaseSwapchainImage");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrReleaseSwapchainImage(swapchain, releaseInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrReleaseSwapchainImage_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrReleaseSwapchainImage: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrReleaseSwapchainImage", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrReleaseSwapchainImage failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrBeginSession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrBeginSession(session, beginInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrBeginSession_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrBeginSession: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrBeginSession", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrBeginSession failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrEndSession(XrSession session)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrEndSession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrEndSession(session);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrEndSession_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrEndSession: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrEndSession", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrEndSession failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrBeginFrame");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrBeginFrame(session, frameBeginInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrBeginFrame_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrBeginFrame: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrBeginFrame", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrBeginFrame failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrEndFrame");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrEndFrame(session, frameEndInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrEndFrame_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrEndFrame: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrEndFrame", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrEndFrame failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrLocateViews");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrLocateViews_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrLocateViews: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrLocateViews", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrLocateViews failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrSuggestInteractionProfileBindings");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrSuggestInteractionProfileBindings(instance, suggestedBindings);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrSuggestInteractionProfileBindings_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrSuggestInteractionProfileBindings: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrSuggestInteractionProfileBindings", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrSuggestInteractionProfileBindings failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrAttachSessionActionSets");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrAttachSessionActionSets(session, attachInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrAttachSessionActionSets_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrAttachSessionActionSets: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrAttachSessionActionSets", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrAttachSessionActionSets failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetCurrentInteractionProfile");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetCurrentInteractionProfile_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrGetCurrentInteractionProfile: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetCurrentInteractionProfile", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrGetCurrentInteractionProfile failed with {}", xr::ToCString(result)));
		}

		return result;
	}

	XrResult XRAPI_CALL xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrSyncActions");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrSyncActions(session, syncInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrSyncActions_Error", TLArg(exc.what(), "Error"));
			ErrorLog(fmt::format("xrSyncActions: {}", exc.what()));
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrSyncActions", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog(fmt::format("xrSyncActions failed with {}", xr::ToCString(result)));
		}

		return result;
	}


	// Auto-generated dispatcher handler.
	XrResult OpenXrApi::xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function)
	{
		return xrGetInstanceProcAddrInternal(instance, name, function);
	}

	XrResult OpenXrApi::xrGetInstanceProcAddrInternal(XrInstance instance, const char* name, PFN_xrVoidFunction* function)
	{
		XrResult result = m_xrGetInstanceProcAddr(instance, name, function);

		const std::string apiName(name);

		if (apiName == "xrDestroyInstance")
		{
			m_xrDestroyInstance = reinterpret_cast<PFN_xrDestroyInstance>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrDestroyInstance);
		}
		else if (apiName == "xrEnumerateInstanceExtensionProperties")
		{
			m_xrEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_xrEnumerateInstanceExtensionProperties>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrEnumerateInstanceExtensionProperties);
		}
		else if (apiName == "xrPollEvent")
		{
			m_xrPollEvent = reinterpret_cast<PFN_xrPollEvent>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrPollEvent);
		}
		else if (apiName == "xrGetSystem")
		{
			m_xrGetSystem = reinterpret_cast<PFN_xrGetSystem>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrGetSystem);
		}
		else if (apiName == "xrCreateSession")
		{
			m_xrCreateSession = reinterpret_cast<PFN_xrCreateSession>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrCreateSession);
		}
		else if (apiName == "xrDestroySession")
		{
			m_xrDestroySession = reinterpret_cast<PFN_xrDestroySession>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrDestroySession);
		}
		else if (apiName == "xrCreateReferenceSpace")
		{
			m_xrCreateReferenceSpace = reinterpret_cast<PFN_xrCreateReferenceSpace>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrCreateReferenceSpace);
		}
		else if (apiName == "xrCreateActionSpace")
		{
			m_xrCreateActionSpace = reinterpret_cast<PFN_xrCreateActionSpace>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrCreateActionSpace);
		}
		else if (apiName == "xrLocateSpace")
		{
			m_xrLocateSpace = reinterpret_cast<PFN_xrLocateSpace>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrLocateSpace);
		}
		else if (apiName == "xrCreateSwapchain")
		{
			m_xrCreateSwapchain = reinterpret_cast<PFN_xrCreateSwapchain>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrCreateSwapchain);
		}
		else if (apiName == "xrDestroySwapchain")
		{
			m_xrDestroySwapchain = reinterpret_cast<PFN_xrDestroySwapchain>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrDestroySwapchain);
		}
		else if (apiName == "xrAcquireSwapchainImage")
		{
			m_xrAcquireSwapchainImage = reinterpret_cast<PFN_xrAcquireSwapchainImage>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrAcquireSwapchainImage);
		}
		else if (apiName == "xrWaitSwapchainImage")
		{
			m_xrWaitSwapchainImage = reinterpret_cast<PFN_xrWaitSwapchainImage>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrWaitSwapchainImage);
		}
		else if (apiName == "xrReleaseSwapchainImage")
		{
			m_xrReleaseSwapchainImage = reinterpret_cast<PFN_xrReleaseSwapchainImage>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrReleaseSwapchainImage);
		}
		else if (apiName == "xrBeginSession")
		{
			m_xrBeginSession = reinterpret_cast<PFN_xrBeginSession>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrBeginSession);
		}
		else if (apiName == "xrEndSession")
		{
			m_xrEndSession = reinterpret_cast<PFN_xrEndSession>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrEndSession);
		}
		else if (apiName == "xrBeginFrame")
		{
			m_xrBeginFrame = reinterpret_cast<PFN_xrBeginFrame>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrBeginFrame);
		}
		else if (apiName == "xrEndFrame")
		{
			m_xrEndFrame = reinterpret_cast<PFN_xrEndFrame>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrEndFrame);
		}
		else if (apiName == "xrLocateViews")
		{
			m_xrLocateViews = reinterpret_cast<PFN_xrLocateViews>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrLocateViews);
		}
		else if (apiName == "xrSuggestInteractionProfileBindings")
		{
			m_xrSuggestInteractionProfileBindings = reinterpret_cast<PFN_xrSuggestInteractionProfileBindings>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrSuggestInteractionProfileBindings);
		}
		else if (apiName == "xrAttachSessionActionSets")
		{
			m_xrAttachSessionActionSets = reinterpret_cast<PFN_xrAttachSessionActionSets>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrAttachSessionActionSets);
		}
		else if (apiName == "xrGetCurrentInteractionProfile")
		{
			m_xrGetCurrentInteractionProfile = reinterpret_cast<PFN_xrGetCurrentInteractionProfile>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrGetCurrentInteractionProfile);
		}
		else if (apiName == "xrSyncActions")
		{
			m_xrSyncActions = reinterpret_cast<PFN_xrSyncActions>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrSyncActions);
		}


		return result;
	}

	// Auto-generated create instance handler.
	XrResult OpenXrApi::xrCreateInstance(const XrInstanceCreateInfo* createInfo)
    {
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetInstanceProperties", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetInstanceProperties))))
		{
			throw std::runtime_error("Failed to resolve xrGetInstanceProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetSystemProperties", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetSystemProperties))))
		{
			throw std::runtime_error("Failed to resolve xrGetSystemProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroySpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroySpace))))
		{
			throw std::runtime_error("Failed to resolve xrDestroySpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateSwapchainFormats", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateSwapchainFormats))))
		{
			throw std::runtime_error("Failed to resolve xrEnumerateSwapchainFormats");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateSwapchainImages", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateSwapchainImages))))
		{
			throw std::runtime_error("Failed to resolve xrEnumerateSwapchainImages");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrStringToPath", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrStringToPath))))
		{
			throw std::runtime_error("Failed to resolve xrStringToPath");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrPathToString", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrPathToString))))
		{
			throw std::runtime_error("Failed to resolve xrPathToString");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateActionSet", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateActionSet))))
		{
			throw std::runtime_error("Failed to resolve xrCreateActionSet");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroyActionSet", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroyActionSet))))
		{
			throw std::runtime_error("Failed to resolve xrDestroyActionSet");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateAction", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateAction))))
		{
			throw std::runtime_error("Failed to resolve xrCreateAction");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroyAction", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroyAction))))
		{
			throw std::runtime_error("Failed to resolve xrDestroyAction");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrSuggestInteractionProfileBindings", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrSuggestInteractionProfileBindings))))
		{
			throw std::runtime_error("Failed to resolve xrSuggestInteractionProfileBindings");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetActionStateBoolean", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetActionStateBoolean))))
		{
			throw std::runtime_error("Failed to resolve xrGetActionStateBoolean");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetActionStatePose", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetActionStatePose))))
		{
			throw std::runtime_error("Failed to resolve xrGetActionStatePose");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrApplyHapticFeedback", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrApplyHapticFeedback))))
		{
			throw std::runtime_error("Failed to resolve xrApplyHapticFeedback");
		}
		m_applicationName = createInfo->applicationInfo.applicationName;
		return XR_SUCCESS;
	}

	std::unique_ptr<OpenXrApi> g_instance;

	void ResetInstance() {
		g_instance.reset();
	}

} // namespace openxr_api_layer

