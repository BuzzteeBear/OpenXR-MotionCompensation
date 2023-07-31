// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********
// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
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

	XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
	{
		TraceLoggingWrite(g_traceProvider, "xrPollEvent");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrPollEvent(instance, eventData);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrPollEvent_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrPollEvent: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrPollEvent_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrPollEvent failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
	{
		TraceLoggingWrite(g_traceProvider, "xrGetSystem");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrGetSystem(instance, getInfo, systemId);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrGetSystem_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrGetSystem: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrGetSystem_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrGetSystem failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
	{
		TraceLoggingWrite(g_traceProvider, "xrCreateSession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrCreateSession(instance, createInfo, session);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrCreateSession_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrCreateSession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrCreateSession_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrCreateSession failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrDestroySession(XrSession session)
	{
		TraceLoggingWrite(g_traceProvider, "xrDestroySession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrDestroySession(session);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrDestroySession_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrDestroySession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrDestroySession_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrDestroySession failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
	{
		TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrCreateReferenceSpace(session, createInfo, space);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrCreateReferenceSpace: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrCreateReferenceSpace failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
	{
		TraceLoggingWrite(g_traceProvider, "xrLocateSpace");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrLocateSpace(space, baseSpace, time, location);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrLocateSpace_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrLocateSpace: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrLocateSpace_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrLocateSpace failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
	{
		TraceLoggingWrite(g_traceProvider, "xrCreateSwapchain");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrCreateSwapchain(session, createInfo, swapchain);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrCreateSwapchain_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrCreateSwapchain: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrCreateSwapchain_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrCreateSwapchain failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrDestroySwapchain(XrSwapchain swapchain)
	{
		TraceLoggingWrite(g_traceProvider, "xrDestroySwapchain");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrDestroySwapchain(swapchain);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrDestroySwapchain_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrDestroySwapchain: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrDestroySwapchain_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrDestroySwapchain failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index)
	{
		TraceLoggingWrite(g_traceProvider, "xrAcquireSwapchainImage");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrAcquireSwapchainImage(swapchain, acquireInfo, index);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrAcquireSwapchainImage_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrAcquireSwapchainImage: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrAcquireSwapchainImage_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrAcquireSwapchainImage failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo)
	{
		TraceLoggingWrite(g_traceProvider, "xrWaitSwapchainImage");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrWaitSwapchainImage(swapchain, waitInfo);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrWaitSwapchainImage_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrWaitSwapchainImage: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrWaitSwapchainImage_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrWaitSwapchainImage failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
	{
		TraceLoggingWrite(g_traceProvider, "xrReleaseSwapchainImage");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrReleaseSwapchainImage(swapchain, releaseInfo);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrReleaseSwapchainImage_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrReleaseSwapchainImage: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrReleaseSwapchainImage_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrReleaseSwapchainImage failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
	{
		TraceLoggingWrite(g_traceProvider, "xrBeginSession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrBeginSession(session, beginInfo);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrBeginSession_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrBeginSession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrBeginSession_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrBeginSession failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrEndSession(XrSession session)
	{
		TraceLoggingWrite(g_traceProvider, "xrEndSession");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrEndSession(session);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrEndSession_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrEndSession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrEndSession_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrEndSession failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
	{
		TraceLoggingWrite(g_traceProvider, "xrBeginFrame");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrBeginFrame(session, frameBeginInfo);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrBeginFrame_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrBeginFrame: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrBeginFrame_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrBeginFrame failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
	{
		TraceLoggingWrite(g_traceProvider, "xrEndFrame");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrEndFrame(session, frameEndInfo);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrEndFrame_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrEndFrame: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrEndFrame_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrEndFrame failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
	{
		TraceLoggingWrite(g_traceProvider, "xrLocateViews");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrLocateViews_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrLocateViews: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrLocateViews_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrLocateViews failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
	{
		TraceLoggingWrite(g_traceProvider, "xrSuggestInteractionProfileBindings");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrSuggestInteractionProfileBindings(instance, suggestedBindings);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrSuggestInteractionProfileBindings_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrSuggestInteractionProfileBindings: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrSuggestInteractionProfileBindings_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrSuggestInteractionProfileBindings failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
	{
		TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrAttachSessionActionSets(session, attachInfo);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrAttachSessionActionSets: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrAttachSessionActionSets failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile)
	{
		TraceLoggingWrite(g_traceProvider, "xrGetCurrentInteractionProfile");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrGetCurrentInteractionProfile_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrGetCurrentInteractionProfile: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrGetCurrentInteractionProfile_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrGetCurrentInteractionProfile failed with %s\n", xr::ToCString(result));
		}

		return result;
	}

	XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
	{
		TraceLoggingWrite(g_traceProvider, "xrSyncActions");

		XrResult result;
		try
		{
			result = openxr_api_layer::GetInstance()->xrSyncActions(session, syncInfo);
		}
		catch (std::exception exc)
		{
			TraceLoggingWrite(g_traceProvider, "xrSyncActions_Error", TLArg(exc.what(), "Error"));
			ErrorLog("xrSyncActions: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWrite(g_traceProvider, "xrSyncActions_Result", TLArg(xr::ToCString(result), "Result"));
		if (XR_FAILED(result)) {
			ErrorLog("xrSyncActions failed with %s\n", xr::ToCString(result));
		}

		return result;
	}


	// Auto-generated dispatcher handler.
	XrResult OpenXrApi::xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function)
	{
		XrResult result = m_xrGetInstanceProcAddr(instance, name, function);

		const std::string apiName(name);

		if (apiName == "xrDestroyInstance")
		{
			m_xrDestroyInstance = reinterpret_cast<PFN_xrDestroyInstance>(*function);
			*function = reinterpret_cast<PFN_xrVoidFunction>(openxr_api_layer::xrDestroyInstance);
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
			throw new std::runtime_error("Failed to resolve xrGetInstanceProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetSystemProperties", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetSystemProperties))))
		{
			throw new std::runtime_error("Failed to resolve xrGetSystemProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateActionSpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateActionSpace))))
		{
			throw new std::runtime_error("Failed to resolve xrCreateActionSpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroySpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroySpace))))
		{
			throw new std::runtime_error("Failed to resolve xrDestroySpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateSwapchainFormats", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateSwapchainFormats))))
		{
			throw new std::runtime_error("Failed to resolve xrEnumerateSwapchainFormats");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateSwapchainImages", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateSwapchainImages))))
		{
			throw new std::runtime_error("Failed to resolve xrEnumerateSwapchainImages");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrStringToPath", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrStringToPath))))
		{
			throw new std::runtime_error("Failed to resolve xrStringToPath");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrPathToString", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrPathToString))))
		{
			throw new std::runtime_error("Failed to resolve xrPathToString");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateActionSet", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateActionSet))))
		{
			throw new std::runtime_error("Failed to resolve xrCreateActionSet");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroyActionSet", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroyActionSet))))
		{
			throw new std::runtime_error("Failed to resolve xrDestroyActionSet");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateAction", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateAction))))
		{
			throw new std::runtime_error("Failed to resolve xrCreateAction");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroyAction", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroyAction))))
		{
			throw new std::runtime_error("Failed to resolve xrDestroyAction");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrSuggestInteractionProfileBindings", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrSuggestInteractionProfileBindings))))
		{
			throw new std::runtime_error("Failed to resolve xrSuggestInteractionProfileBindings");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetActionStateBoolean", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetActionStateBoolean))))
		{
			throw new std::runtime_error("Failed to resolve xrGetActionStateBoolean");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetActionStatePose", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetActionStatePose))))
		{
			throw new std::runtime_error("Failed to resolve xrGetActionStatePose");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrApplyHapticFeedback", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrApplyHapticFeedback))))
		{
			throw new std::runtime_error("Failed to resolve xrApplyHapticFeedback");
		}
		m_applicationName = createInfo->applicationInfo.applicationName;
		return XR_SUCCESS;
	}

} // namespace openxr_api_layer

