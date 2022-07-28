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

#pragma once

#include "pch.h"

#include "layer.h"
#include "tracker.h"
#include "utility.h"
#include "config.h"
#include <log.h>
#include <util.h>


using namespace motion_compensation_layer::log;
using namespace xr::math;
namespace motion_compensation_layer
{
    OpenXrLayer::~OpenXrLayer()
    {
        if (m_Tracker)
        {
            delete m_Tracker;
        }
    }

    XrResult OpenXrLayer::xrCreateInstance(const XrInstanceCreateInfo* createInfo)
    {
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
        OpenXrApi::xrCreateInstance(createInfo);

        m_Application = GetApplicationName();

        // Dump the application name and OpenXR runtime information to help debugging issues.
        XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES};
        CHECK_XRCMD(OpenXrApi::xrGetInstanceProperties(GetXrInstance(), &instanceProperties));
        const auto runtimeName = fmt::format("{} {}.{}.{}",
                                             instanceProperties.runtimeName,
                                             XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
                                             XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                                             XR_VERSION_PATCH(instanceProperties.runtimeVersion));
        TraceLoggingWrite(g_traceProvider, "xrCreateInstance", TLArg(runtimeName.c_str(), "RuntimeName"));
        TraceLoggingWrite(g_traceProvider, "xrCreateInstance", TLArg(m_Application.c_str(), "ApplicationName"));
        Log("Application: %s\n", m_Application.c_str());
        Log("Using OpenXR runtime: %s\n", runtimeName.c_str());

        // Initialize Configuration
        m_Initialized = GetConfig()->Init(m_Application);

        // enable debug test rotation
        if (m_Initialized)
        {
            GetConfig()->GetBool(Cfg::TestRotation, m_TestRotation);
        }

