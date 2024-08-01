// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri, Sebastian Veith
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

#include "layer.h"
#include "tracker.h"
#include "output.h"
#include "config.h"
#include <log.h>
#include <util.h>


using namespace openxr_api_layer::log;
using namespace output;
using namespace xr::math;
namespace openxr_api_layer
{
    XrResult OpenXrLayer::xrDestroyInstance(XrInstance instance)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrDestroyInstance", TLXArg(instance, "Instance"));

        if (m_Enabled)
        {
            Log("xrDestroyInstance");
        }
        m_Overlay.reset();
        m_Tracker.reset();

        const XrResult result = OpenXrApi::xrDestroyInstance(instance);

        TraceLoggingWriteStop(local, "OpenXrLayer::xrDestroyInstance", TLPArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrCreateInstance(const XrInstanceCreateInfo* createInfo)
    {
        Log("xrCreateInstance");
        if (createInfo->type != XR_TYPE_INSTANCE_CREATE_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrLayer::xrCreateInstance",
                               TLArg(xr::ToString(createInfo->applicationInfo.apiVersion).c_str(), "ApiVersion"),
                               TLArg(createInfo->applicationInfo.applicationName, "ApplicationName"),
                               TLArg(createInfo->applicationInfo.applicationVersion, "ApplicationVersion"),
                               TLArg(createInfo->applicationInfo.engineName, "EngineName"),
                               TLArg(createInfo->applicationInfo.engineVersion, "EngineVersion"),
                               TLArg(createInfo->createFlags, "CreateFlags"));

        for (uint32_t i = 0; i < createInfo->enabledApiLayerCount; i++)
        {
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::xrCreateInstance",
                                    TLArg(createInfo->enabledApiLayerNames[i], "ApiLayerName"));
        }
        for (uint32_t i = 0; i < createInfo->enabledExtensionCount; i++)
        {
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::xrCreateInstance",
                                    TLArg(createInfo->enabledExtensionNames[i], "ExtensionName"));
        }

        // Needed to resolve the requested function pointers.
        const XrResult result = OpenXrApi::xrCreateInstance(createInfo);

        m_Application = GetApplicationName();

