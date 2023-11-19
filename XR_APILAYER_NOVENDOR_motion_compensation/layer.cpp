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
#include "feedback.h"
#include "utility.h"
#include "config.h"
#include <log.h>
#include <util.h>


using namespace openxr_api_layer::log;
using namespace Feedback;
using namespace xr::math;
namespace openxr_api_layer
{
    OpenXrLayer::~OpenXrLayer()
    {
        delete m_Tracker;
    }

    XrResult OpenXrLayer::xrDestroyInstance(XrInstance instance)
    {
        if (m_Enabled)
        {
            Log("xrDestroyInstance");
        }
        if (m_Overlay)
        {
            m_Overlay.reset();
        }
        return OpenXrApi::xrDestroyInstance(instance);
    }

    XrResult OpenXrLayer::xrCreateInstance(const XrInstanceCreateInfo* createInfo)
    {
        Log("xrCreateInstance");
        if (createInfo->type != XR_TYPE_INSTANCE_CREATE_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrCreateInstance",
                          TLArg(xr::ToString(createInfo->applicationInfo.apiVersion).c_str(), "ApiVersion"),
                          TLArg(createInfo->applicationInfo.applicationName, "ApplicationName"),
                          TLArg(createInfo->applicationInfo.applicationVersion, "ApplicationVersion"),
                          TLArg(createInfo->applicationInfo.engineName, "EngineName"),
                          TLArg(createInfo->applicationInfo.engineVersion, "EngineVersion"),
                          TLArg(createInfo->createFlags, "CreateFlags"));

        for (uint32_t i = 0; i < createInfo->enabledApiLayerCount; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrCreateInstance",
                              TLArg(createInfo->enabledApiLayerNames[i], "ApiLayerName"));
        }
        for (uint32_t i = 0; i < createInfo->enabledExtensionCount; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrCreateInstance",
                              TLArg(createInfo->enabledExtensionNames[i], "ExtensionName"));
        }

        // Needed to resolve the requested function pointers.
        const XrResult result = OpenXrApi::xrCreateInstance(createInfo);

        m_Application = GetApplicationName();

        // Dump the application name and OpenXR runtime information to help debugging issues.
        XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES};
        CHECK_XRCMD(OpenXrApi::xrGetInstanceProperties(GetXrInstance(), &instanceProperties));
        m_RuntimeName = fmt::format("{} {}.{}.{}",
                                             instanceProperties.runtimeName,
                                             XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
                                             XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                                             XR_VERSION_PATCH(instanceProperties.runtimeVersion));
        TraceLoggingWrite(g_traceProvider, "xrCreateInstance", TLArg(m_RuntimeName.c_str(), "RuntimeName"));
        TraceLoggingWrite(g_traceProvider, "xrCreateInstance", TLArg(m_Application.c_str(), "ApplicationName"));
        Log("Application: %s", m_Application.c_str());
        Log("Using OpenXR runtime: %s", m_RuntimeName.c_str());

        m_VarjoPollWorkaround = m_RuntimeName.find("Varjo") != std::string::npos;

        // Initialize Configuration
        m_Initialized = GetConfig()->Init(m_Application);

        // set log level
        GetConfig()->GetBool(Cfg::LogVerbose, logVerbose);

        if (m_Initialized)
        {
            GetConfig()->GetBool(Cfg::Enabled, m_Enabled);
            if (!m_Enabled)
            {
                Log("motion compensation disabled in config file");
                return result;
            }

            if (!m_ViveTracker.Init())
            {
                m_Initialized = false;
            }

            // enable / disable physical tracker initialization
            GetConfig()->GetBool(Cfg::PhysicalEnabled, m_PhysicalEnabled);
            GetConfig()->GetBool(Cfg::CompensateControllers, m_CompensateControllers);
            if (!m_PhysicalEnabled)
            {
                Log("initialization of physical tracker disabled in config file");
            }
            else
            {
                if (m_CompensateControllers)
                {
                    Log("compensation of motion controllers is active");
                    m_SuppressInteraction = GetConfig()->IsVirtualTracker();
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


            // initialize audio feedback
            GetAudioOut()->Init();

            // enable debug test rotation
            GetConfig()->GetBool(Cfg::TestRotation, m_TestRotation);

            // choose cache for reverting pose in xrEndFrame
            GetConfig()->GetBool(Cfg::CacheUseEye, m_UseEyeCache);

            float timeout;
            if (GetConfig()->GetFloat(Cfg::TrackerTimeout, timeout))
            {
                m_RecoveryWait = static_cast<XrTime>(timeout * 1000000000.0);
            }
            Log("tracker timeout is set to %.3f ms", static_cast<double>(m_RecoveryWait) / 1000000.0);

            float cacheTolerance{2.0};
            GetConfig()->GetFloat(Cfg::CacheTolerance, cacheTolerance);
            Log("cache tolerance is set to %.3f ms", cacheTolerance);
            const auto toleranceTime = static_cast<XrTime>(cacheTolerance * 1000000.0);
            m_PoseCache.SetTolerance(toleranceTime);
            m_EyeCache.SetTolerance(toleranceTime);
        }

        // initialize tracker
        Tracker::GetTracker(&m_Tracker);
        if (!m_Tracker->Init())
        {
            m_Initialized = false;
        }

        // initialize keyboard input handler
        m_Input = std::make_shared<Input::InputHandler>(Input::InputHandler(this));
        if (!m_Input->Init())
        {
            m_Initialized = false;
        }

        // enable / disable graphical overlay initialization
        GetConfig()->GetBool(Cfg::OverlayEnabled, m_OverlayEnabled);
        if (m_OverlayEnabled)
        {
            m_Overlay = std::make_unique<graphics::Overlay>();
            m_Overlay->Init(*createInfo, GetXrInstance(), m_xrGetInstanceProcAddr);  
        }
        else
        {
            Log("graphical overlay disabled in config file");
        }

        // initialize auto activator
        m_AutoActivator = std::make_unique<utility::AutoActivator>(utility::AutoActivator(m_Input));

        // initialize hmd modifier
        m_HmdModifier = std::make_unique<utility::HmdModifier>();
        GetConfig()->GetBool(Cfg::FactorEnabled, m_ModifierActive);

        return result;
    }

    XrResult OpenXrLayer::xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrGetSystem(instance, getInfo, systemId);
        }

        DebugLog("xrGetSystem");
        if (getInfo->type != XR_TYPE_SYSTEM_GET_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetSystem",
                          TLPArg(instance, "Instance"),
                          TLArg(xr::ToCString(getInfo->formFactor), "FormFactor"));

        const XrResult result = OpenXrApi::xrGetSystem(instance, getInfo, systemId);
        if (XR_SUCCEEDED(result) && getInfo->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY)
        {
            if (*systemId != m_systemId)
            {
                XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
                CHECK_XRCMD(OpenXrApi::xrGetSystemProperties(instance, *systemId, &systemProperties));
                TraceLoggingWrite(g_traceProvider, "xrGetSystem", TLArg(systemProperties.systemName, "SystemName"));
                Log("Using OpenXR system: %s", systemProperties.systemName);
            }

            // Remember the XrSystemId to use.
            m_systemId = *systemId;
        }

        TraceLoggingWrite(g_traceProvider, "xrGetSystem", TLArg((int)*systemId, "SystemId"));

        return result;
    }

    XrResult OpenXrLayer::xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
    {
        TraceLoggingWrite(g_traceProvider, "xrPollEvent", TLPArg(instance, "Instance"));
        m_VarjoPollWorkaround = false;
        return OpenXrApi::xrPollEvent(instance, eventData);
    }

    XrResult OpenXrLayer::xrCreateSession(XrInstance instance,
                                          const XrSessionCreateInfo* createInfo,
                                          XrSession* session)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrCreateSession(instance, createInfo, session);
        }

        Log("xrCreateSession");
        if (createInfo->type != XR_TYPE_SESSION_CREATE_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrCreateSession",
                          TLPArg(instance, "Instance"),
                          TLArg((int)createInfo->systemId, "SystemId"),
                          TLArg(createInfo->createFlags, "CreateFlags"));

        const XrResult result =  OpenXrApi::xrCreateSession(instance, createInfo, session);
        if (XR_SUCCEEDED(result))
        {
            if (isSystemHandled(createInfo->systemId))
            {
                m_Session = *session;
                if (m_Overlay)
                {
                    m_Overlay->CreateSession(createInfo, m_Session);
                }

                if (bool earlyPhysicalInit; m_PhysicalEnabled &&
                                            GetConfig()->GetBool(Cfg::PhysicalEarly, earlyPhysicalInit) &&
                                            earlyPhysicalInit)
                {
                    // initialize everything except tracker
                    LazyInit(0);
                }

                constexpr XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                                          nullptr,
                                                                          XR_REFERENCE_SPACE_TYPE_VIEW,
                                                                          Pose::Identity()};
                if (const XrResult refResult =
                        xrCreateReferenceSpace(*session, &referenceSpaceCreateInfo, &m_ViewSpace);
                    XR_FAILED(refResult))
                {
                    ErrorLog("%s: unable to create reference view space: %s", __FUNCTION__, xr::ToCString(refResult));
                }
            }

            TraceLoggingWrite(g_traceProvider, "xrCreateSession", TLPArg(*session, "Session"));
        }

        return result;
    }

    XrResult OpenXrLayer::xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrBeginSession(session, beginInfo);
        }

        Log("xrBeginSession");
        if (beginInfo->type != XR_TYPE_SESSION_BEGIN_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }
        
        TraceLoggingWrite(
            g_traceProvider,
            "xrBeginSession",
            TLPArg(session, "Session"),
            TLArg(xr::ToCString(beginInfo->primaryViewConfigurationType), "PrimaryViewConfigurationType"));

        const XrResult result = OpenXrApi::xrBeginSession(session, beginInfo);
        m_ViewConfigType = beginInfo->primaryViewConfigurationType;

        return result;
    }

    XrResult OpenXrLayer::xrEndSession(XrSession session)
    {
        if (m_Enabled)
        {
            Log("xrEndSession");
            TraceLoggingWrite(g_traceProvider, "xrEndssion", TLPArg(session, "Session"));
        }
        return OpenXrApi::xrEndSession(session);
    }

    XrResult OpenXrLayer::xrDestroySession(XrSession session)
    {
        if (m_Enabled)
        {
            Log("xrDestroySession");
            TraceLoggingWrite(g_traceProvider, "xrDestroySession", TLPArg(session, "Session"));
            if (XR_NULL_HANDLE != m_TrackerSpace)
            {
                GetInstance()->xrDestroySpace(m_TrackerSpace);
                m_TrackerSpace = XR_NULL_HANDLE;
            }
            m_ActionSpaceCreated = false;
            m_ViewSpaces.clear();
            m_ActionSpaces.clear();
            if (m_Overlay)
            {
                m_Overlay->DestroySession(session);
            }
        }
        const XrResult result = OpenXrApi::xrDestroySession(session);
        return result;
    }

    XrResult OpenXrLayer::xrGetCurrentInteractionProfile(XrSession session,
                                                         XrPath topLevelUserPath,
                                                         XrInteractionProfileState* interactionProfile)
    {
        const XrResult result = OpenXrApi::xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
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

        if (suggestedBindings->type != XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        const std::string profile = getXrPath(suggestedBindings->interactionProfile);
        Log("xrSuggestInteractionProfileBindings: %s", profile.c_str());
        TraceLoggingWrite(g_traceProvider,
                          "xrSuggestInteractionProfileBindings",
                          TLPArg(instance, "Instance"),
                          TLArg(profile.c_str(), "InteractionProfile"));

        for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrSuggestInteractionProfileBindings",
                              TLPArg(suggestedBindings->suggestedBindings[i].action, "Action"),
                              TLArg(getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str(), "Path"));

            DebugLog("binding: %s", getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str());
        }

        if (m_ActionSetAttached)
        {
            // detach and recreate action set and tracker space
            DestroyTrackerActions("xrSuggestInteractionProfileBindings");
            Log("destroyed tracker action for recreation");
        }
        CreateTrackerActions("xrSuggestInteractionProfileBindings");

        XrInteractionProfileSuggestedBinding bindingProfiles = *suggestedBindings;
        std::vector<XrActionSuggestedBinding> bindings{};

        bindings.resize((uint32_t)bindingProfiles.countSuggestedBindings);
        memcpy(bindings.data(),
               bindingProfiles.suggestedBindings,
               bindingProfiles.countSuggestedBindings * sizeof(XrActionSuggestedBinding));

        const bool isVirtual = GetConfig()->IsVirtualTracker();
        const std::string trackerPath(m_SubActionPath + "/");
        const std::string posePath(trackerPath + "input/grip/pose");
        const std::string movePath(isVirtual ? trackerPath + m_ButtonPath.GetSubPath(profile, 0) : "");
        const std::string positionPath(isVirtual ? trackerPath + m_ButtonPath.GetSubPath(profile, 1) : "");
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

                if (getXrPath(curBinding.binding) == posePath)
                {
                    curBinding.action = m_PoseAction;
                    poseBindingOverriden = true;
                    m_InteractionProfileSuggested = true;
                    Log("Binding %s - %s overridden with reference tracker action", profile.c_str(), posePath.c_str());
                }
                if (isVirtual && getXrPath(curBinding.binding) == movePath)
                {
                    curBinding.action = m_MoveAction;
                    moveBindingOverriden = true;
                    Log("Binding %s - %s overridden with move action", profile.c_str(), movePath.c_str());
                }
                if (isVirtual && getXrPath(curBinding.binding) == positionPath)
                {
                    curBinding.action = m_PositionAction;
                    positionBindingOverriden = true;
                    Log("Binding %s - %s overridden with position action", profile.c_str(), positionPath.c_str());
                }
                if (isVirtual && getXrPath(curBinding.binding) == hapticPath)
                {
                    curBinding.action = m_HapticAction;
                    hapticBindingOverriden = true;
                    Log("Binding %s - %s overridden with haptic action",
                        profile.c_str(),
                        hapticPath.c_str());
                }
            }
        }
        if (isTrackerPath && !poseBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include pose -> add it
            XrActionSuggestedBinding newBinding{m_PoseAction};
            if (const XrResult result = xrStringToPath(GetXrInstance(), posePath.c_str(), &newBinding.binding);
                XR_SUCCEEDED(result))
            {
                bindings.push_back(newBinding);
                m_InteractionProfileSuggested = true;
                Log("Binding %s - %s for pose action added", profile.c_str(), posePath.c_str());
            }
            else
            {
                ErrorLog("%s: unable to create XrPath from %s: %s", __FUNCTION__, posePath.c_str(), xr::ToCString(result));
            }
        }
        if (isVirtual && isTrackerPath && !moveBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include move -> add it
            XrActionSuggestedBinding newBinding{m_MoveAction};
            if (const XrResult result = xrStringToPath(GetXrInstance(), movePath.c_str(), &newBinding.binding);
                XR_SUCCEEDED(result))
            {
                bindings.push_back(newBinding);
                Log("Binding %s - %s for move action added", profile.c_str(), movePath.c_str());
            }
            else
            {
                ErrorLog("%s: unable to create XrPath from %s: %s", __FUNCTION__, movePath.c_str(), xr::ToCString(result));
            }
        }
        if (isVirtual && isTrackerPath && !positionBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include position -> add it
            XrActionSuggestedBinding newBinding{m_PositionAction};
            if (const XrResult result = xrStringToPath(GetXrInstance(), positionPath.c_str(), &newBinding.binding);
                XR_SUCCEEDED(result))
            {
                bindings.push_back(newBinding);
                Log("Binding %s - %s for position action added", profile.c_str(), positionPath.c_str());
            }
            else
            {
                ErrorLog("%s: unable to create XrPath from %s: %s", __FUNCTION__, positionPath.c_str(), xr::ToCString(result));
            }
        }
        if (isVirtual && isTrackerPath  && !hapticBindingOverriden)
        {
            // suggestion is for tracker input but doesn't include haptic -> add it
            XrActionSuggestedBinding newBinding{m_HapticAction};
            if (const XrResult result = xrStringToPath(GetXrInstance(), hapticPath.c_str(), &newBinding.binding);
                XR_SUCCEEDED(result))
            {
                bindings.push_back(newBinding);
                Log("Binding %s - %s for haptic action added", profile.c_str(), hapticPath.c_str());
            }
            else
            {
                ErrorLog("%s: unable to create XrPath from %s: %s", __FUNCTION__, hapticPath.c_str(), xr::ToCString(result));
            }
        }

        bindingProfiles.suggestedBindings = bindings.data();
        bindingProfiles.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
        return OpenXrApi::xrSuggestInteractionProfileBindings(instance, &bindingProfiles);
    }

    XrResult OpenXrLayer::xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
    {
        if (!m_Enabled || !m_PhysicalEnabled || m_SuppressInteraction)
        {
            return OpenXrApi::xrAttachSessionActionSets(session, attachInfo);
        }

        Log("xrAttachSessionActionSets");
        if (attachInfo->type != XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets", TLPArg(session, "Session"));
        for (uint32_t i = 0; i < attachInfo->countActionSets; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrAttachSessionActionSets",
                              TLPArg(attachInfo->actionSets[i], "ActionSet"));
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
        Log("action set(s)%s attached, result = %s, #sets = %d",
            XR_SUCCEEDED(result) ? "" : " not",
            xr::ToCString(result),
            chainAttachInfo.countActionSets);
        if (XR_ERROR_ACTIONSETS_ALREADY_ATTACHED == result)
        {
            Log("If you're using an application that does not support motion controllers, try disabling physical "
                "tracker (if you don't use it for mc) or enabling early initialization");
        }
        if (XR_SUCCEEDED(result))
        {
            m_ActionSetAttached = true;
        }
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

        if (createInfo->type != XR_TYPE_REFERENCE_SPACE_CREATE_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }
        TraceLoggingWrite(g_traceProvider,
                          "xrCreateReferenceSpace",
                          TLPArg(session, "Session"),
                          TLArg(xr::ToCString(createInfo->referenceSpaceType), "ReferenceSpaceType"),
                          TLArg(xr::ToString(createInfo->poseInReferenceSpace).c_str(), "PoseInReferenceSpace"));

        const XrResult result = OpenXrApi::xrCreateReferenceSpace(session, createInfo, space);
        DebugLog("xrCreateReferenceSpace: %u type: %d ", *space, createInfo->referenceSpaceType);
        if (XR_SUCCEEDED(result))
        {
            if (XR_REFERENCE_SPACE_TYPE_VIEW == createInfo->referenceSpaceType)
            {
                Log("creation of view space detected: %u, offset pose: %s",
                    *space,
                    xr::ToString(createInfo->poseInReferenceSpace).c_str());

                // memorize view spaces
                TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace", TLArg("View_Space", "Added"));
                m_ViewSpaces.insert(*space);
            }
            else if (XR_REFERENCE_SPACE_TYPE_LOCAL == createInfo->referenceSpaceType)
            {
                Log("creation of local reference space detected: %u, offset pose: %s",
                    *space,
                    xr::ToString(createInfo->poseInReferenceSpace).c_str());
            }
            else if (XR_REFERENCE_SPACE_TYPE_STAGE == createInfo->referenceSpaceType)
            {
                Log("creation of stage reference space detected: %u, offset pose: %s",
                    *space,
                    xr::ToString(createInfo->poseInReferenceSpace).c_str());
            }
        }
        TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace", TLPArg(*space, "Space"));

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
        const std::string subActionPath{!createInfo->subactionPath ? "null" : getXrPath(createInfo->subactionPath)};
        TraceLoggingWrite(g_traceProvider,
                          "xrCreateActionSpace",
                          TLPArg(session, "Session"),
                          TLPArg(*space, "Space"),
                          TLArg(subActionPath.c_str(), "SubactionPath"));
        Log("creation of action space detected: %u, sub action path: %s", *space, subActionPath.c_str());
        if (m_CompensateControllers)
        {
            Log("added action space for motion controller compensation: %u", *space);
            m_ActionSpaces.insert(*space);
        }
        return result;
    }

    XrResult OpenXrLayer::xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
    {
        if (!m_Enabled)
        {
            return OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
        }

        if (location->type != XR_TYPE_SPACE_LOCATION)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        DebugLog("xrLocateSpace(%u): %u %u", time, space, baseSpace);
        TraceLoggingWrite(g_traceProvider,
                          "xrLocateSpace",
                          TLPArg(space, "Space"),
                          TLPArg(baseSpace, "BaseSpace"),
                          TLArg(time, "Time"));

        // determine original location
        const XrResult result = OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
        if (XR_FAILED(result))
        {
            ErrorLog("%s: xrLocateSpace failed: %s", __FUNCTION__, xr::ToCString(result));
            return result; 
        }
        const bool compensateSpace = isViewSpace(space) || (m_CompensateControllers && isActionSpace(space));
        const bool compensateBaseSpace =
            isViewSpace(baseSpace) || (m_CompensateControllers && isActionSpace(baseSpace));
        if (m_Activated && ((compensateSpace && !compensateBaseSpace) || (!compensateSpace && compensateBaseSpace)))
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrLocateSpace",
                              TLArg(xr::ToString(location->pose).c_str(), "PoseBefore"),
                              TLArg(location->locationFlags, "LocationFlags"));

            std::unique_lock lock(m_FrameLock);

            // determine stage to local transformation
            if (SetStageToLocalSpace(baseSpace, time))
            {
                // manipulate pose using tracker
                XrPosef trackerDelta{Pose::Identity()};
                bool apply = true;
                if (!m_TestRotation)
                {
                    apply = m_Tracker->GetPoseDelta(trackerDelta, m_Session, time);
                    m_HmdModifier->Apply(trackerDelta, location->pose);
                    trackerDelta =
                        Pose::Multiply(Pose::Multiply(Pose::Invert(m_StageToLocal), trackerDelta), m_StageToLocal);
                }
                else
                {
                    TestRotation(&trackerDelta, time, false);
                }
                if (apply)
                {
                    m_RecoveryStart = 0;
                    if (compensateSpace)
                    {
                        location->pose = Pose::Multiply(location->pose, trackerDelta);
                    }
                    if (compensateBaseSpace)
                    {
                        // TODO: verify calculation
                        Log("Please report the application in use to oxrmc developer!");
                        location->pose = Pose::Multiply(location->pose, Pose::Invert(trackerDelta));
                    }
                }
                else
                {
                    if (0 == m_RecoveryStart)
                    {
                        ErrorLog("unable to retrieve tracker pose delta");
                        m_RecoveryStart = time;
                    }
                    else if (m_RecoveryWait >= 0 && time - m_RecoveryStart > m_RecoveryWait)
                    {
                        ErrorLog("tracker connection lost");
                        GetAudioOut()->Execute(Event::ConnectionLost);
                        m_Activated = false;
                        m_RecoveryStart = -1;
                    }
                }

                // safe pose for use in xrEndFrame
                m_PoseCache.AddSample(time, trackerDelta);
            }

            TraceLoggingWrite(g_traceProvider,
                              "xrLocateSpace",
                              TLArg(xr::ToString(location->pose).c_str(), "PoseAfter"));
        }

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

        if (viewLocateInfo->type != XR_TYPE_VIEW_LOCATE_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        DebugLog("xrLocateViews(%u): %u", viewLocateInfo->displayTime, viewLocateInfo->space);
        TraceLoggingWrite(g_traceProvider,
                          "xrLocateViews",
                          TLPArg(session, "Session"),
                          TLArg(xr::ToCString(viewLocateInfo->viewConfigurationType), "ViewConfigurationType"),
                          TLArg(viewLocateInfo->displayTime, "DisplayTime"),
                          TLPArg(viewLocateInfo->space, "Space"),
                          TLArg(viewCapacityInput, "ViewCapacityInput"));

        const XrResult result =
            OpenXrApi::xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);

        TraceLoggingWrite(g_traceProvider, "xrLocateViews", TLArg(viewState->viewStateFlags, "ViewStateFlags"));

        if (!m_Activated)
        {
            return result;
        }

        // store eye poses to avoid recalculation in xrEndFrame
        std::vector<XrPosef> originalEyePoses{};
        for (uint32_t i = 0; i < *viewCountOutput; i++)
        {
            originalEyePoses.push_back(views[i].pose);
        }
        m_EyeCache.AddSample(viewLocateInfo->displayTime, originalEyePoses);

        if (m_EyeOffsets.empty())
        {
            // determine eye poses
            const XrViewLocateInfo offsetViewLocateInfo{viewLocateInfo->type,
                                                        nullptr,
                                                        viewLocateInfo->viewConfigurationType,
                                                        viewLocateInfo->displayTime,
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
                }
            }
        }

        // manipulate reference space location
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        if (XR_SUCCEEDED(xrLocateSpace(m_ViewSpace, viewLocateInfo->space, viewLocateInfo->displayTime, &location)))
        {
            for (uint32_t i = 0; i < *viewCountOutput; i++)
            {
                TraceLoggingWrite(g_traceProvider, "xrLocateViews", TLArg(xr::ToString(views[i].fov).c_str(), "Fov"));
                TraceLoggingWrite(g_traceProvider,
                                  "xrLocateViews",
                                  TLArg(i, "Index"),
                                  TLArg(xr::ToString(views[i].pose).c_str(), "PoseBefore"));

                // apply manipulation
                views[i].pose = Pose::Multiply(m_EyeOffsets[i].pose, location.pose);

                TraceLoggingWrite(g_traceProvider,
                                  "xrLocateViews",
                                  TLArg(i, "Index"),
                                  TLArg(xr::ToString(views[i].pose).c_str(), "PoseAfter"));
            }
        }

        return result;
    }

    XrResult OpenXrLayer::xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
    {
        if (!m_Enabled || !m_PhysicalEnabled || m_SuppressInteraction)
        {
            return OpenXrApi::xrSyncActions(session, syncInfo);
        }

        DebugLog("xrSyncActions");
        if (syncInfo->type != XR_TYPE_ACTIONS_SYNC_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }
        
        TraceLoggingWrite(g_traceProvider, "xrSyncActions", TLPArg(session, "Session"));
        for (uint32_t i = 0; i < syncInfo->countActiveActionSets; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrSyncActions",
                              TLPArg(syncInfo->activeActionSets[i].actionSet, "ActionSet"),
                              TLArg(syncInfo->activeActionSets[i].subactionPath, "SubactionPath"));
        }

        AttachActionSet("xrSyncActions");

        XrActionsSyncInfo chainSyncInfo = *syncInfo;
        std::vector<XrActiveActionSet> newActiveActionSets;
        const auto trackerActionSet = m_ActionSet;
        if (trackerActionSet != XR_NULL_HANDLE)
        {
            newActiveActionSets.resize(static_cast<size_t>(chainSyncInfo.countActiveActionSets) + 1);
            memcpy(newActiveActionSets.data(),
                   chainSyncInfo.activeActionSets,
                   chainSyncInfo.countActiveActionSets * sizeof(XrActiveActionSet));
            uint32_t nextActionSetSlot = chainSyncInfo.countActiveActionSets;

            newActiveActionSets[nextActionSetSlot].actionSet = trackerActionSet;
            newActiveActionSets[nextActionSetSlot++].subactionPath = XR_NULL_PATH;

            chainSyncInfo.activeActionSets = newActiveActionSets.data();
            chainSyncInfo.countActiveActionSets = nextActionSetSlot;

            TraceLoggingWrite(g_traceProvider,
                              "xrSyncActions",
                              TLPArg(trackerActionSet, "ActionSet Attached"),
                              TLArg(chainSyncInfo.countActiveActionSets, "ActionSet Count"));
        }
        const XrResult result = OpenXrApi::xrSyncActions(session, &chainSyncInfo);
        DebugLog("xrSyncAction result = %s, #sets = %d", xr::ToCString(result), chainSyncInfo.countActiveActionSets);
        m_Tracker->m_XrSyncCalled = true;
        return result;
    }

    XrResult OpenXrLayer::xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
    {
        if (m_VarjoPollWorkaround && m_Enabled && m_PhysicalEnabled && !m_SuppressInteraction)
        {
            // call xrPollEvent (if the app hasn't already) to acquire focus
            XrEventDataBuffer buf{XR_TYPE_EVENT_DATA_BUFFER};
            OpenXrApi::xrPollEvent(GetXrInstance(), &buf);
        }
        if (!m_Enabled || !m_OverlayEnabled)
        {
            return OpenXrApi::xrBeginFrame(session, frameBeginInfo);
        }

        if (frameBeginInfo && frameBeginInfo->type != XR_TYPE_FRAME_BEGIN_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }
        DebugLog("xrBeginFrame");

        std::unique_lock lock(m_FrameLock);

        TraceLoggingWrite(g_traceProvider, "xrBeginFrame", TLPArg(session, "Session"));

        return OpenXrApi::xrBeginFrame(session, frameBeginInfo);
    }

    XrResult OpenXrLayer::xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
    {
        if (!m_Enabled || !isSessionHandled(session))
        {
            return OpenXrApi::xrEndFrame(session, frameEndInfo);
        }

        if (frameEndInfo->type != XR_TYPE_FRAME_END_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        DebugLog("xrEndFrame(%u)", frameEndInfo->displayTime);
        TraceLoggingWrite(g_traceProvider,
                          "xrEndFrame",
                          TLPArg(session, "Session"),
                          TLArg(frameEndInfo->displayTime, "DisplayTime"),
                          TLArg(xr::ToCString(frameEndInfo->environmentBlendMode), "EnvironmentBlendMode"));

        std::unique_lock lock(m_FrameLock);

        if (m_AutoActivator)
        {
            m_AutoActivator->ActivateIfNecessary(frameEndInfo->displayTime);
        }

        XrFrameEndInfo chainFrameEndInfo = *frameEndInfo;

        XrPosef reversedManipulation{Pose::Identity()};
        std::vector<XrPosef> cachedEyePoses;
        if (m_Activated)
        {
            reversedManipulation = Pose::Invert(m_PoseCache.GetSample(chainFrameEndInfo.displayTime));
            m_PoseCache.CleanUp(chainFrameEndInfo.displayTime);
            cachedEyePoses =
                m_UseEyeCache ? m_EyeCache.GetSample(chainFrameEndInfo.displayTime) : std::vector<XrPosef>();
            m_EyeCache.CleanUp(chainFrameEndInfo.displayTime);
        }
        else if (m_Tracker->m_Calibrated && !m_SuppressInteraction)
        {
            m_Tracker->ApplyCorManipulation(session, chainFrameEndInfo.displayTime);
        }

        if (m_OverlayEnabled)
        {
            m_Overlay->DrawOverlay(session,
                                   &chainFrameEndInfo,
                                   Pose::Multiply(m_Tracker->GetReferencePose(), m_StageToLocal),
                                   reversedManipulation,
                                   m_Activated);
        }

        m_Tracker->m_XrSyncCalled = false;

        if (!m_Activated)
        {
            m_Input->HandleKeyboardInput(chainFrameEndInfo.displayTime);
            XrResult result =  OpenXrApi::xrEndFrame(session, &chainFrameEndInfo);
            if (m_OverlayEnabled)
            {
                m_Overlay->DeleteResources();
            }
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
                DebugLog("xrEndFrame: projection layer %u, space: %u", i, baseHeader.space);

                const auto* projectionLayer =
                    reinterpret_cast<const XrCompositionLayerProjection*>(chainFrameEndInfo.layers[i]);

                TraceLoggingWrite(g_traceProvider,
                                  "xrEndFrame_Layer",
                                  TLArg("ProjectionLayer", "Type"),
                                  TLArg(projectionLayer->layerFlags, "Flags"),
                                  TLPArg(projectionLayer->space, "Space"));

                auto projectionViews = new std::vector<XrCompositionLayerProjectionView>{};
                resetViews.push_back(projectionViews);
                projectionViews->resize(projectionLayer->viewCount);
                memcpy(projectionViews->data(),
                       projectionLayer->views,
                       projectionLayer->viewCount * sizeof(XrCompositionLayerProjectionView));

                TraceLoggingWrite(g_traceProvider,
                                  "xrEndFrame_View",
                                  TLArg("Reversed_Manipulation", "Type"),
                                  TLArg(xr::ToString(reversedManipulation).c_str(), "Pose"));

                for (uint32_t j = 0; j < projectionLayer->viewCount; j++)
                {
                    TraceLoggingWrite(
                        g_traceProvider,
                        "xrEndFrame_View",
                        TLArg("View_Before", "Type"),
                        TLArg(xr::ToString((*projectionViews)[j].pose).c_str(), "Pose"),
                        TLArg(j, "Index"),
                        TLPArg((*projectionViews)[j].subImage.swapchain, "Swapchain"),
                        TLArg((*projectionViews)[j].subImage.imageArrayIndex, "ImageArrayIndex"),
                        TLArg(xr::ToString((*projectionViews)[j].subImage.imageRect).c_str(), "ImageRect"),
                        TLArg(xr::ToString((*projectionViews)[j].fov).c_str(), "Fov"));

                    XrPosef reversedEyePose = m_UseEyeCache
                                                  ? cachedEyePoses[j]
                                                  : Pose::Multiply((*projectionViews)[j].pose, reversedManipulation);

                    (*projectionViews)[j].pose = reversedEyePose;

                    TraceLoggingWrite(g_traceProvider,
                                      "xrEndFrame_View",
                                      TLArg("View_After", "Type"),
                                      TLArg(xr::ToString((*projectionViews)[j].pose).c_str(), "Pose"),
                                      TLArg(j, "Index"));
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
                DebugLog("xrEndFrame: quad layer %u, space: %u", i, baseHeader.space);

                const auto* quadLayer = reinterpret_cast<const XrCompositionLayerQuad*>(chainFrameEndInfo.layers[i]);

                TraceLoggingWrite(g_traceProvider,
                                  "xrEndFrame_Layer",
                                  TLArg("QuadLayer", "Type"),
                                  TLArg(quadLayer->layerFlags, "Flags"),
                                  TLPArg(quadLayer->space, "Space"),
                                  TLArg(xr::ToString(quadLayer->pose).c_str(), "Pose"));

                // apply reverse manipulation to quad layer pose
                XrPosef resetPose = Pose::Multiply(quadLayer->pose, reversedManipulation);

                TraceLoggingWrite(g_traceProvider,
                                  "xrEndFrame_Layer",
                                  TLArg("QuadLayer_After", "Type"),
                                  TLArg(xr::ToString(resetPose).c_str(), "Pose"));

                // create quad layer with reset pose
                auto* const resetQuadLayer = new XrCompositionLayerQuad{quadLayer->type,
                                                                        quadLayer->next,
                                                                        quadLayer->layerFlags,
                                                                        quadLayer->space,
                                                                        quadLayer->eyeVisibility,
                                                                        quadLayer->subImage,
                                                                        resetPose,
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
        m_Input->HandleKeyboardInput(chainFrameEndInfo.displayTime);

        XrFrameEndInfo resetFrameEndInfo{chainFrameEndInfo.type,
                                         chainFrameEndInfo.next,
                                         chainFrameEndInfo.displayTime,
                                         chainFrameEndInfo.environmentBlendMode,
                                         chainFrameEndInfo.layerCount,
                                         resetLayers.data()};

        XrResult result = OpenXrApi::xrEndFrame(session, &resetFrameEndInfo);

        // clean up memory
        if (m_Overlay)
        {
            m_Overlay->DeleteResources();
        }
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

        return result;
    }

    void OpenXrLayer::RequestCurrentInteractionProfile()
    {
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
    }

    void OpenXrLayer::SetForwardRotation(const XrPosef& pose) const
    {
        m_HmdModifier->SetFwdToStage(pose);
    }

    bool OpenXrLayer::ToggleModifierActive()
    {
        m_ModifierActive = !m_ModifierActive;
        m_Tracker->SetModifierActive(m_ModifierActive);
        m_HmdModifier->SetActive(m_ModifierActive);
        return m_ModifierActive;
    }

    // private
    bool OpenXrLayer::CreateStageSpace(const std::string& caller)
    {
        TraceLoggingWrite(g_traceProvider, "CreateStageSpace", TLPArg(caller.c_str(), "Called by"));
        if (m_StageSpace == XR_NULL_HANDLE)
        {
            // Create internal stage reference space.
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
            referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
            referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();

            if (const XrResult result =
                    OpenXrApi::xrCreateReferenceSpace(m_Session, &referenceSpaceCreateInfo, &m_StageSpace);
                XR_FAILED(result))
            {
                TraceLoggingWrite(g_traceProvider,
                                  "CreateStageSpace",
                                  TLPArg("failure", "xrCreateReferenceSpaceStage"));
                ErrorLog("%s (%s): xrCreateReferenceSpace(stage) failed: %s",
                         __FUNCTION__,
                         caller.c_str(),
                         xr::ToCString(result));
                return false;
            }
            TraceLoggingWrite(g_traceProvider, "CreateStageSpace", TLPArg("success", "xrCreateReferenceSpaceStage"));
            Log("internal stage space created (%s): %u", caller.c_str(), m_StageSpace);
        }
        return true;
    }

    bool OpenXrLayer::SetStageToLocalSpace(const XrSpace space, const XrTime time)
    {
        TraceLoggingWrite(g_traceProvider, "SetStageToLocalSpace", TLPArg(space, "Space"), TLArg(time, "Time"));
        if (m_StageSpace == XR_NULL_HANDLE && !CreateStageSpace("SetStageToLocalSpace"))
        {
            return false;
        }
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        const XrResult result = OpenXrApi::xrLocateSpace(m_StageSpace, space, time, &location);
        if (XR_FAILED(result))
        {
            ErrorLog("%s: unable to locate local reference space (%u) in stage reference space (%u): %s",
                     __FUNCTION__,
                     space,
                     m_StageSpace,
                     xr::ToCString(result));
            return false;
        }
        if (!Pose::IsPoseValid(location.locationFlags))
        {
            ErrorLog("%s: pose of local space in stage space not valid. locationFlags: %d", __FUNCTION__, location.locationFlags);
            return false;
        }
        if (!DirectX::XMVector3Equal(LoadXrVector3(location.pose.position), LoadXrVector3(m_StageToLocal.position)))
        {
            Log("local space to stage space: %s", xr::ToString(location.pose).c_str());
            m_StageToLocal = location.pose;
            m_HmdModifier->SetStageToLocal(m_StageToLocal);
            m_Tracker->SetStageToLocal(m_StageToLocal);
        }

        TraceLoggingWrite(g_traceProvider,
                          "SetStageToLocalSpace",
                          TLArg(xr::ToString(m_StageToLocal).c_str(), "StageToLocalPose"));
        return true;
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
        bool success = true;
        if (m_PhysicalEnabled && !m_SuppressInteraction)
        {
            if (!m_ActionsCreated)
            {
                DebugLog("CreateTrackerActionSet %s", caller.c_str());
                XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO, nullptr};
                strcpy_s(actionSetCreateInfo.actionSetName, "general_tracker_set");
                strcpy_s(actionSetCreateInfo.localizedActionSetName, "General Tracker Set");
                actionSetCreateInfo.priority = 0;
                if (const XrResult result = xrCreateActionSet(GetXrInstance(), &actionSetCreateInfo, &m_ActionSet);
                    XR_FAILED(result))
                {
                    ErrorLog("%s: unable to create action set: %s", __FUNCTION__, xr::ToCString(result));
                    success = false;
                }
                TraceLoggingWrite(g_traceProvider, "CreateTrackerActions", TLPArg(m_ActionSet, "ActionSet"));
                XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO, nullptr};
                strcpy_s(actionCreateInfo.actionName, "tracker_pose");
                strcpy_s(actionCreateInfo.localizedActionName, "Tracker Pose");
                actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                actionCreateInfo.countSubactionPaths = 1;
                actionCreateInfo.subactionPaths = &m_XrSubActionPath;

                if (const XrResult result = xrCreateAction(m_ActionSet, &actionCreateInfo, &m_PoseAction);
                    XR_FAILED(result))
                {
                    ErrorLog("%s: unable to create pose action: %s", __FUNCTION__, xr::ToCString(result));
                    success = false;
                }
                TraceLoggingWrite(g_traceProvider, "CreateTrackerActions", TLPArg(m_PoseAction, "PoseAction"));
                if (GetConfig()->IsVirtualTracker())
                {
                    strcpy_s(actionCreateInfo.actionName, "cor_move");
                    strcpy_s(actionCreateInfo.localizedActionName, "COR Move");
                    actionCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                    if (const XrResult result = xrCreateAction(m_ActionSet, &actionCreateInfo, &m_MoveAction);
                        XR_FAILED(result))
                    {
                        ErrorLog("%s: unable to create move action: %s", __FUNCTION__, xr::ToCString(result));
                    }
                    strcpy_s(actionCreateInfo.actionName, "cor_position");
                    strcpy_s(actionCreateInfo.localizedActionName, "COR Position");
                    if (const XrResult result = xrCreateAction(m_ActionSet, &actionCreateInfo, &m_PositionAction);
                        XR_FAILED(result))
                    {
                        ErrorLog("%s: unable to create position action: %s", __FUNCTION__, xr::ToCString(result));
                    }
                    strcpy_s(actionCreateInfo.actionName, "haptic_feedback");
                    strcpy_s(actionCreateInfo.localizedActionName, "Haptic Feedback");
                    actionCreateInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
                    if (const XrResult result = xrCreateAction(m_ActionSet, &actionCreateInfo, &m_HapticAction);
                        XR_FAILED(result))
                    {
                        ErrorLog("%s: unable to create haptic action: %s", __FUNCTION__, xr::ToCString(result));
                    }
                    TraceLoggingWrite(g_traceProvider,
                                      "CreateTrackerActions",
                                      TLPArg(m_MoveAction, "MoveAction"),
                                      TLPArg(m_PositionAction, "ButtonAction"),
                                      TLPArg(m_HapticAction, "HapticAction"));
                }
                m_ActionsCreated = success;
            }
            if (m_ActionsCreated && !m_ActionSpaceCreated && XR_NULL_HANDLE != m_Session)
            {
                DebugLog("CreateTrackerActionSpace %s", caller.c_str());
                XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr};
                actionSpaceCreateInfo.action = m_PoseAction;
                actionSpaceCreateInfo.subactionPath = m_XrSubActionPath;
                actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
                if (const XrResult result = GetInstance()->OpenXrApi::xrCreateActionSpace(m_Session,
                                                                                          &actionSpaceCreateInfo,
                                                                                          &m_TrackerSpace);
                    XR_FAILED(result))
                {
                    ErrorLog("%s: unable to create action space: %s", __FUNCTION__, xr::ToCString(result));
                    success = false;
                }
                else
                {
                    Log("action space for tracker pose created: %u", m_TrackerSpace);
                }

                TraceLoggingWrite(g_traceProvider, "CreateTrackerActions", TLPArg(m_TrackerSpace, "ActionSpace"));
                m_ActionSpaceCreated = success;
            }
        }
        return success;
    }

    void OpenXrLayer::DestroyTrackerActions(const std::string& caller)
    {
        DebugLog("DestroyTrackerActions %s", caller.c_str());
        TraceLoggingWrite(g_traceProvider, "DestroyTrackerActions", TLPArg(caller.c_str(), "Called by"));
        m_ActionsCreated = false;
        m_ActionSpaceCreated = false;
        m_ActionSetAttached = false;
        m_InteractionProfileSuggested = false;
        if (XR_NULL_HANDLE != m_ActionSet)
        {
            TraceLoggingWrite(g_traceProvider, "DestroyTrackerActions", TLPArg(m_ActionSet, "Action Set"));
            if (const XrResult result = GetInstance()->xrDestroyActionSet(m_ActionSet); XR_FAILED(result))
            {
                DebugLog("%s: unable to destroy action set (%d): %s", m_ActionSet, xr::ToCString(result));
            }
            m_ActionSet = XR_NULL_HANDLE;
        }
        if (XR_NULL_HANDLE != m_TrackerSpace)
        {
            TraceLoggingWrite(g_traceProvider, "DestroyTrackerActions", TLPArg(m_TrackerSpace, "Action Space"));
            if (const XrResult result = GetInstance()->xrDestroySpace(m_TrackerSpace); XR_FAILED(result))
            {
                DebugLog("%s: unable to destroy action space (%d): %s", m_TrackerSpace, xr::ToCString(result));
            }
            m_TrackerSpace = XR_NULL_HANDLE;
        }
    }

    bool OpenXrLayer::AttachActionSet(const std::string& caller)
    {
        bool success{true};
        if (m_PhysicalEnabled && !m_SuppressInteraction && !m_ActionSetAttached)
        {
            TraceLoggingWrite(g_traceProvider, "AttachActionSet", TLPArg(caller.c_str(), "Called by"));
            constexpr XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            if (XR_SUCCEEDED(xrAttachSessionActionSets(m_Session, &actionSetAttachInfo)))
            {
                Log("action set attached during %s", caller.c_str());
            }
            else
            {
                ErrorLog("%s: xrAttachSessionActionSets during %s failed", __FUNCTION__, caller.c_str());
                success = false;
            }
        }
        return success;
    }

    void OpenXrLayer::SuggestInteractionProfiles(const std::string& caller)
    {
        DebugLog("SuggestInteractionProfiles %s", caller.c_str());
        TraceLoggingWrite(g_traceProvider, "SuggestInteractionProfiles", TLPArg(caller.c_str(), "Called by"));

        CreateTrackerActions("SuggestInteractionProfiles");

        if (!m_InteractionProfileSuggested && !m_SuppressInteraction)
        {
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
                std::string movePath;
                std::string positionPath;
                std::string hapticPath;
                const std::string trackerPath(m_SubActionPath + "/");
                const std::string posePath{trackerPath + "input/grip/pose"};
                if (const XrResult poseResult = xrStringToPath(GetXrInstance(), posePath.c_str(), &poseBinding.binding);
                    XR_FAILED(poseResult))
                {
                    ErrorLog("%s: unable to create XrPath from %s: %s", __FUNCTION__, posePath.c_str(), xr::ToCString(poseResult));
                }
                else
                {
                    bindings.push_back(poseBinding);

                    // add move, position and haptic bindings for controller
                    if (GetConfig()->IsVirtualTracker())
                    {
                        movePath = trackerPath + m_ButtonPath.GetSubPath(profile, 0);
                        if (const XrResult result =
                                xrStringToPath(GetXrInstance(), movePath.c_str(), &moveBinding.binding);
                            XR_FAILED(result))
                        {
                            ErrorLog("%s: unable to create XrPath from %s: %s",
                                     __FUNCTION__,
                                     movePath.c_str(),
                                     xr::ToCString(result));
                        }
                        else
                        {
                            bindings.push_back(moveBinding);
                        }
                        positionPath =
                            trackerPath + m_ButtonPath.GetSubPath(profile, 1);
                        if (const XrResult result =
                                xrStringToPath(GetXrInstance(), positionPath.c_str(), &positionBinding.binding);
                            XR_FAILED(result))
                        {
                            ErrorLog("%s: unable to create XrPath from %s: %s",
                                     __FUNCTION__,
                                     positionPath.c_str(),
                                     xr::ToCString(result));
                        }
                        else
                        {
                            bindings.push_back(positionBinding);
                        }
                        hapticPath = trackerPath + "output/haptic";
                        if (const XrResult result =
                                xrStringToPath(GetXrInstance(), hapticPath.c_str(), &hapticBinding.binding);
                            XR_FAILED(result))
                        {
                            ErrorLog("%s: unable to create XrPath from %s: %s",
                                     __FUNCTION__,
                                     hapticPath.c_str(),
                                     xr::ToCString(result));
                        }
                        else
                        {
                            bindings.push_back(hapticBinding);
                        }
                    }
                    suggestedBindings.suggestedBindings = bindings.data();
                    suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
                    if (const XrResult suggestResult =
                            OpenXrApi::xrSuggestInteractionProfileBindings(GetXrInstance(), &suggestedBindings);
                        XR_FAILED(suggestResult))
                    {
                        ErrorLog("%s: unable to suggest bindings: %s", __FUNCTION__, xr::ToCString(suggestResult));
                    }
                    else
                    {
                        m_InteractionProfileSuggested = true;
                        m_SimpleProfileSuggested = true;
                        Log("suggested %s as fallback", profile.c_str());
                        TraceLoggingWrite(g_traceProvider,
                                          "xrAttachSessionActionSets",
                                          TLArg(caller.c_str(), "Called by"),
                                          TLArg(profile.c_str(), "Profile"),
                                          TLPArg(poseBinding.action, "Action"),
                                          TLArg(posePath.c_str(), "PosePath"),
                                          TLArg(movePath.c_str(), "MovePath"),
                                          TLArg(positionPath.c_str(), "PositionPath"),
                                          TLArg(hapticPath.c_str(), "HapticPath"));
                    }
                }
            }
        }
    }

    bool OpenXrLayer::LazyInit(const XrTime time)
    {
        bool success = true;
        if (!CreateStageSpace("LazyInit"))
        {
            success = false;
        }

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
        return success;
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
            ErrorLog("%s: unable to convert XrPath %u to string: %s", __FUNCTION__, path, xr::ToCString(result));
        }
        return str;
    }

    bool OpenXrLayer::TestRotation(XrPosef* pose, const XrTime time, const bool reverse) const
    {
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

        return true;
    }

    std::unique_ptr<OpenXrLayer> g_instance = nullptr;

    OpenXrApi* GetInstance()
    {
        if (!g_instance)
        {
            g_instance = std::make_unique<OpenXrLayer>();
        }
        return g_instance.get();
    }

    void ResetInstance()
    {
        g_instance.reset();
    }

} // namespace openxr_api_layer

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        TraceLoggingRegister(openxr_api_layer::log::g_traceProvider);
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    default: ;
    }
    return TRUE;
}