        // Create the resources for the tracker space.
        {
            XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO, nullptr};
            strcpy_s(actionSetCreateInfo.actionSetName, "general_tracker_set");
            strcpy_s(actionSetCreateInfo.localizedActionSetName, "General Tracker Set");
            actionSetCreateInfo.priority = 0;
            if (XR_SUCCESS != xrCreateActionSet(GetXrInstance(), &actionSetCreateInfo, &m_ActionSet))
            {
                ErrorLog("%s: error creating action set\n", __FUNCTION__);
                m_Initialized = false;
            }
            TraceLoggingWrite(g_traceProvider, "OpenXrTracker::Init", TLPArg(m_ActionSet, "xrCreateActionSet"));
        }
        {
            XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO, nullptr};
            strcpy_s(actionCreateInfo.actionName, "general_tracker");
            strcpy_s(actionCreateInfo.localizedActionName, "General Tracker");
            actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            actionCreateInfo.countSubactionPaths = 0;
            if (XR_SUCCESS != xrCreateAction(m_ActionSet, &actionCreateInfo, &m_TrackerPoseAction))
            {
                ErrorLog("%s: error creating action\n", __FUNCTION__);
                m_Initialized = false;
            }
            TraceLoggingWrite(g_traceProvider, "OpenXrTracker::Init", TLPArg(m_TrackerPoseAction, "xrCreateAction"));
        }
        {
            // suggest simple controller
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                                                                   nullptr};
            CHECK_XRCMD(xrStringToPath(GetXrInstance(),
                                       "/interaction_profiles/khr/simple_controller",
                                       &suggestedBindings.interactionProfile));
            std::string side{"left"};
            if (!GetConfig()->GetString(Cfg::TrackerSide, side))
            {
                ErrorLog("%s: unable to determine contoller side. Defaulting to %s\n", __FUNCTION__, side);
            }
            if ("right" != side && "left" != side)
            {
                ErrorLog("%s: invalid contoller side: %s. Defaulting to 'left'\n", __FUNCTION__, side);
                side = "left";
            }
            else
            {
                const std::string path("/user/hand/" + side + "/input/grip/pose");
                XrActionSuggestedBinding binding;
                binding.action = m_TrackerPoseAction;
                CHECK_XRCMD(xrStringToPath(GetXrInstance(), path.c_str(), &binding.binding));

                suggestedBindings.suggestedBindings = &binding;
                suggestedBindings.countSuggestedBindings = 1;
                CHECK_XRCMD(xrSuggestInteractionProfileBindings(GetXrInstance(), &suggestedBindings));
                TraceLoggingWrite(g_traceProvider,
                                  "OpenXrTracker::Init",
                                  TLArg("/interaction_profiles/khr/simple_controller", "Profile"),
                                  TLPArg(binding.action, "Action"),
                                  TLArg(path.c_str(), "Path"));
            }
        }

        // initialize tracker
        GetTracker(&m_Tracker);
        if (!m_Tracker->Init())
        {
            m_Initialized = false;
        }

        // initialize keyboard input handler
        if (!m_Input.Init())
        {
            m_Initialized = false;
        }

        return XR_SUCCESS;
    }

    XrResult OpenXrLayer::xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
    {
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
                Log("Using OpenXR system: %s\n", systemProperties.systemName);
            }

            // Remember the XrSystemId to use.
            m_systemId = *systemId;
        }

        TraceLoggingWrite(g_traceProvider, "xrGetSystem", TLArg((int)*systemId, "SystemId"));

        return result;
    }

    XrResult OpenXrLayer::xrCreateSession(XrInstance instance,
                                          const XrSessionCreateInfo* createInfo,
                                          XrSession* session)
    {
        if (createInfo->type != XR_TYPE_SESSION_CREATE_INFO)
        {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrCreateSession",
                          TLPArg(instance, "Instance"),
                          TLArg((int)createInfo->systemId, "SystemId"),
                          TLArg(createInfo->createFlags, "CreateFlags"));

        const XrResult result = OpenXrApi::xrCreateSession(instance, createInfo, session);
        if (XR_SUCCEEDED(result))
        {
            if (isSystemHandled(createInfo->systemId))
            {
                XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                                                                    nullptr,
                                                                    XR_REFERENCE_SPACE_TYPE_VIEW,
                                                                    Pose::Identity()};
                CHECK_XRCMD(xrCreateReferenceSpace(*session, &referenceSpaceCreateInfo, &m_ViewSpace));
            }

            TraceLoggingWrite(g_traceProvider, "xrCreateSession", TLPArg(*session, "Session"));
        }

        return result;
    }

    XrResult OpenXrLayer::xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
    {
        DebugLog("xrBeginSession\n");
        TraceLoggingWrite(
            g_traceProvider,
            "xrBeginSession",
            TLPArg(session, "Session"),
            TLArg(xr::ToCString(beginInfo->primaryViewConfigurationType), "PrimaryViewConfigurationType"));

        XrResult result = OpenXrApi::xrBeginSession(session, beginInfo);
        m_Session = session;
        m_ViewConfigType = beginInfo->primaryViewConfigurationType;

        // create action space
        XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr};
        actionSpaceCreateInfo.action = m_TrackerPoseAction;
        actionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
        actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
        CHECK_XRCMD(GetInstance()->xrCreateActionSpace(m_Session, &actionSpaceCreateInfo, &m_TrackerSpace));
        TraceLoggingWrite(g_traceProvider,
                          "OpenXrTracker::beginSession",
                          TLPArg(m_TrackerSpace, "xrCreateActionSpace"));

        // start tracker session
        m_Tracker->beginSession(session);

        return result;
    }

    XrResult OpenXrLayer::xrEndSession(XrSession session)
    {
        DebugLog("xrEndSession\n");
        TraceLoggingWrite(g_traceProvider, "xrEndssion", TLPArg(session, "Session"));
        if (m_TrackerSpace != XR_NULL_HANDLE)
        {
            GetInstance()->xrDestroySpace(m_TrackerSpace);
            m_TrackerSpace = XR_NULL_HANDLE;
        }
        m_Tracker->endSession();
        return OpenXrApi::xrEndSession(session);
    }

    XrResult OpenXrLayer::xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
    {
        DebugLog("xrAttachSessionActionSets\n");
        TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets", TLPArg(session, "Session"));
        for (uint32_t i = 0; i < attachInfo->countActionSets; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrAttachSessionActionSets",
                              TLPArg(attachInfo->actionSets[i], "ActionSet"));
        }

        XrSessionActionSetsAttachInfo chainAttachInfo = *attachInfo;
        std::vector<XrActionSet> newActionSets;

        newActionSets.resize((size_t)chainAttachInfo.countActionSets);
        memcpy(newActionSets.data(), chainAttachInfo.actionSets, chainAttachInfo.countActionSets * sizeof(XrActionSet));
        newActionSets.push_back(m_ActionSet);

        chainAttachInfo.actionSets = newActionSets.data();
        chainAttachInfo.countActionSets = (uint32_t)newActionSets.size();

        XrResult result = OpenXrApi::xrAttachSessionActionSets(session, &chainAttachInfo);
        if (XR_SUCCEEDED(result))
        {
            Log("tracker action set attached\n");
            m_Tracker->m_SkipLazyInit = true;
        }
        return result;
    }

    XrResult
    OpenXrLayer::xrSuggestInteractionProfileBindings(XrInstance instance,
                                                     const XrInteractionProfileSuggestedBinding* suggestedBindings)
    {
        const std::string profile = getXrPath(suggestedBindings->interactionProfile);
        TraceLoggingWrite(g_traceProvider,
                          "xrSuggestInteractionProfileBindings",
                          TLPArg(instance, "Instance"),
                          TLArg(profile.c_str(), "InteractionProfile"));

        DebugLog("xrSuggestInteractionProfileBindings: %s\n", profile.c_str());
        for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrSuggestInteractionProfileBindings",
                              TLPArg(suggestedBindings->suggestedBindings[i].action, "Action"),
                              TLArg(getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str(), "Path"));

            DebugLog("binding: %s\n", getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str());
        }

        XrInteractionProfileSuggestedBinding bindingProfiles = *suggestedBindings;
        std::vector<XrActionSuggestedBinding> bindings{};

        std::string side{"left"};
        if (!GetConfig()->GetString(Cfg::TrackerSide, side))
        {
            ErrorLog("%s: unable to determine contoller side. Defaulting to %s\n", __FUNCTION__, side);
        }
        if ("right" != side && "left" != side)
        {
            ErrorLog("%s: invalid contoller side: %s. Defaulting to 'left'\n", side);
            side = "left";
        }
        const std::string path("/user/hand/" + side + "/input/grip/pose");

        // left or right hand pose action
        // TODO: find a way to persist original pose action?
        bindings.resize((uint32_t)bindingProfiles.countSuggestedBindings);
        memcpy(bindings.data(),
               bindingProfiles.suggestedBindings,
               bindingProfiles.countSuggestedBindings * sizeof(XrActionSuggestedBinding));
        for (XrActionSuggestedBinding& curBinding : bindings)
        {
            if (getXrPath(curBinding.binding) == path)
            {
                curBinding.action = m_TrackerPoseAction;
                Log("Binding %s - %s overridden with reference tracker action\n", profile.c_str(), path.c_str());
            }
        }
        bindingProfiles.suggestedBindings = bindings.data();
        bindingProfiles.countSuggestedBindings = (uint32_t)bindings.size();
        return OpenXrApi::xrSuggestInteractionProfileBindings(instance, &bindingProfiles);
    }

    XrResult OpenXrLayer::xrCreateReferenceSpace(XrSession session,
                                                 const XrReferenceSpaceCreateInfo* createInfo,
                                                 XrSpace* space)
    {
        TraceLoggingWrite(g_traceProvider,
                          "xrCreateReferenceSpace",
                          TLPArg(session, "Session"),
                          TLArg(xr::ToCString(createInfo->referenceSpaceType), "ReferenceSpaceType"),
                          TLArg(xr::ToString(createInfo->poseInReferenceSpace).c_str(), "PoseInReferenceSpace"));

        const XrResult result = OpenXrApi::xrCreateReferenceSpace(session, createInfo, space);
        if (XR_SUCCEEDED(result))
        {
            DebugLog("xrCreateReferenceSpace: %d type: %d \n", *space, createInfo->referenceSpaceType);

            if (XR_REFERENCE_SPACE_TYPE_VIEW == createInfo->referenceSpaceType)
            {
                Log("creation of view space detected: %d\n", *space);

                // memorize view spaces
                TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace", TLArg("View_Space", "Added"));
                m_ViewSpaces.insert(*space);
            }
            else
            {
                if (XR_REFERENCE_SPACE_TYPE_LOCAL == createInfo->referenceSpaceType)
                {
                    // view reset -> recreate tracker reference space to match
                    Log("creation of local reference space detected: %d\n", *space);
                    TraceLoggingWrite(g_traceProvider,
                                      "xrCreateReferenceSpace",
                                      TLArg("Tracker_Reference_Space", "Reset"));

                    m_ReferenceSpace = *space;

                    // and reset tracker reference pose
                    // TODO: modify reference pose instead?
                    m_Tracker->m_ResetReferencePose = true;
                }
            }
        }
        TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace", TLPArg(*space, "Space"));

        return result;
    }

    XrResult OpenXrLayer::xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
    {
        TraceLoggingWrite(g_traceProvider,
                          "xrLocateSpace",
                          TLPArg(space, "Space"),
                          TLPArg(baseSpace, "BaseSpace"),
                          TLArg(time, "Time"));
        DebugLog("xrLocateSpace: %d %d\n", space, baseSpace);

        // determine original location
        CHECK_XRCMD(OpenXrApi::xrLocateSpace(space, baseSpace, time, location));

        if (m_Activated && (isViewSpace(space) || isViewSpace(baseSpace)))
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrLocateSpace",
                              TLArg(xr::ToString(location->pose).c_str(), "Pose_Before"),
                              TLArg(location->locationFlags, "LocationFlags"));

            // manipulate pose using tracker
            bool spaceIsViewSpace = isViewSpace(space);
            bool baseSpaceIsViewSpace = isViewSpace(baseSpace);

            XrPosef trackerDelta = Pose::Identity();
            // TestRotation(&trackerDelta, time, false);
            bool success = !m_TestRotation ? m_Tracker->GetPoseDelta(trackerDelta, time)
                                           : TestRotation(&trackerDelta, time, false);
            if (success)
            {
                if (spaceIsViewSpace && !baseSpaceIsViewSpace)
                {
                    location->pose = Pose::Multiply(location->pose, trackerDelta);
                }
                if (baseSpaceIsViewSpace && !spaceIsViewSpace)
                {
                    // TODO: verify calculation
                    location->pose = Pose::Multiply(location->pose, Pose::Invert(trackerDelta));
                }
            }
            else
            {
                ErrorLog("unable to retrieve tracker pose delta\n");
            }
            // TODO: use either location or eye cache?
            // safe pose for use in xrEndFrame
            m_PoseCache.AddSample(time, trackerDelta);

            TraceLoggingWrite(g_traceProvider,
                              "xrLocateSpace",
                              TLArg(xr::ToString(location->pose).c_str(), "Pose_After"));
        }

        return XR_SUCCESS;
    }

    XrResult OpenXrLayer::xrLocateViews(XrSession session,
                                        const XrViewLocateInfo* viewLocateInfo,
                                        XrViewState* viewState,
                                        uint32_t viewCapacityInput,
                                        uint32_t* viewCountOutput,
                                        XrView* views)
    {
        TraceLoggingWrite(g_traceProvider,
                          "xrLocateViews",
                          TLPArg(session, "Session"),
                          TLArg(xr::ToCString(viewLocateInfo->viewConfigurationType), "ViewConfigurationType"),
                          TLArg(viewLocateInfo->displayTime, "DisplayTime"),
                          TLPArg(viewLocateInfo->space, "Space"),
                          TLArg(viewCapacityInput, "ViewCapacityInput"));

        DebugLog("xrLocateViews: %d\n", viewLocateInfo->space);

        CHECK_XRCMD(
            OpenXrApi::xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views));

        TraceLoggingWrite(g_traceProvider, "xrLocateViews", TLArg(viewState->viewStateFlags, "ViewStateFlags"));

        if (!m_Activated)
        {
            return XR_SUCCESS;
        }

        // store eye poses to avoid recalculation in xrEndFrame?
        std::vector<XrPosef> originalEyePoses{};
        for (uint32_t i = 0; i < *viewCountOutput; i++)
        {
            originalEyePoses.push_back(views[i].pose);
        }
        m_EyeCache.AddSample(viewLocateInfo->displayTime, originalEyePoses);

        if (m_EyeOffsets.empty())
        {
            // determine eye poses
            XrViewLocateInfo offsetViewLocateInfo{viewLocateInfo->type,
                                                  nullptr,
                                                  viewLocateInfo->viewConfigurationType,
                                                  viewLocateInfo->displayTime,
                                                  m_ViewSpace};

            CHECK_XRCMD(OpenXrApi::xrLocateViews(session,
                                                 &offsetViewLocateInfo,
                                                 viewState,
                                                 viewCapacityInput,
                                                 viewCountOutput,
                                                 views));
            for (uint32_t i = 0; i < *viewCountOutput; i++)
            {
                m_EyeOffsets.push_back(views[i]);
            }
        }

        // manipulate reference space location
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        CHECK_XRCMD(xrLocateSpace(m_ViewSpace, viewLocateInfo->space, viewLocateInfo->displayTime, &location));
        for (uint32_t i = 0; i < *viewCountOutput; i++)
        {
            TraceLoggingWrite(g_traceProvider, "xrLocateViews", TLArg(xr::ToString(views[i].fov).c_str(), "Fov"));
            TraceLoggingWrite(g_traceProvider,
                              "xrLocateViews",
                              TLArg(xr::ToString(views[i].pose).c_str(), "Pose_Before"));

            // apply manipulation
            views[i].pose = Pose::Multiply(m_EyeOffsets[i].pose, location.pose);

            TraceLoggingWrite(g_traceProvider,
                              "xrLocateViews",
                              TLArg(xr::ToString(views[i].pose).c_str(), "Pose_After"));
        }

        return XR_SUCCESS;
    }

    XrResult OpenXrLayer::xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
    {
        TraceLoggingWrite(g_traceProvider, "xrSyncActions", TLPArg(session, "Session"));
        for (uint32_t i = 0; i < syncInfo->countActiveActionSets; i++)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrSyncActions",
                              TLPArg(syncInfo->activeActionSets[i].actionSet, "ActionSet"),
                              TLArg(syncInfo->activeActionSets[i].subactionPath, "SubactionPath"));
        }

        XrActionsSyncInfo chainSyncInfo = *syncInfo;
        std::vector<XrActiveActionSet> newActiveActionSets;
        const auto trackerActionSet = m_ActionSet;
        if (trackerActionSet != XR_NULL_HANDLE)
        {
            newActiveActionSets.resize((size_t)chainSyncInfo.countActiveActionSets + 1);
            memcpy(newActiveActionSets.data(),
                   chainSyncInfo.activeActionSets,
                   chainSyncInfo.countActiveActionSets * sizeof(XrActionSet));
            uint32_t nextActionSetSlot = chainSyncInfo.countActiveActionSets;

            newActiveActionSets[nextActionSetSlot].actionSet = trackerActionSet;
            newActiveActionSets[nextActionSetSlot++].subactionPath = XR_NULL_PATH;

            chainSyncInfo.activeActionSets = newActiveActionSets.data();
            chainSyncInfo.countActiveActionSets = nextActionSetSlot;
        }

        return OpenXrApi::xrSyncActions(session, syncInfo);
    }

    XrResult OpenXrLayer::xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
    {
        TraceLoggingWrite(g_traceProvider,
                          "xrEndFrame",
                          TLPArg(session, "Session"),
                          TLArg(frameEndInfo->displayTime, "DisplayTime"),
                          TLArg(xr::ToCString(frameEndInfo->environmentBlendMode), "EnvironmentBlendMode"));

        if (!m_Activated)
        {
            HandleKeyboardInput(frameEndInfo->displayTime);
            return OpenXrApi::xrEndFrame(session, frameEndInfo);
        }

        std::vector<const XrCompositionLayerBaseHeader*> resetLayers{};
        std::vector<XrCompositionLayerProjection*> resetProjectionLayers{};
        std::vector<std::vector<XrCompositionLayerProjectionView>*> resetViews{};

        for (uint32_t i = 0; i < frameEndInfo->layerCount; i++)
        {
            XrCompositionLayerBaseHeader baseHeader = *frameEndInfo->layers[i];
            XrCompositionLayerBaseHeader* headerPtr{nullptr};
            if (XR_TYPE_COMPOSITION_LAYER_PROJECTION == baseHeader.type)
            {
                DebugLog("xrEndFrame: projection layer %d, space: %d\n", i, baseHeader.space);

                const XrCompositionLayerProjection* projectionLayer =
                    reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[i]);

                TraceLoggingWrite(g_traceProvider,
                                  "xrEndFrame_Layer",
                                  TLArg("ProjectionLayer", "Type"),
                                  TLArg(projectionLayer->layerFlags, "Flags"),
                                  TLPArg(projectionLayer->space, "Space"));

                std::vector<XrCompositionLayerProjectionView>* projectionViews =
                    new std::vector<XrCompositionLayerProjectionView>{};
                resetViews.push_back(projectionViews);
                projectionViews->resize(projectionLayer->viewCount);
                memcpy(projectionViews->data(),
                       projectionLayer->views,
                       projectionLayer->viewCount * sizeof(XrCompositionLayerProjectionView));

                if (!m_EyeCache.empty())
                {
                    // use eye cache if available
                    std::vector<XrPosef> cachedEyePoses = m_EyeCache.GetSample(frameEndInfo->displayTime);
                    m_EyeCache.CleanUp(frameEndInfo->displayTime);
                    m_PoseCache.CleanUp(frameEndInfo->displayTime);
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

                        (*projectionViews)[j].pose = cachedEyePoses[j];

                        TraceLoggingWrite(g_traceProvider,
                                          "xrEndFrame_View",
                                          TLArg("View_After", "Type"),
                                          TLArg(xr::ToString(cachedEyePoses[j]).c_str(), "Pose"),
                                          TLArg(j, "Index"));
                    }
                }
                else
                {
                    // use pose cache for reverse calculation
                    XrPosef reversedManipulation = Pose::Invert(m_PoseCache.GetSample(frameEndInfo->displayTime));
                    m_PoseCache.CleanUp(frameEndInfo->displayTime);

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

                        (*projectionViews)[j].pose = Pose::Multiply((*projectionViews)[j].pose, reversedManipulation);

                        TraceLoggingWrite(g_traceProvider,
                                          "xrEndFrame_View",
                                          TLArg("View_After", "Type"),
                                          TLArg(xr::ToString((*projectionViews)[j].pose).c_str(), "Pose"),
                                          TLArg(j, "Index"));
                    }
                }
                // create layer with reset view poses
                XrCompositionLayerProjection* const resetProjectionLayer =
                    new XrCompositionLayerProjection{projectionLayer->type,
                                                     projectionLayer->next,
                                                     projectionLayer->layerFlags,
                                                     projectionLayer->space,
                                                     projectionLayer->viewCount,
                                                     projectionViews->data()};

                resetProjectionLayers.push_back(resetProjectionLayer);
                headerPtr = reinterpret_cast<XrCompositionLayerBaseHeader*>(resetProjectionLayer);
            }
            if (!headerPtr)
            {
                resetLayers.push_back(frameEndInfo->layers[i]);
            }
            else
            {
                const XrCompositionLayerBaseHeader* const baseHeaderPtr = headerPtr;
                resetLayers.push_back(baseHeaderPtr);
            }
        }
        HandleKeyboardInput(frameEndInfo->displayTime);

        XrFrameEndInfo resetFrameEndInfo{frameEndInfo->type,
                                         frameEndInfo->next,
                                         frameEndInfo->displayTime,
                                         frameEndInfo->environmentBlendMode,
                                         frameEndInfo->layerCount,
                                         resetLayers.data()};

        XrResult result = OpenXrApi::xrEndFrame(session, &resetFrameEndInfo);

        // clean up memory
        for (auto layer : resetProjectionLayers)
        {
            delete layer;
        }
        for (auto views : resetViews)
        {
            delete views;
        }

        return result;
    }

    // private
    bool OpenXrLayer::isSystemHandled(XrSystemId systemId) const
    {
        return systemId == m_systemId;
    }

    bool OpenXrLayer::isViewSpace(XrSpace space) const
    {
        return m_ViewSpaces.count(space);
    }

    uint32_t OpenXrLayer::GetNumViews()
    {
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO == m_ViewConfigType                                ? 1
               : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO == m_ViewConfigType                            ? 2
               : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO == m_ViewConfigType                        ? 4
               : XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT == m_ViewConfigType ? 1
                                                                                                          : 0;
    }

    void OpenXrLayer::ToggleActive(XrTime time)
    {
        // handle debug test rotation
        if (m_TestRotation)
        {
            m_TestRotStart = time;
            m_Activated = !m_Activated;
            Log("test rotation motion compensation % s\n", m_Activated ? "activated" : "deactivated");
            return;
        }

        // perform last-minute tracker initialization
        bool lazySuccess = m_Tracker->LazyInit();

        const bool oldstate = m_Activated;
        if (m_Initialized && lazySuccess)
        {
            // if tracker is not initialized, activate only after successful init
            m_Activated = m_Tracker->m_Calibrated ? !m_Activated : m_Tracker->ResetReferencePose(time);
        }
        else
        {
            ErrorLog("layer intitalization failed or incomplete!");
        }
        Log("motion compensation %s\n",
            oldstate != m_Activated ? (m_Activated ? "activated" : "deactivated")
            : m_Activated           ? "kept active"
                                    : "could not be activated");
        if (oldstate != m_Activated)
        {
            MessageBeep(m_Activated ? MB_OK : MB_ICONWARNING);
        }
        else if (!m_Activated)
        {
            MessageBeep(MB_ICONERROR);
        }
        TraceLoggingWrite(g_traceProvider,
                          "ToggleActive",
                          TLArg(m_Activated ? "Deactivated" : "Activated", "Motion_Compensation"),
                          TLArg(time, "Time"));
    }

    void OpenXrLayer::Recalibrate(XrTime time)
    {
        if (m_TestRotation)
        {
            m_TestRotStart = time;
            Log("test rotation motion compensation recalibrated");
            return;
        }

        if (m_Tracker->ResetReferencePose(time))
        {
            MessageBeep(MB_OK);
            TraceLoggingWrite(g_traceProvider,
                              "HandleKeyboardInput",
                              TLArg("Reset", "Tracker_Reference"),
                              TLArg(time, "Time"));
        }
        else
        {
            // failed to update reference pose -> deactivate mc
            if (m_Activated)
            {
                ErrorLog("motion compensation deactivated because tracker reference pose cold not be reset\n");
            }
            m_Activated = false;
            MessageBeep(MB_ICONERROR);
            TraceLoggingWrite(g_traceProvider,
                              "HandleKeyboardInput",
                              TLArg("Deactivated_Reset", "Motion_Compensation"),
                              TLArg(time, "Time"));
        }
    }

    void OpenXrLayer::ReloadConfig()
    {
        m_Tracker->m_Calibrated = false;
        m_Activated = false;
        bool success = GetConfig()->Init(m_Application);
        if (success)
        {
            GetConfig()->GetBool(Cfg::TestRotation, m_TestRotation);
            GetTracker(&m_Tracker);
        }
        if (!m_Tracker->LoadFilters())
        {
            success = false;
        }
        MessageBeep(success ? MB_OK : MB_ICONERROR);
    }

    bool OpenXrLayer::LazyInit()
    {
        bool success = true;
        if (m_ReferenceSpace == XR_NULL_HANDLE)
        {
            Log("reference space created during lazy init\n");
            // Create a reference space.
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
            referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
            TraceLoggingWrite(g_traceProvider, "OpenXrTracker::LazyInit", TLPArg("Executed", "xrCreateReferenceSpace"));
            if (!XR_SUCCEEDED(xrCreateReferenceSpace(m_Session, &referenceSpaceCreateInfo, &m_ReferenceSpace)))
            {
                ErrorLog("%s: xrCreateReferenceSpace failed\n", __FUNCTION__);
                success = false;
            }
        }
        if (!m_Tracker->LazyInit())
        {
            success = false;
        }
        return success;
    }


    void OpenXrLayer::HandleKeyboardInput(XrTime time)
    {
        bool isRepeat{false};
        if (m_Input.GetKeyState(Cfg::KeyActivate, isRepeat) && !isRepeat)
        {
            ToggleActive(time);
        }
        isRepeat = false;
        if (m_Input.GetKeyState(Cfg::KeyCenter, isRepeat) && !isRepeat)
        {
            Recalibrate(time);
        }
        isRepeat = false;
        if (m_Input.GetKeyState(Cfg::KeyTransInc, isRepeat))
        {
            m_Tracker->ModifyFilterStrength(true, true);
        }
        isRepeat = false;
        if (m_Input.GetKeyState(Cfg::KeyTransDec, isRepeat))
        {
            m_Tracker->ModifyFilterStrength(true, false);
        }
        isRepeat = false;
        if (m_Input.GetKeyState(Cfg::KeyRotInc, isRepeat))
        {
            m_Tracker->ModifyFilterStrength(false, true);
        }
        isRepeat = false;
        if (m_Input.GetKeyState(Cfg::KeyRotDec, isRepeat))
        {
            m_Tracker->ModifyFilterStrength(false, false);
        }
        isRepeat = false;
        if (m_Input.GetKeyState(Cfg::KeySaveConfig, isRepeat) && !isRepeat)
        {
            GetConfig()->WriteConfig();
        }
        isRepeat = false;
        if (m_Input.GetKeyState(Cfg::KeyReloadConfig, isRepeat) && !isRepeat)
        {
            ReloadConfig();
        }
    }

    std::string OpenXrLayer::getXrPath(XrPath path)
    {
        char buf[XR_MAX_PATH_LENGTH];
        uint32_t count;
        CHECK_XRCMD(GetInstance()->xrPathToString(GetInstance()->GetXrInstance(), path, sizeof(buf), &count, buf));
        std::string str;
        str.assign(buf, count - 1);
        return str;
    }

    bool OpenXrLayer::TestRotation(XrPosef* pose, XrTime time, bool reverse)
    {
        // save current location
        XrVector3f pos = pose->position;

        // determine rotation angle
        int64_t milliseconds = ((time - m_TestRotStart) / 1000000) % 10000;
        float angle = (float)M_PI * 0.0002f * milliseconds;
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
} // namespace motion_compensation_layer

namespace motion_compensation_layer
{
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

} // namespace motion_compensation_layer

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        TraceLoggingRegister(motion_compensation_layer::log::g_traceProvider);
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