        // Dump the application name and OpenXR runtime information to help debugging issues.
        XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES};
        CHECK_XRCMD(OpenXrApi::xrGetInstanceProperties(GetXrInstance(), &instanceProperties));
        m_RuntimeName = std::format("{} {}.{}.{}",
                                    instanceProperties.runtimeName,
                                    XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
                                    XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                                    XR_VERSION_PATCH(instanceProperties.runtimeVersion));

        Log("Application: %s", m_Application.c_str());
        Log("Using OpenXR runtime: %s", m_RuntimeName.c_str());

        m_VarjoPollWorkaround = m_RuntimeName.find("Varjo") != std::string::npos;

        // Initialize Configuration
        m_Initialized = GetConfig()->Init(m_Application);

        // set log level
        GetConfig()->GetBool(Cfg::LogVerbose, logVerbose);
        Log("verbose logging %s\n", logVerbose ? "activated" : "off");

        if (!m_Initialized)
        {
            ErrorLog("%s: initialization failed - config file missing or incomplete", __FUNCTION__);
            AudioOut::Execute(Event::Error);
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrCreateInstance",
                                  TLArg(false, "Config_Valid"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }

        GetConfig()->GetBool(Cfg::Enabled, m_Enabled);
        if (!m_Enabled)
        {
            Log("motion compensation disabled in config file");
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrCreateInstance",
                                  TLArg(false, "Enabled"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }

        m_Tracker = tracker::GetTracker();
        m_Input = std::make_shared<input::InputHandler>(input::InputHandler(this));

        // enable / disable physical tracker initialization
        GetConfig()->GetBool(Cfg::PhysicalEnabled, m_PhysicalEnabled);
        if (!m_PhysicalEnabled)
        {
            Log("initialization of physical tracker disabled in config file");
        }
        else
        {
            if (!m_ViveTracker.Init())
            {
                m_Initialized = false;
            }
        }

        // enable / disable graphical overlay initialization
        bool overlayEnabled{false};
        GetConfig()->GetBool(Cfg::OverlayEnabled, overlayEnabled);
        if (overlayEnabled)
        {
            // Needed by DirectXTex.
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            m_Overlay = std::make_unique<graphics::Overlay>();
            m_CompositionFrameworkFactory = createCompositionFrameworkFactory(*createInfo,
                                                                              GetXrInstance(),
                                                                              m_xrGetInstanceProcAddr,
                                                                              graphics::CompositionApi::D3D11);
        }
        Log("graphical overlay is %s", overlayEnabled ? "enabled" : "disabled in config file");

        // initialize auto activator
        m_AutoActivator = std::make_unique<utility::AutoActivator>(utility::AutoActivator(m_Input));

        m_VirtualTrackerUsed = GetConfig()->IsVirtualTracker();
        if (m_PhysicalEnabled)
        {
            GetConfig()->GetBool(Cfg::CompensateControllers, m_CompensateControllers);
            if (m_CompensateControllers)
            {
                Log("compensation of motion controllers is activated (experimental)");
                m_SuppressInteraction = m_VirtualTrackerUsed;
            }
            if (!m_SuppressInteraction)
            {
                m_SubActionPath =
                    m_ViveTracker.active ? m_ViveTracker.role : "/user/hand/" + GetConfig()->GetControllerSide();
                if (const XrResult pathResult =
                        xrStringToPath(GetXrInstance(), m_SubActionPath.c_str(), &m_XrSubActionPath);
                    XR_FAILED(result))
                {
                    ErrorLog("%s: unable to create XrPath for sub action path %s: %s",
                             __FUNCTION__,
                             m_SubActionPath.c_str(),
                             xr::ToCString(pathResult));
                    m_SuppressInteraction = true;
                }
            }
        }

        // initialize tracker
        if (!m_Tracker->Init())
        {
            m_Initialized = false;
        }

        if (float timeout; GetConfig()->GetFloat(Cfg::TrackerTimeout, timeout))
        {
            m_RecoveryWait = static_cast<XrTime>(timeout * 1000000000.0);
        }
        Log("tracker timeout is set to %.3f ms", static_cast<double>(m_RecoveryWait) / 1000000.0);

        // use legacy mode
        GetConfig()->GetBool(Cfg::LegacyMode, m_LegacyMode);
        Log("legacy mode is %s", m_LegacyMode ? "activated" : "off");

        // initialize hmd modifier
        m_HmdModifier = std::make_unique<modifier::HmdModifier>();
        GetConfig()->GetBool(Cfg::FactorEnabled, m_ModifierActive);

        // choose cache for reverting pose in xrEndFrame
        GetConfig()->GetBool(Cfg::CacheUseEye, m_UseEyeCache);
        Log("%s is used for reconstruction of eye positions", m_UseEyeCache ? "caching" : "calculation");

        float cacheTolerance{2.0};
        GetConfig()->GetFloat(Cfg::CacheTolerance, cacheTolerance);
        Log("cache tolerance is set to %.3f ms", cacheTolerance);
        const auto toleranceTime = static_cast<XrTime>(cacheTolerance * 1000000.0);
        m_DeltaCache.SetTolerance(toleranceTime);
        m_EyeCache.SetTolerance(toleranceTime);

        // initialize keyboard input handler
        if (!m_Input->Init())
        {
            m_Initialized = false;
        }

        // enable debug test rotation
        GetConfig()->GetBool(Cfg::TestRotation, m_TestRotation);

        Log("layer initialization completed\n");
        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrCreateInstance",
                              TLArg(m_Initialized, "Initialized"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrGetSystem(instance, getInfo, systemId);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrGetSystem", TLXArg(instance, "Instance"));

        DebugLog("xrGetSystem");
        if (getInfo->type != XR_TYPE_SYSTEM_GET_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrGetSystem", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrGetSystem",
                                TLArg(xr::ToCString(getInfo->formFactor), "FormFactor"));

        const XrResult result = OpenXrApi::xrGetSystem(instance, getInfo, systemId);
        if (XR_SUCCEEDED(result) && getInfo->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY)
        {
            if (*systemId != m_systemId)
            {
                XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
                CHECK_XRCMD(OpenXrApi::xrGetSystemProperties(instance, *systemId, &systemProperties));
                TraceLoggingWriteTagged(local,
                                        "OpenXrLayer::xrGetSystem",
                                        TLArg(systemProperties.systemName, "SystemName"));
                Log("Using OpenXR system: %s", systemProperties.systemName);
            }

            // Remember the XrSystemId to use.
            m_systemId = *systemId;
        }

        TraceLoggingWriteStop(local, "OpenXrLayer::xrGetSystem", TLArg((int)*systemId, "SystemId"));

        return result;
    }

    XrResult OpenXrLayer::xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrPollEvent", TLXArg(instance, "Instance"));

        m_VarjoPollWorkaround = false;
        const XrResult result = OpenXrApi::xrPollEvent(instance, eventData);

        if (m_Enabled)
        {
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::xrPollEvent",
                                    TLArg(static_cast<int>(eventData->type), "EventType"));
            if (XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING == eventData->type)
            {
                if (const auto event = reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(eventData);
                    event && XR_REFERENCE_SPACE_TYPE_STAGE == event->referenceSpaceType ||
                    XR_REFERENCE_SPACE_TYPE_LOCAL == event->referenceSpaceType)
                {
                    // trigger re-location of static reference spaces
                    std::unique_lock lock(m_FrameLock);
                    Log("change of stage/local reference space location detected", event->changeTime);
                    m_UpdateRefSpaceTime = std::max(event->changeTime, m_LastFrameTime);

                    // reset tracker calibration and activation state
                    if (m_Tracker->m_Calibrated)
                    {
                        Log("tracker calibration lost");
                        m_Tracker->InvalidateCalibration();
                        AudioOut::Execute(Event::CalibrationLost);

                        if (m_Activated)
                        {
                            Log("motion compensation deactivated");
                            m_Activated = false;
                        }
                    }
                }
            }
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::xrPollEvent", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrCreateSession(XrInstance instance,
                                          const XrSessionCreateInfo* createInfo,
                                          XrSession* session)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrCreateSession(instance, createInfo, session);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrCreateSession", TLXArg(instance, "Instance"));
        Log("xrCreateSession");

        if (createInfo->type != XR_TYPE_SESSION_CREATE_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrCreateSession", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrCreateSession",
                                TLXArg(instance, "Instance"),
                                TLArg(createInfo->createFlags, "CreateFlags"));

        const XrResult result = OpenXrApi::xrCreateSession(instance, createInfo, session);
        if (XR_SUCCEEDED(result))
        {
            if (isSystemHandled(createInfo->systemId))
            {
                m_Session = *session;
                m_LastFrameTime = 0;
                m_UpdateRefSpaceTime = 0;

                if (m_Overlay && m_Overlay->m_Initialized && m_CompositionFrameworkFactory)
                {
                    m_CompositionFrameworkFactory->CreateSession(createInfo, *session);
                    m_Overlay->m_D3D12inUse = m_CompositionFrameworkFactory->IsUsingD3D12(*session);
                }

                if (bool earlyPhysicalInit; m_PhysicalEnabled &&
                                            GetConfig()->GetBool(Cfg::PhysicalEarly, earlyPhysicalInit) &&
                                            earlyPhysicalInit)
                {
                    Log("preforming early initialization of physical tracker");
                    // initialize everything except tracker
                    LazyInit(0);
                }

                CreateStageSpace();
                CreateViewSpace();
            }
        }

        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrCreateSession",
                              TLXArg(*session, "Session"),
                              TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrBeginSession(session, beginInfo);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrBeginSession", TLXArg(session, "Session"));

        Log("xrBeginSession");
        if (beginInfo->type != XR_TYPE_SESSION_BEGIN_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrBeginSession", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWriteTagged(
            local,
            "OpenXrLayer::xrBeginSession",
            TLXArg(session, "Session"),
            TLArg(xr::ToCString(beginInfo->primaryViewConfigurationType), "PrimaryViewConfigurationType"));

        const XrResult result = OpenXrApi::xrBeginSession(session, beginInfo);
        m_ViewConfigType = beginInfo->primaryViewConfigurationType;

        TraceLoggingWriteStop(local, "OpenXrLayer::xrBeginSession", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrEndSession(XrSession session)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrEndSession(session);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrEndssion", TLXArg(session, "Session"));
        Log("xrEndSession");

        const XrResult result = OpenXrApi::xrEndSession(session);

        TraceLoggingWriteStop(local, "OpenXrLayer::xrEndssion", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrDestroySession(XrSession session)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrDestroySession(session);
        }

        Log("xrDestroySession");
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrDestroySession", TLXArg(session, "Session"));

        m_Tracker->InvalidateCalibration();

        // clean up open xr session resources
        if (XR_NULL_HANDLE != m_TrackerSpace)
        {
            GetInstance()->xrDestroySpace(m_TrackerSpace);
            m_TrackerSpace = XR_NULL_HANDLE;
        }
        if (XR_NULL_HANDLE != m_StageSpace)
        {
            GetInstance()->xrDestroySpace(m_StageSpace);
            m_StageSpace = XR_NULL_HANDLE;
        }
        if (XR_NULL_HANDLE != m_ViewSpace)
        {
            GetInstance()->xrDestroySpace(m_ViewSpace);
            m_ViewSpace = XR_NULL_HANDLE;
        }
        m_ActionSpaceCreated = false;
        m_ViewSpaces.clear();
        m_ActionSpaces.clear();
        m_StaticRefSpaces.clear();
        m_RefToStageMap.clear();
        m_EyeToHmd.reset();
        if (m_Overlay)
        {
            m_Overlay->DestroySession(session);
        }
        if (m_CompositionFrameworkFactory)
        {
            m_CompositionFrameworkFactory->DestroySession(session);
        }

        const XrResult result = OpenXrApi::xrDestroySession(session);

        m_Session = XR_NULL_HANDLE;

        TraceLoggingWriteStop(local, "OpenXrLayer::xrDestroySession", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrCreateSwapchain(XrSession session,
                                            const XrSwapchainCreateInfo* createInfo,
                                            XrSwapchain* swapchain)
    {
        const XrResult result = OpenXrApi::xrCreateSwapchain(session, createInfo, swapchain);
        if (!m_Enabled || !m_Overlay)
        {
            return result;
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrCreateSwapchain", TLXArg(session, "Session"));

        if (createInfo->type != XR_TYPE_SWAPCHAIN_CREATE_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrAttachSessionActionSets", TLArg(false, "TypeCheck"));

            return XR_ERROR_VALIDATION_FAILURE;
        }
        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrCreateSwapchain",
                                TLArg(createInfo->createFlags, "CreateFlags"),
                                TLArg(createInfo->usageFlags, "UsageFlags"),
                                TLArg(createInfo->format, "Format"),
                                TLArg(createInfo->sampleCount, "SampleCount"),
                                TLArg(createInfo->width, "Width"),
                                TLArg(createInfo->height, "Height"),
                                TLArg(createInfo->faceCount, "FaceCount"),
                                TLArg(createInfo->arraySize, "ArraySize"),
                                TLArg(createInfo->mipCount, "MipCount"));

        if (XR_FAILED(result))
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrCreateSwapchain", TLArg(xr::ToCString(result), "Result"));

            return result;
        }
        Log("swapchain (%llu) created, with dimensions=%ux%u, format=%lld, "
            "createFlags=0x%x, usageFlags=0x%x, faceCount=%u, arraySize=%u, mipCount=%u, sampleCount=%u",
            *swapchain,
            createInfo->width,
            createInfo->height,
            createInfo->format,
            createInfo->createFlags,
            createInfo->usageFlags,
            createInfo->faceCount,
            createInfo->arraySize,
            createInfo->mipCount,
            createInfo->sampleCount);

        if (!m_Overlay->m_Initialized)
        {
            ErrorLog("%s: overlay not properly initialized", __FUNCTION__);
            TraceLoggingWriteStop(local, "OpenXrLayer::xrCreateSwapchain", TLArg(false, "Overlay_Initialized"));
            return result;
        }
        if (!m_CompositionFrameworkFactory)
        {
            ErrorLog("%s: composition framework not properly initialized", __FUNCTION__);
            TraceLoggingWriteStop(local, "OpenXrLayer::xrCreateSwapchain", TLArg(false, "Factory_Initialized"));

            return result;
        }

        m_Overlay->CreateSwapchain(*swapchain, createInfo);

        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrCreateSwapchain",
                              TLXArg(session, "Session"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrDestroySwapchain(XrSwapchain swapchain)
    {
        const XrResult result = OpenXrApi::xrDestroySwapchain(swapchain);
        if (!m_Enabled || !m_Overlay)
        {
            return result;
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrDestroySwapchain", TLXArg(swapchain, "Swapchain"));
        DebugLog("xrDestroySwapchain %llu", swapchain);

        if (XR_SUCCEEDED(result))
        {
            m_Overlay->DestroySwapchain(swapchain);
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::xrDestroySwapchain", TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrAcquireSwapchainImage(XrSwapchain swapchain,
                                                  const XrSwapchainImageAcquireInfo* acquireInfo,
                                                  uint32_t* index)
    {
        if (!m_Enabled || !m_Overlay)
        {
            return OpenXrApi::xrAcquireSwapchainImage(swapchain, acquireInfo, index);
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrAcquireSwapchainImage", TLXArg(swapchain, "Swapchain"));

        if (!m_Overlay->m_Initialized)
        {
            const XrResult result = OpenXrApi::xrAcquireSwapchainImage(swapchain, acquireInfo, index);
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrAcquireSwapchainImage",
                                  TLArg(false, "Overlay_Initialized"),
                                  TLArg(*index, "Index"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }
        const XrResult result = m_Overlay->AcquireSwapchainImage(swapchain, acquireInfo, index);

        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrAcquireSwapchainImage",
                              TLArg(*index, "Index"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
    {
        if (!m_Enabled || !m_Overlay)
        {
            return OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrReleaseSwapchainImage", TLXArg(swapchain, "Swapchain"));
        if (!m_Overlay->m_Initialized)
        {
            const XrResult result = OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrReleaseSwapchainImage",
                                  TLArg(false, "Overlay_Initialized"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }
        const XrResult result = m_Overlay->ReleaseSwapchainImage(swapchain, releaseInfo);
        TraceLoggingWriteStop(local, "OpenXrLayer::xrReleaseSwapchainImage", TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrGetCurrentInteractionProfile(XrSession session,
                                                         XrPath topLevelUserPath,
                                                         XrInteractionProfileState* interactionProfile)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrLayer::xrGetCurrentInteractionProfile",
                               TLXArg(session, "session"),
                               TLArg(topLevelUserPath, "Path"),
                               TLArg(getXrPath(topLevelUserPath).c_str(), "Readable"));

        const XrResult result =
            OpenXrApi::xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);

        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrGetCurrentInteractionProfile",
                              TLArg((XR_NULL_PATH != interactionProfile->interactionProfile
                                         ? getXrPath(interactionProfile->interactionProfile).c_str()
                                         : "XR_NULL_PATH"),
                                    "Profile"),
                              TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult
    OpenXrLayer::xrSuggestInteractionProfileBindings(XrInstance instance,
                                                     const XrInteractionProfileSuggestedBinding* suggestedBindings)
    {
        if (!m_Enabled || !m_PhysicalEnabled || m_SuppressInteraction)
        {
            return OpenXrApi::xrSuggestInteractionProfileBindings(instance, suggestedBindings);
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrSuggestInteractionProfileBindings", TLXArg(instance, "Instance"));

        if (suggestedBindings->type != XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrSuggestInteractionProfileBindings", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        const std::string profile = getXrPath(suggestedBindings->interactionProfile);
        Log("xrSuggestInteractionProfileBindings: %s", profile.c_str());
        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrSuggestInteractionProfileBindings",
                                TLArg(profile.c_str(), "InteractionProfile"));

        for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; i++)
        {
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::xrSuggestInteractionProfileBindings",
                                    TLXArg(suggestedBindings->suggestedBindings[i].action, "Action"),
                                    TLArg(getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str(), "Path"));

            DebugLog("binding: %s", getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str());
        }

        if (m_ActionSetAttached)
        {
            // detach and recreate action set and tracker space
            DestroyTrackerActions();
            Log("destroyed tracker action for recreation");
        }
        CreateTrackerActions("xrSuggestInteractionProfileBindings");

        XrInteractionProfileSuggestedBinding bindingProfiles = *suggestedBindings;
        std::vector<XrActionSuggestedBinding> bindings{};

        bindings.resize((uint32_t)bindingProfiles.countSuggestedBindings);
        memcpy(bindings.data(),
               bindingProfiles.suggestedBindings,
               bindingProfiles.countSuggestedBindings * sizeof(XrActionSuggestedBinding));

        const std::string trackerPath(m_SubActionPath + "/");
        const std::string posePath(trackerPath + "input/grip/pose");
        const std::string movePath(m_VirtualTrackerUsed ? trackerPath + m_ButtonPath.GetSubPath(profile, 0) : "");
        const std::string positionPath(m_VirtualTrackerUsed ? trackerPath + m_ButtonPath.GetSubPath(profile, 1) : "");
        const std::string hapticPath(trackerPath + "output/haptic");

        bool isTrackerPath{false};
        bool poseBindingOverriden{false};
        bool moveBindingOverriden{false};
        bool positionBindingOverriden{false};
        bool hapticBindingOverriden{false};
        for (XrActionSuggestedBinding& curBinding : bindings)
        {
            // find and override tracker pose action
            const std::string bindingPath = getXrPath(curBinding.binding);
            if (!bindingPath.compare(0, trackerPath.size(), trackerPath))
            {
                // path starts with user/hand/<side>/input
                isTrackerPath = true;

                auto tryOverride = [&curBinding, profile](XrAction action,
                                                          const std::string& path,
                                                          const std::string& name,
                                                          bool& overriden) {
                    if (getXrPath(curBinding.binding) == path)
                    {
                        curBinding.action = action;
                        overriden = true;
                        Log("Binding %s - %s overridden with %s action", profile.c_str(), path.c_str(), name.c_str());
                        return true;
                    }
                    return false;
                };

                if (tryOverride(m_PoseAction, posePath, "reference tracker", poseBindingOverriden))
                {
                    m_InteractionProfileSuggested = true;
                }

                if (m_VirtualTrackerUsed)
                {
                    tryOverride(m_MoveAction, movePath, "move", moveBindingOverriden);
                    tryOverride(m_PositionAction, positionPath, "position", positionBindingOverriden);
                    tryOverride(m_HapticAction, hapticPath, "haptic", hapticBindingOverriden);
                }
            }
        }

        auto addBinding = [this, &bindings, profile](XrAction action,
                                                     const std::string& path,
                                                     const std::string& name) {
            XrActionSuggestedBinding newBinding{action};
            if (const XrResult result = xrStringToPath(GetXrInstance(), path.c_str(), &newBinding.binding);
                XR_SUCCEEDED(result))
            {
                bindings.push_back(newBinding);
                Log("Binding %s - %s for %s action added", profile.c_str(), path.c_str(), name.c_str());
                return true;
            }
            else
            {
                ErrorLog("%s: unable to create XrPath from %s: %s", __FUNCTION__, path.c_str(), xr::ToCString(result));
                return false;
            }
        };

        if (isTrackerPath && !poseBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include pose -> add it
            if (addBinding(m_PoseAction, posePath, "reference tracker"))
            {
                m_InteractionProfileSuggested = true;
            }
        }
        if (m_VirtualTrackerUsed && isTrackerPath && !moveBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include move -> add it
            addBinding(m_MoveAction, movePath, "move");
        }
        if (m_VirtualTrackerUsed && isTrackerPath && !positionBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include position -> add it
            addBinding(m_PositionAction, positionPath, "position");
        }
        if (m_VirtualTrackerUsed && isTrackerPath && !hapticBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include haptic -> add it
            addBinding(m_HapticAction, hapticPath, "haptic");
        }

        bindingProfiles.suggestedBindings = bindings.data();
        bindingProfiles.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
        const XrResult result = OpenXrApi::xrSuggestInteractionProfileBindings(instance, &bindingProfiles);

        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrSuggestInteractionProfileBindings",
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
    {
        if (!m_Enabled || !m_PhysicalEnabled || m_SuppressInteraction)
        {
            return OpenXrApi::xrAttachSessionActionSets(session, attachInfo);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrAttachSessionActionSets", TLXArg(session, "Session"));
        Log("xrAttachSessionActionSets");

        if (attachInfo->type != XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrAttachSessionActionSets", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        for (uint32_t i = 0; i < attachInfo->countActionSets; i++)
        {
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::xrAttachSessionActionSets",
                                    TLXArg(attachInfo->actionSets[i], "ActionSet"));
        }

        SuggestInteractionProfiles("xrAttachSessionActionSets");

        XrSessionActionSetsAttachInfo chainAttachInfo = *attachInfo;
        std::vector<XrActionSet> newActionSets;

        newActionSets.resize((size_t)chainAttachInfo.countActionSets);
        memcpy(newActionSets.data(), chainAttachInfo.actionSets, chainAttachInfo.countActionSets * sizeof(XrActionSet));
        newActionSets.push_back(m_ActionSet);

        chainAttachInfo.actionSets = newActionSets.data();
        chainAttachInfo.countActionSets = static_cast<uint32_t>(newActionSets.size());

        const XrResult result = OpenXrApi::xrAttachSessionActionSets(session, &chainAttachInfo);
        Log("%u action set(s)%s attached: %s",
            chainAttachInfo.countActionSets,
            XR_SUCCEEDED(result) ? "" : " not",
            xr::ToCString(result));
        if (XR_ERROR_ACTIONSETS_ALREADY_ATTACHED == result)
        {
            Log("If you're using an application that does not support motion controllers, try disabling physical "
                "tracker (if you don't use it for mc) or enabling early initialization");
        }
        if (XR_SUCCEEDED(result))
        {
            m_ActionSetAttached = true;
        }

        TraceLoggingWriteStop(local, "OpenXrLayer::xrAttachSessionActionSets", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrCreateReferenceSpace(XrSession session,
                                                 const XrReferenceSpaceCreateInfo* createInfo,
                                                 XrSpace* space)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrCreateReferenceSpace(session, createInfo, space);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrCreateReferenceSpace", TLXArg(session, "Session"));

        if (createInfo->type != XR_TYPE_REFERENCE_SPACE_CREATE_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrCreateReferenceSpace", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrCreateReferenceSpace",
                                TLArg(xr::ToCString(createInfo->referenceSpaceType), "ReferenceSpaceType"),
                                TLArg(xr::ToString(createInfo->poseInReferenceSpace).c_str(), "PoseInReferenceSpace"));

        const XrResult result = OpenXrApi::xrCreateReferenceSpace(session, createInfo, space);
        if (XR_SUCCEEDED(result))
        {
            if (XR_REFERENCE_SPACE_TYPE_VIEW == createInfo->referenceSpaceType)
            {
                Log("creation of view space detected: %llu, offset pose: %s",
                    *space,
                    xr::ToString(createInfo->poseInReferenceSpace).c_str());

                // memorize view spaces
                m_ViewSpaces.insert(*space);
            }
            else if (XR_REFERENCE_SPACE_TYPE_LOCAL == createInfo->referenceSpaceType)
            {
                Log("creation of local reference space detected: %llu, offset pose: %s",
                    *space,
                    xr::ToString(createInfo->poseInReferenceSpace).c_str());
                AddStaticRefSpace(*space);
            }
            else if (XR_REFERENCE_SPACE_TYPE_STAGE == createInfo->referenceSpaceType)
            {
                Log("creation of stage reference space detected: %llu, offset pose: %s",
                    *space,
                    xr::ToString(createInfo->poseInReferenceSpace).c_str());
                AddStaticRefSpace(*space);
            }
        }

        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrCreateReferenceSpace",
                              TLXArg(*space, "Space"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrCreateActionSpace(XrSession session,
                                              const XrActionSpaceCreateInfo* createInfo,
                                              XrSpace* space)
    {
        const XrResult result = OpenXrApi::xrCreateActionSpace(session, createInfo, space);
        if (!m_Enabled)
        {
            return result;
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrLayer::xrCreateActionSpace",
                               TLXArg(session, "Session"),
                               TLXArg(*space, "Space"));
        if (createInfo->type != XR_TYPE_ACTION_SPACE_CREATE_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrCreateActionSpace", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        const std::string subActionPath{!createInfo->subactionPath ? "null" : getXrPath(createInfo->subactionPath)};
        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrCreateActionSpace",
                                TLArg(subActionPath.c_str(), "SubactionPath"));
        Log("creation of action space detected: %llu, sub action path: %s, pose: %s",
            *space,
            subActionPath.c_str(),
            xr::ToString(createInfo->poseInActionSpace).c_str());

        if (m_CompensateControllers)
        {
            Log("action space for motion controller compensation added: %llu", *space);
            m_ActionSpaces.insert(*space);
        }

        TraceLoggingWriteStop(local, "OpenXrLayer::xrCreateActionSpace", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrLayer::xrLocateSpace",
                               TLXArg(space, "Space"),
                               TLXArg(baseSpace, "BaseSpace"),
                               TLArg(time, "Time"));
        DebugLog("xrLocateSpace(%lld): space = %llu, baseSpace = %llu", time, space, baseSpace);

        if (location->type != XR_TYPE_SPACE_LOCATION)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrLocateSpace", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        // determine original location
        const XrResult result = OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
        if (XR_FAILED(result))
        {
            ErrorLog("%s: xrLocateSpace(%lld) failed: %s", __FUNCTION__, time, xr::ToCString(result));
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrLocateSpace",
                                  TLArg(xr::ToCString(result), "LocateSpaceResult"));
            return result;
        }

        std::unique_lock lock(m_FrameLock);

        const bool spaceView = isViewSpace(space);
        const bool baseView = isViewSpace(baseSpace);
        const bool spaceAction = isActionSpace(space);
        const bool baseAction = isActionSpace(baseSpace);

        const bool spaceComp = spaceView || (m_CompensateControllers && spaceAction);
        const bool baseComp = baseView || (m_CompensateControllers && baseAction);

        TraceLoggingWriteTagged(local,
                                "OpenXrLayerxrLocateSpace",
                                TLArg(spaceView, "SpaceView"),
                                TLArg(baseView, "BaseView"),
                                TLArg(spaceAction, "SpaceAction"),
                                TLArg(baseAction, "BaseAction"),
                                TLArg(spaceComp, "SpaceComp"),
                                TLArg(baseComp, "BaseComp"));

        if (m_Activated && ((spaceComp && !baseComp) || (!spaceComp && baseComp)))
        {
            DebugLog("xrLocateSpace(%lld): original pose = %s", time, xr::ToString(location->pose).c_str());
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayerxrLocateSpace",
                                    TLArg(xr::ToString(location->pose).c_str(), "OriginalPose"),
                                    TLArg(location->locationFlags, "LocationFlags"));

            // switch roles if base space is the one to be compensated
            const XrPosef poseToCompensate = spaceComp ? location->pose : Pose::Invert(location->pose);
            const XrSpace refSpaceForCompensation = spaceComp ? baseSpace : space;

            // manipulate pose using tracker
            XrPosef trackerDelta{Pose::Identity()}, refDelta{Pose::Identity()}, refToStage, stageToRef;
            ;
            bool apply = true;
            if (m_TestRotation)
            {
                TestRotation(&trackerDelta, time, false);
            }
            else if (((apply = m_Tracker->GetPoseDelta(trackerDelta, m_Session, time))) &&
                     GetRefToStage(refSpaceForCompensation, &refToStage, &stageToRef))
            {
                if (m_ModifierActive && (spaceView || baseView))
                {
                    const XrPosef poseStage = Pose::Multiply(poseToCompensate, stageToRef);
                    m_HmdModifier->Apply(trackerDelta, poseStage);
                }
                refDelta = xr::Normalize(Pose::Multiply(Pose::Multiply(stageToRef, trackerDelta), refToStage));
            }
            if (apply)
            {
                m_RecoveryStart = 0;

                location->pose = Pose::Multiply(location->pose, refDelta);

                if (baseComp)
                {
                    // TODO: verify calculation
                    Log("Please report the application in use to the oxrmc developer!");
                    // undo inversion (undo role switch)
                    location->pose = Pose::Invert(location->pose);
                }

                location->pose = xr::Normalize(location->pose);
            }
            else
            {
                RecoveryTimeOut(time);
            }

            if ((spaceView && !baseAction) || (baseView && !spaceAction))
            {
                // save pose for use in xrEndFrame, if there isn't one from xrLocateViews already
                m_DeltaCache.AddSample(time, trackerDelta, false);
            }
            DebugLog("xrLocateSpace(%lld): compensated pose = %s", time, xr::ToString(location->pose).c_str());
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayerxrLocateSpace",
                                    TLArg(xr::ToString(location->pose).c_str(), "CompensatedPose"));
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::xrLocateSpace", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrLocateViews(XrSession session,
                                        const XrViewLocateInfo* viewLocateInfo,
                                        XrViewState* viewState,
                                        uint32_t viewCapacityInput,
                                        uint32_t* viewCountOutput,
                                        XrView* views)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrLocateViews(session,
                                            viewLocateInfo,
                                            viewState,
                                            viewCapacityInput,
                                            viewCountOutput,
                                            views);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrLocateViews", TLXArg(session, "Session"));

        if (viewLocateInfo->type != XR_TYPE_VIEW_LOCATE_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrLocateViews", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }
        const XrTime displayTime = viewLocateInfo->displayTime;
        const XrSpace refSpace = viewLocateInfo->space;

        DebugLog("xrLocateViews(%lld): reference space = %llu", displayTime, refSpace);
        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrLocateViews",
                                TLArg(xr::ToCString(viewLocateInfo->viewConfigurationType), "ViewConfigurationType"),
                                TLArg(displayTime, "DisplayTime"),
                                TLXArg(refSpace, "Space"),
                                TLArg(viewCapacityInput, "ViewCapacityInput"));

        const XrResult result =
            OpenXrApi::xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);

        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrLocateViews",
                                TLArg(viewState->viewStateFlags, "ViewStateFlags"));

        if (!m_Activated)
        {
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrLocateViews",
                                  TLArg(false, "Activated"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }

        if (isViewSpace(refSpace))
        {
            DebugLog("xrLocateViews(%lld): omitting manipulation against view space (%llu)", displayTime, refSpace);
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrLocateViews",
                                  TLArg(true, "RefSpaceIsView"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }

        std::unique_lock lock(m_FrameLock);

        // store eye poses to avoid recalculation in xrEndFrame
        std::vector<XrPosef> originalEyePoses{};
        for (uint32_t i = 0; i < *viewCountOutput; i++)
        {
            originalEyePoses.push_back(views[i].pose);
        }
        // assumption: the first xrLocateView call within a frame is the one used for rendering
        m_EyeCache.AddSample(displayTime, originalEyePoses, false);

        if (!m_LegacyMode)
        {
            if (!m_EyeToHmd)
            {
                // determine eye poses
                std::vector<XrView> eyeViews;
                eyeViews.resize(*viewCountOutput, {XR_TYPE_VIEW});

                const XrViewLocateInfo offsetViewLocateInfo{viewLocateInfo->type,
                                                            nullptr,
                                                            viewLocateInfo->viewConfigurationType,
                                                            displayTime,
                                                            m_ViewSpace};

                const XrResult toHmdResult = OpenXrApi::xrLocateViews(session,
                                                                      &offsetViewLocateInfo,
                                                                      viewState,
                                                                      viewCapacityInput,
                                                                      viewCountOutput,
                                                                      eyeViews.data());

                if (SUCCEEDED(toHmdResult) && 0 < *viewCountOutput)
                {
                    m_EyeToHmd = std::make_unique<XrPosef>(Pose::Invert(eyeViews[0].pose));
                    TraceLoggingWriteTagged(local,
                                            "OpenXrLayer::xrLocateViews",
                                            TLArg(xr::ToString(*m_EyeToHmd).c_str(), "EyeToHmd"));
                }
                else
                {
                    ErrorLog("%s: unable to determine eyeToHmd pose");
                }
            }

            // manipulate pose using tracker
            XrPosef trackerDelta{Pose::Identity()};
            if (m_TestRotation)
            {
                TestRotation(&trackerDelta, displayTime, false);
            }
            else if (m_Tracker->GetPoseDelta(trackerDelta, m_Session, displayTime))
            {
                XrPosef refToStage, stageToRef;
                if (GetRefToStage(refSpace, &refToStage, &stageToRef))
                {
                    if (m_ModifierActive && m_EyeToHmd && 0 < *viewCountOutput)
                    {
                        // apply hmd pose modifier on delta
                        const XrPosef hmdPoseStage =
                            Pose::Multiply(Pose::Multiply(*m_EyeToHmd, views[0].pose), stageToRef);
                        m_HmdModifier->Apply(trackerDelta, hmdPoseStage);
                    }
                    const XrPosef refDelta = Pose::Multiply(Pose::Multiply(stageToRef, trackerDelta), refToStage);
                    for (uint32_t i = 0; i < *viewCountOutput; i++)
                    {
                        DebugLog("xrLocateView(%lld): eye (%u) original pose = %s",
                                 displayTime,
                                 i,
                                 xr::ToString(views[i].pose).c_str());
                        TraceLoggingWriteTagged(local,
                                                "OpenXrLayer::xrLocateViews",
                                                TLArg(i, "Index"),
                                                TLArg(xr::ToString(views[i].fov).c_str(), "Fov"),
                                                TLArg(xr::ToString(views[i].pose).c_str(), "OriginalViewPose"));

                        // apply manipulation
                        views[i].pose = xr::Normalize(Pose::Multiply(views[i].pose, refDelta));

                        DebugLog("xrLocateView(%lld): eye (%u) compensated pose = %s",
                                 displayTime,
                                 i,
                                 xr::ToString(views[i].pose).c_str());
                        TraceLoggingWriteTagged(local,
                                                "OpenXrLayer::xrLocateViews",
                                                TLArg(i, "Index"),
                                                TLArg(xr::ToString(views[i].pose).c_str(), "CompensatedViewPose"));
                    }
                }
            }
            else
            {
                RecoveryTimeOut(displayTime);
            }
            // sample from xrLocateView potentially overrides previous one
            m_DeltaCache.AddSample(displayTime, trackerDelta, true);

            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrLocateViews",
                                  TLArg(true, "Activated"),
                                  TLArg(false, "LegacyMode"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }

        // legacy mode using xrLocateSpace
        if (m_EyeOffsets.empty())
        {
            // determine eye poses
            const XrViewLocateInfo offsetViewLocateInfo{viewLocateInfo->type,
                                                        nullptr,
                                                        viewLocateInfo->viewConfigurationType,
                                                        displayTime,
                                                        m_ViewSpace};

            if (XR_SUCCEEDED(OpenXrApi::xrLocateViews(session,
                                                      &offsetViewLocateInfo,
                                                      viewState,
                                                      viewCapacityInput,
                                                      viewCountOutput,
                                                      views)))
            {
                for (uint32_t i = 0; i < *viewCountOutput; i++)
                {
                    m_EyeOffsets.push_back(views[i]);

                    TraceLoggingWriteTagged(local,
                                            "OpenXrLayer::xrLocateViews",
                                            TLArg(i, "Index"),
                                            TLArg(i, "Index"),
                                            TLArg(xr::ToString(views[i].fov).c_str(), "Offset_Fov"),
                                            TLArg(xr::ToString(views[i].pose).c_str(), "Offset_ViewPose"));
                }
            }
        }

        // unlock for xrLocateSpace
        lock.unlock();

        // manipulate reference space location
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        if (XR_SUCCEEDED(xrLocateSpace(m_ViewSpace, refSpace, displayTime, &location)))
        {
            for (uint32_t i = 0; i < *viewCountOutput; i++)
            {
                DebugLog("xrLocateView(%lld): eye (%u) original pose = %s",
                         displayTime,
                         i,
                         xr::ToString(views[i].pose).c_str());
                TraceLoggingWriteTagged(local,
                                        "OpenXrLayer::xrLocateViews",
                                        TLArg(i, "Index"),
                                        TLArg(xr::ToString(views[i].fov).c_str(), "Fov"),
                                        TLArg(xr::ToString(views[i].pose).c_str(), "OriginalViewPose"));

                // apply manipulation
                views[i].pose = xr::Normalize(Pose::Multiply(m_EyeOffsets[i].pose, location.pose));

                DebugLog("xrLocateView(%lld): eye (%u) compensated pose = %s",
                         displayTime,
                         i,
                         xr::ToString(views[i].pose).c_str());
                TraceLoggingWriteTagged(local,
                                        "OpenXrLayer::xrLocateViews",
                                        TLArg(i, "Index"),
                                        TLArg(xr::ToString(views[i].pose).c_str(), "CompensatedViewPose"));
            }
        }
        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrLocateViews",
                              TLArg(true, "Activated"),
                              TLArg(true, "LegacyMode"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
    {
        if (!m_Enabled || !m_PhysicalEnabled || m_SuppressInteraction)
        {
            return OpenXrApi::xrSyncActions(session, syncInfo);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrSyncActions", TLXArg(session, "Session"));

        if (syncInfo->type != XR_TYPE_ACTIONS_SYNC_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrSyncActions", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }
        for (uint32_t i = 0; i < syncInfo->countActiveActionSets; i++)
        {
            const auto& [actionSet, subactionPath] = syncInfo->activeActionSets[i];
            const std::string path = XR_NULL_PATH == subactionPath ? "XR_NULL_PATH" : getXrPath(subactionPath);
            DebugLog("xrSyncActions: action set %llu, path %s", actionSet, path.c_str());
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::xrSyncActions",
                                    TLXArg(actionSet, "ActionSet"),
                                    TLArg(path.c_str(), "SubactionPath"));
        }

        AttachActionSet("xrSyncActions");

        XrActionsSyncInfo chainSyncInfo = *syncInfo;
        std::vector<XrActiveActionSet> newActiveActionSets;
        if (m_ActionSet != XR_NULL_HANDLE)
        {
            newActiveActionSets.resize(static_cast<size_t>(chainSyncInfo.countActiveActionSets) + 1);
            memcpy(newActiveActionSets.data(),
                   chainSyncInfo.activeActionSets,
                   chainSyncInfo.countActiveActionSets * sizeof(XrActiveActionSet));
            uint32_t nextActionSetSlot = chainSyncInfo.countActiveActionSets;

            newActiveActionSets[nextActionSetSlot].actionSet = m_ActionSet;
            newActiveActionSets[nextActionSetSlot++].subactionPath = XR_NULL_PATH;

            chainSyncInfo.activeActionSets = newActiveActionSets.data();
            chainSyncInfo.countActiveActionSets = nextActionSetSlot;

            DebugLog("xrSyncActions: action set %llu, path XR_NULL_PATH added", m_ActionSet);
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::xrSyncActions",
                                    TLXArg(m_ActionSet, "ActionSet Attached"),
                                    TLArg(chainSyncInfo.countActiveActionSets, "ActionSet Count"));
        }
        const XrResult result = OpenXrApi::xrSyncActions(session, &chainSyncInfo);
        DebugLog("xrSyncAction: %s", xr::ToCString(result));
        m_XrSyncCalled = true;

        TraceLoggingWriteStop(local, "OpenXrLayer::xrSyncActions", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState)
    {
        if (!m_Enabled)
        {
           return OpenXrApi::xrWaitFrame(session, frameWaitInfo, frameState);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrWaitFrame", TLXArg(session, "Session"));

        const XrResult result = OpenXrApi::xrWaitFrame(session, frameWaitInfo, frameState);

        DebugLog("xrWaitFrame predicted time: %lld, predicted period: %lld",
                 frameState->predictedDisplayTime,
                 frameState->predictedDisplayPeriod);
        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrWaitFrame",
                              TLArg(frameState->predictedDisplayTime, "PredictedTime"),
                              TLArg(frameState->predictedDisplayPeriod, "PredictedPPeriod"),
                              TLArg(frameState->shouldRender, "ShouldRender"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult OpenXrLayer::xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
    {
        if (!m_Enabled)
        {
           return OpenXrApi::xrBeginFrame(session, frameBeginInfo);
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrBeginFrame", TLXArg(session, "Session"));
        DebugLog("xrBeginFrame");

        std::unique_lock lock(m_FrameLock);

        if (m_VarjoPollWorkaround && m_Enabled && m_PhysicalEnabled && !m_SuppressInteraction)
        {
           TraceLoggingWriteTagged(local, "OpenXrLayer::xrBeginFrame", TLArg(true, "PollWorkaround"));

           // call xrPollEvent (if the app hasn't already) to acquire focus
           XrEventDataBuffer buf{XR_TYPE_EVENT_DATA_BUFFER};
           OpenXrApi::xrPollEvent(GetXrInstance(), &buf);
        }

        if (m_Overlay && m_Overlay->m_Initialized)
        {
           m_Overlay->ReleaseAllSwapChainImages();
        }

        const XrResult result = OpenXrApi::xrBeginFrame(session, frameBeginInfo);

        TraceLoggingWriteStop(local, "OpenXrLayer::xrBeginFrame", TLArg(xr::ToCString(result), "Result"));

        return result;
    }

    XrResult OpenXrLayer::xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
    {
        if (!m_Enabled || !isSessionHandled(session))
        {
            return OpenXrApi::xrEndFrame(session, frameEndInfo);
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::xrEndFrame", TLXArg(session, "Session"));

        if (frameEndInfo->type != XR_TYPE_FRAME_END_INFO)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::xrEndFrame", TLArg(false, "TypeCheck"));
            return XR_ERROR_VALIDATION_FAILURE;
        }

        XrTime time = frameEndInfo->displayTime;

        DebugLog("xrEndFrame(%lld)", time);
        TraceLoggingWriteTagged(local,
                                "OpenXrLayer::xrEndFrame",
                                TLArg(time, "DisplayTime"),
                                TLArg(xr::ToCString(frameEndInfo->environmentBlendMode), "EnvironmentBlendMode"));

        std::unique_lock lock(m_FrameLock);

        // update last frame time
        if (m_UpdateRefSpaceTime >= std::exchange(m_LastFrameTime, time) && m_UpdateRefSpaceTime < time)
        {
            // (re)locate all static reference spaces
            for (const XrSpace& space : m_StaticRefSpaces)
            {
                LocateRefSpace(space);
            }
        }

        XrFrameEndInfo chainFrameEndInfo = *frameEndInfo;

        XrPosef delta{Pose::Identity()};
        std::vector<XrPosef> cachedEyePoses;
        if (m_Activated)
        {
            delta = m_DeltaCache.GetSample(time);
            m_DeltaCache.CleanUp(time);
            cachedEyePoses = m_UseEyeCache ? m_EyeCache.GetSample(time) : std::vector<XrPosef>();
            m_EyeCache.CleanUp(time);
        }

        if (m_Overlay)
        {
            m_Overlay
                ->DrawOverlay(m_Tracker->GetReferencePose(), delta, m_Activated, session, &chainFrameEndInfo, this);
            m_Overlay->ReleaseAllSwapChainImages();
        }

        if (!m_Activated)
        {
            if (m_Tracker->m_Calibrated)
            {
                if (m_RecorderActive)
                {
                    // request pose delta to force recording
                    m_Tracker->GetPoseDelta(delta, session, time);
                }
                if (!m_SuppressInteraction)
                {
                    m_Tracker->ApplyCorManipulation(session, time);
                }
            }
            m_Input->HandleKeyboardInput(time);
            if (m_AutoActivator)
            {
                m_AutoActivator->ActivateIfNecessary(time);
            }

            m_XrSyncCalled = false;

            XrResult result = OpenXrApi::xrEndFrame(session, &chainFrameEndInfo);

            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::xrEndFrame",
                                  TLArg(false, "Activated"),
                                  TLArg(xr::ToCString(result), "Result"));
            return result;
        }

        std::vector<const XrCompositionLayerBaseHeader*> resetLayers{};
        std::vector<XrCompositionLayerProjection*> resetProjectionLayers{};
        std::vector<XrCompositionLayerQuad*> resetQuadLayers{};
        std::vector<std::vector<XrCompositionLayerProjectionView>*> resetViews{};

        // use pose cache for reverse calculation
        for (uint32_t i = 0; i < chainFrameEndInfo.layerCount; i++)
        {
            XrCompositionLayerBaseHeader baseHeader = *chainFrameEndInfo.layers[i];
            XrCompositionLayerBaseHeader* resetBaseHeader{nullptr};
            if (XR_TYPE_COMPOSITION_LAYER_PROJECTION == baseHeader.type)
            {
                DebugLog("xrEndFrame: projection layer index: %u, space: %llu", i, baseHeader.space);

                const auto* projectionLayer =
                    reinterpret_cast<const XrCompositionLayerProjection*>(chainFrameEndInfo.layers[i]);

                TraceLoggingWriteTagged(local,
                                        "OpenXrLayer::EndFrame",
                                        TLArg(projectionLayer->layerFlags, "ProjectionLayerFlags"),
                                        TLXArg(projectionLayer->space, "ProjectionLayerSpace"));

                // apply reverse manipulation to projection-layer pose
                XrPosef refToStage, stageToRef;
                if (!GetRefToStage(projectionLayer->space, &refToStage, &stageToRef))
                {
                    ErrorLog(
                        "%s: unable to determine reference/stage transformation pose for projection layer space: %llu",
                        __FUNCTION__,
                        projectionLayer->space);

                    resetLayers.push_back(chainFrameEndInfo.layers[i]);
                    continue;
                }

                auto projectionViews = new std::vector<XrCompositionLayerProjectionView>{};
                resetViews.push_back(projectionViews);
                projectionViews->resize(projectionLayer->viewCount);
                memcpy(projectionViews->data(),
                       projectionLayer->views,
                       projectionLayer->viewCount * sizeof(XrCompositionLayerProjectionView));

                for (uint32_t j = 0; j < projectionLayer->viewCount; j++)
                {
                    DebugLog("xrEndFrame: original view(%u) pose = %s",
                             j,
                             xr::ToString((*projectionViews)[j].pose).c_str());
                    TraceLoggingWriteTagged(
                        local,
                        "OpenXrLayer::xrEndFrame",
                        TLArg(j, "Index"),
                        TLArg(xr::ToString((*projectionViews)[j].pose).c_str(), "OriginalViewPose"),
                        TLXArg((*projectionViews)[j].subImage.swapchain, "Swapchain"),
                        TLArg((*projectionViews)[j].subImage.imageArrayIndex, "ImageArrayIndex"),
                        TLArg(xr::ToString((*projectionViews)[j].subImage.imageRect).c_str(), "ImageRect"),
                        TLArg(xr::ToString((*projectionViews)[j].fov).c_str(), "Fov"));

                    XrPosef revertedEyePose =
                        m_UseEyeCache
                            ? cachedEyePoses[j]
                            : xr::Normalize(Pose::Multiply(
                                  (*projectionViews)[j].pose,
                                  Pose::Invert(Pose::Multiply(Pose::Multiply(stageToRef, delta), refToStage))));

                    (*projectionViews)[j].pose = revertedEyePose;
                    DebugLog("xrEndFrame: reverted view(%u) pose = %s", j, xr::ToString(revertedEyePose).c_str());
                    TraceLoggingWriteTagged(
                        local,
                        "OpenXrLayer::xrEndFrame",
                        TLArg(j, "Index"),
                        TLArg(xr::ToString((*projectionViews)[j].pose).c_str(), "RevertedViewPose"));
                }

                // create layer with reset view poses
                const auto resetProjectionLayer = new XrCompositionLayerProjection{projectionLayer->type,
                                                                                   projectionLayer->next,
                                                                                   projectionLayer->layerFlags,
                                                                                   projectionLayer->space,
                                                                                   projectionLayer->viewCount,
                                                                                   projectionViews->data()};
                resetProjectionLayers.push_back(resetProjectionLayer);
                resetBaseHeader = reinterpret_cast<XrCompositionLayerBaseHeader*>(resetProjectionLayer);
            }
            else if (XR_TYPE_COMPOSITION_LAYER_QUAD == baseHeader.type && !isViewSpace(baseHeader.space))
            {
                // compensate quad layers unless they are relative to view space
                DebugLog("xrEndFrame: quad layer index: %u, space: %llu", i, baseHeader.space);

                const auto* quadLayer = reinterpret_cast<const XrCompositionLayerQuad*>(chainFrameEndInfo.layers[i]);

                DebugLog("xrEndFrame: original quad layer pose = %s", xr::ToString(quadLayer->pose).c_str());
                TraceLoggingWriteTagged(local,
                                        "OpenXrLayer::xrEndFrame",
                                        TLArg("QuadLayer", "Type"),
                                        TLArg(quadLayer->layerFlags, "QuadLayerFlags"),
                                        TLXArg(quadLayer->space, "QuadLayerSpace"),
                                        TLArg(xr::ToString(quadLayer->pose).c_str(), "QuadLayerPose"));

                // apply reverse manipulation to quad layer pose
                XrPosef refToStage, stageToRef;
                if (!GetRefToStage(quadLayer->space, &refToStage, &stageToRef))
                {
                    ErrorLog("%s: unable to determine reference/stage transformation pose for quad layer space: %llu",
                             __FUNCTION__,
                             quadLayer->space);
                    resetLayers.push_back(chainFrameEndInfo.layers[i]);
                    continue;
                }

                XrPosef revertedPose = xr::Normalize(
                    Pose::Multiply(Pose::Multiply(Pose::Multiply(quadLayer->pose, stageToRef), Pose::Invert(delta)),
                                   refToStage));

                DebugLog("xrEndFrame: reverted quad layer pose = %s", xr::ToString(revertedPose).c_str());
                TraceLoggingWriteTagged(local,
                                        "OpenXrLayer::xrEndFrame",
                                        TLArg(xr::ToString(revertedPose).c_str(), "QuadLayerRevertedPose"));

                // create quad layer with reset pose
                auto* const resetQuadLayer = new XrCompositionLayerQuad{quadLayer->type,
                                                                        quadLayer->next,
                                                                        quadLayer->layerFlags,
                                                                        quadLayer->space,
                                                                        quadLayer->eyeVisibility,
                                                                        quadLayer->subImage,
                                                                        revertedPose,
                                                                        quadLayer->size};
                resetQuadLayers.push_back(resetQuadLayer);
                resetBaseHeader = reinterpret_cast<XrCompositionLayerBaseHeader*>(resetQuadLayer);
            }
            if (resetBaseHeader)
            {
                resetLayers.push_back(resetBaseHeader);
            }
            else
            {
                resetLayers.push_back(chainFrameEndInfo.layers[i]);
            }
        }
        m_Input->HandleKeyboardInput(time);

        m_XrSyncCalled = false;

        XrFrameEndInfo resetFrameEndInfo{chainFrameEndInfo.type,
                                         chainFrameEndInfo.next,
                                         time,
                                         chainFrameEndInfo.environmentBlendMode,
                                         chainFrameEndInfo.layerCount,
                                         resetLayers.data()};

        XrResult result = OpenXrApi::xrEndFrame(session, &resetFrameEndInfo);

        // clean up memory
        for (auto projection : resetProjectionLayers)
        {
            delete projection;
        }
        for (auto quad : resetQuadLayers)
        {
            delete quad;
        }
        for (auto views : resetViews)
        {
            delete views;
        }

        TraceLoggingWriteStop(local,
                              "OpenXrLayer::xrEndFrame",
                              TLArg(true, "Activated"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    void OpenXrLayer::SetForwardRotation(const XrPosef& pose) const
    {
        m_HmdModifier->SetFwdToStage(pose);
    }

    bool OpenXrLayer::GetRefToStage(XrSpace space, XrPosef* refToStage, XrPosef* stageToRef)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::GetRefToStage", TLXArg(space, "Space"));

        const auto it = m_RefToStageMap.find(space);
        if (it == m_RefToStageMap.end())
        {
            // fallback for dynamic (= action) ref space
            if (const auto maybeRefToStage = LocateRefSpace(space); maybeRefToStage.has_value())
            {
                if (refToStage)
                {
                    *refToStage = maybeRefToStage.value();
                    TraceLoggingWriteTagged(local,
                                            "OpenXrLayer::GetRefToStage",
                                            TLArg(xr::ToString(*refToStage).c_str(), "RefToStage"));
                }
                if (stageToRef)
                {
                    *stageToRef = Pose::Invert(maybeRefToStage.value());
                    TraceLoggingWriteTagged(local,
                                            "OpenXrLayer::GetRefToStage",
                                            TLArg(xr::ToString(*stageToRef).c_str(), "StageToRef"));
                }
                TraceLoggingWriteStop(local, "OpenXrLayer::GetRefToStage", TLArg(true, "Fallback"));
                return true;
            }
            ErrorLog("%s: unable to locate reference space");
            TraceLoggingWriteStop(local, "OpenXrLayer::GetRefToStage", TLArg(false, "Fallback"));
            return false;
        }
        if (refToStage)
        {
            *refToStage = it->second.first;
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::GetRefToStage",
                                    TLArg(xr::ToString(*refToStage).c_str(), "RefToStage"));
        }
        if (stageToRef)
        {
            *stageToRef = it->second.second;
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::GetRefToStage",
                                    TLArg(xr::ToString(*stageToRef).c_str(), "StageToRef"));
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::GetRefToStage", TLArg(true, "Success"));
        return true;
    }

    std::shared_ptr<graphics::ICompositionFrameworkFactory> OpenXrLayer::GetCompositionFactory()
    {
        return m_CompositionFrameworkFactory;
    }

    // private
    void OpenXrLayer::CreateStageSpace()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::CreateStageSpace");

        if (m_StageSpace == XR_NULL_HANDLE)
        {
            // create internal stage reference space.
            constexpr XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                            nullptr,
                                                            XR_REFERENCE_SPACE_TYPE_STAGE,
                                                            Pose::Identity()};
            if (const XrResult result = OpenXrApi::xrCreateReferenceSpace(m_Session, &createInfo, &m_StageSpace);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrCreateReferenceSpace(stage) failed: %s",
                         __FUNCTION__,
                         xr::ToCString(result));
                TraceLoggingWriteStop(local,
                                      "OpenXrLayer::CreateStageSpace",
                                      TLPArg(xr::ToCString(result), "Result_xrCreateReferenceSpace"));
                m_Initialized = false;
                return;
            }
            Log("internal stage space created: %llu", m_StageSpace);
            TraceLoggingWriteTagged(local, "OpenXrLayer::CreateStageSpace", TLArg(true, "StageSpoaceCreated"));
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::CreateStageSpace");
    }

    void OpenXrLayer::CreateViewSpace()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::CreateViewSpace");

        if (m_ViewSpace == XR_NULL_HANDLE)
        {
            // create internal view reference space.
            constexpr XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                            nullptr,
                                                            XR_REFERENCE_SPACE_TYPE_VIEW,
                                                            Pose::Identity()};

            if (const XrResult result = OpenXrApi::xrCreateReferenceSpace(m_Session, &createInfo, &m_ViewSpace);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrCreateReferenceSpace(view) failed: %s",
                         __FUNCTION__,
                         xr::ToCString(result));
                TraceLoggingWriteStop(local,
                                      "OpenXrLayer::CreateViewSpace",
                                      TLPArg(xr::ToCString(result), "Result_xrCreateReferenceSpace"));
                m_Initialized = false;
                return;
            }
            m_ViewSpaces.insert(m_ViewSpace);

            DebugLog("internal view space created: %llu", m_ViewSpace);
            TraceLoggingWriteTagged(local, "OpenXrLayer::CreateViewSpace", TLArg(true, "ViewSpaceCreated"));
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::CreateViewSpace");
    }

    void OpenXrLayer::AddStaticRefSpace(const XrSpace space)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::AddStaticRefSpace", TLXArg(space, "Space"));

        m_StaticRefSpaces.insert(space);

        if (0 != m_LastFrameTime)
        {
            LocateRefSpace(space);
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::AddStaticRefSpace");
    }

    std::optional<XrPosef> OpenXrLayer::LocateRefSpace(const XrSpace space)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrLayer::LocateStaticRefSpace",
                               TLXArg(space, "Space"),
                               TLArg(m_LastFrameTime, "Time"));

        if (m_StageSpace == XR_NULL_HANDLE)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::LocateStaticRefSpace", TLArg(false, "StageSpace"));
            return std::optional<XrPosef>();
        }

        if (space == XR_NULL_HANDLE)
        {
            TraceLoggingWriteStop(local, "OpenXrLayer::LocateStaticRefSpace", TLArg(false, "RefSpace"));
            return std::optional<XrPosef>();
        }

        if (m_StageSpace == space)
        {
            DebugLog("RefToStage(%llu = internal stage) = identity", space);
            TraceLoggingWriteStop(local, "OpenXrLayer::LocateStaticRefSpace", TLArg(true, "Success"), TLArg(true, "IsStageSpace"));
            return Pose::Identity();
        }

        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        const XrResult result = OpenXrApi::xrLocateSpace(m_StageSpace, space, m_LastFrameTime, &location);
        if (XR_FAILED(result))
        {
            ErrorLog("%s: unable to locate local reference space (%llu) in stage reference space (%llu): %s",
                     __FUNCTION__,
                     space,
                     m_StageSpace,
                     xr::ToCString(result));
            TraceLoggingWriteStop(local,
                                  "OpenXrLayer::LocateStaticRefSpace",
                                  TLArg(xr::ToCString(result), "LocateStageSpace"));
            return std::optional<XrPosef>();
        }
        if (!Pose::IsPoseValid(location.locationFlags))
        {
            ErrorLog("%s: pose of local space in stage space not valid. locationFlags: %llu",
                     __FUNCTION__,
                     location.locationFlags);
            TraceLoggingWriteStop(local, "OpenXrLayer::LocateStaticRefSpace", TLArg(false, "PoseValid"));
            return std::optional<XrPosef>();
        }
        if (m_StaticRefSpaces.contains(space))
        {
            m_RefToStageMap[space] = {location.pose, Pose::Invert(location.pose)};
        }
        DebugLog("RefToStage(%llu) = %s", space, xr::ToString(location.pose).c_str());
        TraceLoggingWriteStop(local, "OpenXrLayer::LocateStaticRefSpace", TLArg(true, "Success"));
        return location.pose;
    }

    bool OpenXrLayer::isSystemHandled(XrSystemId systemId) const
    {
        return systemId == m_systemId;
    }

    bool OpenXrLayer::isSessionHandled(XrSession session) const
    {
        return session == m_Session;
    }

    bool OpenXrLayer::isViewSpace(XrSpace space) const
    {
        return m_ViewSpaces.contains(space);
    }

    bool OpenXrLayer::isActionSpace(XrSpace space) const
    {
        return m_ActionSpaces.contains(space);
    }

    uint32_t OpenXrLayer::GetNumViews() const
    {
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO == m_ViewConfigType                                ? 1
               : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO == m_ViewConfigType                            ? 2
               : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO == m_ViewConfigType                        ? 4
               : XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT == m_ViewConfigType ? 1
                                                                                                          : 0;
    }

    bool OpenXrLayer::CreateTrackerActions(const std::string& caller)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::CreateTrackerActions", TLArg(caller.c_str(), "Caller"));
        DebugLog("CreateTrackerAction %s", caller.c_str());

        bool success = true;
        if (m_PhysicalEnabled && !m_SuppressInteraction)
        {
            if (!m_ActionsCreated)
            {
                XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO, nullptr};
                strcpy_s(actionSetCreateInfo.actionSetName, "general_tracker_set");
                strcpy_s(actionSetCreateInfo.localizedActionSetName, "General tracker Set");
                actionSetCreateInfo.priority = 0;
                if (const XrResult result = xrCreateActionSet(GetXrInstance(), &actionSetCreateInfo, &m_ActionSet);
                    XR_SUCCEEDED(result))
                {
                    DebugLog("created action set: %llu", m_ActionSet);
                    TraceLoggingWriteTagged(local,
                                            "OpenXrLayer::CreateTrackerActions",
                                            TLXArg(m_ActionSet, "CreateActionSet"));
                }
                else
                {
                    ErrorLog("%s: unable to create action set: %s", __FUNCTION__, xr::ToCString(result));
                    TraceLoggingWriteTagged(local,
                                            "OpenXrLayer::CreateTrackerActions",
                                            TLArg(xr::ToCString(result), "CreateActionSet"));
                    success = false;
                }

                auto createAction =
                    [this](XrAction* action, XrActionType type, const char* name, const char* localName) {
                        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO, nullptr};
                        strcpy_s(actionCreateInfo.actionName, name);
                        strcpy_s(actionCreateInfo.localizedActionName, localName);
                        actionCreateInfo.actionType = type;
                        actionCreateInfo.countSubactionPaths = 1;
                        actionCreateInfo.subactionPaths = &m_XrSubActionPath;
                        if (const XrResult result = xrCreateAction(m_ActionSet, &actionCreateInfo, action);
                            XR_FAILED(result))
                        {
                            ErrorLog("%s: unable to create action: %s", __FUNCTION__, name, xr::ToCString(result));
                            return false;
                        }
                        DebugLog("CreateTrackerActions: created %s action: %llu", name, *action);
                        return true;
                    };

                if (!createAction(&m_PoseAction, XR_ACTION_TYPE_POSE_INPUT, "pose", "Pose"))
                {
                    success = false;
                }

                if (m_VirtualTrackerUsed)
                {
                    createAction(&m_MoveAction, XR_ACTION_TYPE_BOOLEAN_INPUT, "move", "Move");
                    createAction(&m_PositionAction, XR_ACTION_TYPE_BOOLEAN_INPUT, "position", "Position");
                    createAction(&m_HapticAction, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Haptic");

                    TraceLoggingWriteTagged(local,
                                            "OpenXrLayer::CreateTrackerActions",
                                            TLXArg(m_MoveAction, "MoveAction"),
                                            TLXArg(m_PositionAction, "ButtonAction"),
                                            TLXArg(m_HapticAction, "HapticAction"));
                }
                m_ActionsCreated = success;
            }
            if (m_ActionsCreated && !m_ActionSpaceCreated && XR_NULL_HANDLE != m_Session)
            {
                XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr};
                actionSpaceCreateInfo.action = m_PoseAction;
                actionSpaceCreateInfo.subactionPath = m_XrSubActionPath;
                actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
                if (const XrResult result = GetInstance()->OpenXrApi::xrCreateActionSpace(m_Session,
                                                                                          &actionSpaceCreateInfo,
                                                                                          &m_TrackerSpace);
                    XR_SUCCEEDED(result))
                {
                    Log("created action space: %llu", m_TrackerSpace);
                }
                else
                {
                    ErrorLog("%s: unable to create action space: %s", __FUNCTION__, xr::ToCString(result));
                    success = false;
                }

                TraceLoggingWriteTagged(local,
                                        "OpenXrLayer::CreateTrackerActions",
                                        TLXArg(m_TrackerSpace, "ActionSpace"));
                m_ActionSpaceCreated = success;
            }
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::CreateTrackerActions", TLArg(success, "Success"));

        return success;
    }

    void OpenXrLayer::DestroyTrackerActions()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::DestroyTrackerActions");
        DebugLog("DestroyTrackerActions");

        m_ActionsCreated = false;
        m_ActionSpaceCreated = false;
        m_ActionSetAttached = false;
        m_InteractionProfileSuggested = false;
        if (XR_NULL_HANDLE != m_ActionSet)
        {
            TraceLoggingWriteTagged(local, "OpenXrLayer::DestroyTrackerActions", TLXArg(m_ActionSet, "Action Set"));
            if (const XrResult result = GetInstance()->xrDestroyActionSet(m_ActionSet); XR_FAILED(result))
            {
                ErrorLog("%s: unable to destroy action set (%llu): %s", __FUNCTION__, m_ActionSet, xr::ToCString(result));
            }
            m_ActionSet = XR_NULL_HANDLE;
        }
        if (XR_NULL_HANDLE != m_TrackerSpace)
        {
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::DestroyTrackerActions",
                                    TLXArg(m_TrackerSpace, "Action Space"));
            if (const XrResult result = GetInstance()->xrDestroySpace(m_TrackerSpace); XR_FAILED(result))
            {
                ErrorLog("%s: unable to destroy action space (%llu): %s",
                         __FUNCTION__,
                         m_TrackerSpace,
                         xr::ToCString(result));
            }
            m_TrackerSpace = XR_NULL_HANDLE;
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::DestroyTrackerActions");
    }

    bool OpenXrLayer::AttachActionSet(const std::string& caller)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::AttachActionSet", TLArg(caller.c_str(), "Caller"));

        bool success{true};
        if (m_PhysicalEnabled && !m_SuppressInteraction && !m_ActionSetAttached)
        {
            constexpr XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            const XrResult result = xrAttachSessionActionSets(m_Session, &actionSetAttachInfo);
            if (XR_SUCCEEDED(result))
            {
                Log("action set attached during %s", caller.c_str());
            }
            else
            {
                ErrorLog("%s: xrAttachSessionActionSets during %s failed", __FUNCTION__, caller.c_str());
                success = false;
            }
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::AttachActionSet", TLArg(success, "Success"));

        return success;
    }

    void OpenXrLayer::SuggestInteractionProfiles(const std::string& caller)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::SuggestInteractionProfiles", TLPArg(caller.c_str(), "Caller"));
        DebugLog("SuggestInteractionProfiles %s", caller.c_str());

        CreateTrackerActions("SuggestInteractionProfiles");

        if (!m_InteractionProfileSuggested && !m_SuppressInteraction)
        {
            TraceLoggingWriteTagged(local,
                                    "OpenXrLayer::SuggestInteractionProfiles",
                                    TLArg(m_InteractionProfileSuggested, "InteractionProfileSuggested"),
                                    TLArg(m_SuppressInteraction, "SuppressInteraction"));

            // suggest fallback in case application does not suggest any bindings
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                                                                   nullptr};
            std::vector<XrActionSuggestedBinding> bindings{};
            XrActionSuggestedBinding poseBinding{m_PoseAction};
            XrActionSuggestedBinding moveBinding{m_MoveAction};
            XrActionSuggestedBinding positionBinding{m_PositionAction};
            XrActionSuggestedBinding hapticBinding{m_HapticAction};

            const std::string profile{m_ViveTracker.active ? "/interaction_profiles/htc/vive_tracker_htcx"
                                                           : "/interaction_profiles/khr/simple_controller"};

            if (const XrResult profileResult =
                    xrStringToPath(GetXrInstance(), profile.c_str(), &suggestedBindings.interactionProfile);
                XR_FAILED(profileResult))
            {
                ErrorLog("%s: unable to create XrPath from %s: %s", __FUNCTION__, profile.c_str(), xr::ToCString(profileResult));
            }
            else
            {
                const std::string trackerPath(m_SubActionPath + "/");
                auto addBinding = [this, &bindings](const std::string& path,
                                                    XrActionSuggestedBinding& binding,
                                                    const std::string& action) {
                    if (const XrResult result = xrStringToPath(GetXrInstance(), path.c_str(), &binding.binding);
                        XR_FAILED(result))
                    {
                        ErrorLog("%s: unable to create XrPath from %s: %s",
                                 __FUNCTION__,
                                 path.c_str(),
                                 xr::ToCString(result));
                        return false;
                    }
                    bindings.push_back(binding);
                    DebugLog("SuggestInteractionProfiles: added binding for %s action (%llu) with path: %s",
                             action.c_str(),
                             binding.action,
                             path.c_str());
                    return true;
                };

                if (addBinding(trackerPath + "input/grip/pose", poseBinding, "pose"))
                {
                    // add move, position and haptic bindings for controller
                    if (m_VirtualTrackerUsed)
                    {
                        addBinding(trackerPath + m_ButtonPath.GetSubPath(profile, 0), moveBinding, "move");
                        addBinding(trackerPath + m_ButtonPath.GetSubPath(profile, 1), positionBinding, "position");
                        addBinding(trackerPath + "output/haptic", hapticBinding, "haptic");
                    }
                    suggestedBindings.suggestedBindings = bindings.data();
                    suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
                    if (const XrResult suggestResult =
                            OpenXrApi::xrSuggestInteractionProfileBindings(GetXrInstance(), &suggestedBindings);
                        XR_FAILED(suggestResult))
                    {
                        ErrorLog("%s: unable to suggest bindings: %s", __FUNCTION__, xr::ToCString(suggestResult));
                        TraceLoggingWriteTagged(local,
                                                "OpenXrLayer::SuggestInteractionProfiles",
                                                TLArg(xr::ToCString(suggestResult), "SuggestBindings"));
                    }
                    else
                    {
                        m_InteractionProfileSuggested = true;
                        m_SimpleProfileSuggested = true;
                        Log("suggested %s as fallback", profile.c_str());
                        TraceLoggingWriteTagged(local,
                                          "OpenXrLayer::SuggestInteractionProfiles",
                                          TLArg(caller.c_str(), "Caller"),
                                          TLArg(profile.c_str(), "Profile"));
                    }
                }
            }
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::SuggestInteractionProfiles");
    }

    bool OpenXrLayer::LazyInit(const XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::LazyInit");

        bool success = true;
        if (!CreateTrackerActions("LazyInit"))
        {
            success = false;
        }

        if (!AttachActionSet("LazyInit"))
        {
            success = false;
        }
        
        if (time != 0 && !m_Tracker->LazyInit(time))
        {
            success = false;
        }

        TraceLoggingWriteStop(local, "OpenXrLayer::LazyInit", TLArg(success, "Success"));

        return success;
    }

    void OpenXrLayer::RecoveryTimeOut(XrTime time)
    {
        if (0 == m_RecoveryStart)
        {
            ErrorLog("%s: unable to retrieve tracker pose delta", __FUNCTION__);
            m_RecoveryStart = time;
        }
        else if (m_RecoveryWait >= 0 && time - m_RecoveryStart > m_RecoveryWait)
        {
            ErrorLog("%s, tracker connection lost", __FUNCTION__);
            AudioOut::Execute(Event::ConnectionLost);
            m_Activated = false;
            m_RecoveryStart = -1;
        }
    }

    void OpenXrLayer::LogCurrentInteractionProfile()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::logCurrentInteractionProfile");

        XrInteractionProfileState profileState{XR_TYPE_INTERACTION_PROFILE_STATE, nullptr, XR_NULL_PATH};
        if (const XrResult interactionResult =
                xrGetCurrentInteractionProfile(m_Session, m_XrSubActionPath, &profileState);
            XR_SUCCEEDED(interactionResult))
        {
            Log("current interaction profile for %s: %s",
                m_SubActionPath.c_str(),
                XR_NULL_PATH != profileState.interactionProfile ? getXrPath(profileState.interactionProfile).c_str()
                                                                : "XR_NULL_PATH");
        }
        else
        {
            ErrorLog("%s: unable get current interaction profile for %s: %s",
                     __FUNCTION__,
                     m_SubActionPath.c_str(),
                     xr::ToCString(interactionResult));
        }
        TraceLoggingWriteStop(local, "OpenXrLayer::logCurrentInteractionProfile");
    }

    bool OpenXrLayer::ToggleModifierActive()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::ToggleModifierActive");

        m_ModifierActive = !m_ModifierActive;
        m_Tracker->SetModifierActive(m_ModifierActive);
        m_HmdModifier->SetActive(m_ModifierActive);

        Log("pose modifier globally %s", m_ModifierActive ? "activated" : "off");

        TraceLoggingWriteStop(local, "OpenXrLayer::ToggleModifierActive", TLArg(m_ModifierActive, "ModifierActive"));

        return m_ModifierActive;
    }

    void OpenXrLayer::ToggleRecorderActive()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrLayer::ToggleRecorderActive");

        m_RecorderActive = m_Tracker->ToggleRecording();

        TraceLoggingWriteStop(local, "OpenXrLayer::ToggleRecorderActive", TLArg(m_RecorderActive, "RecorderActive"));
    }

    std::string OpenXrLayer::getXrPath(const XrPath path)
    {
        char buf[XR_MAX_PATH_LENGTH];
        uint32_t count;
        std::string str;
        if (const XrResult result =
                GetInstance()->xrPathToString(GetInstance()->GetXrInstance(), path, sizeof(buf), &count, buf);
            XR_SUCCEEDED(result))
        {
            str.assign(buf, count - 1);
        }
        else
        {
            ErrorLog("%s: unable to convert XrPath %llu to string: %s", __FUNCTION__, path, xr::ToCString(result));
        }
        return str;
    }

    bool OpenXrLayer::TestRotation(XrPosef* pose, const XrTime time, const bool reverse) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrLayer::TestRotation",
                               TLArg(time, "Time"),
                               TLArg(reverse, "Reverse"),
                               TLArg(xr::ToString(*pose).c_str(), "OriginalPose"));

        // save current location
        const XrVector3f pos = pose->position;

        // determine rotation angle
        const int64_t milliseconds = ((time - m_TestRotStart) / 1000000) % 10000;
        float angle = utility::floatPi * 0.0002f * static_cast<float>(milliseconds);
        if (reverse)
        {
            angle = -angle;
        }

        // remove translation to rotate around center
        pose->position = {0, 0, 0};
        StoreXrPose(pose, XMMatrixMultiply(LoadXrPose(*pose), DirectX::XMMatrixRotationRollPitchYaw(0.f, angle, 0.f)));
        // reapply translation
        pose->position = pos;

        TraceLoggingWriteStop(local, "OpenXrLayer::TestRotation", TLArg(xr::ToString(*pose).c_str(), "Pose"));

        return true;
    }

    OpenXrApi* GetInstance()
    {
        if (!g_instance)
        {
            g_instance = std::make_unique<OpenXrLayer>();
        }
        return g_instance.get();
    }

} // namespace openxr_api_layer

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        TraceLoggingRegister(g_traceProvider);
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    default: ;
    }
    return TRUE;
}
