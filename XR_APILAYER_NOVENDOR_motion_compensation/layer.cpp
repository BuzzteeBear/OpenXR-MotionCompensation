// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
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

namespace
{
    using namespace motion_compensation_layer;
    using namespace motion_compensation_layer::log;
    using namespace xr::math;

    class OpenXrLayer : public motion_compensation_layer::OpenXrApi
    {
      public:
        OpenXrLayer() = default;
        
        // TODO: add xrEndSesion and clean up memory

        XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo) override
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

            // Dump the application name and OpenXR runtime information to help debugging issues.
            XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES};
            CHECK_XRCMD(OpenXrApi::xrGetInstanceProperties(GetXrInstance(), &instanceProperties));
            const auto runtimeName = fmt::format("{} {}.{}.{}",
                                                 instanceProperties.runtimeName,
                                                 XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
                                                 XR_VERSION_MINOR(instanceProperties.runtimeVersion),
                                                 XR_VERSION_PATCH(instanceProperties.runtimeVersion));
            TraceLoggingWrite(g_traceProvider, "xrCreateInstance", TLArg(runtimeName.c_str(), "RuntimeName"));
            TraceLoggingWrite(g_traceProvider, "xrCreateInstance", TLArg(GetInstance()->GetApplicationName().c_str(), "ApplicationName"));
            Log("Application: %s\n", GetApplicationName().c_str());
            Log("Using OpenXR runtime: %s\n", runtimeName.c_str());

            // Initialize Configuration
            if (m_Config.Init(GetInstance()->GetApplicationName()))
            {
                // TODO: prevent activation without valid configuration
            }

            // initialize tracker object
            m_Tracker.Init();
            
            return XR_SUCCESS;
        }

        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override
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

        XrResult xrCreateSession(XrInstance instance,
                                 const XrSessionCreateInfo* createInfo,
                                 XrSession* session) override
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

        XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
        {
            TraceLoggingWrite(
                g_traceProvider,
                "xrBeginSession",
                TLPArg(session, "Session"),
                TLArg(xr::ToCString(beginInfo->primaryViewConfigurationType), "PrimaryViewConfigurationType"));
            
            XrResult result = OpenXrApi::xrBeginSession(session, beginInfo);
            m_ViewConfigType = beginInfo->primaryViewConfigurationType;
           
            // start tracker session
            m_Tracker.beginSession(session);
            
            return result;
        }

        XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo) override
        {
            TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets", TLPArg(session, "Session"));
            for (uint32_t i = 0; i < attachInfo->countActionSets; i++)
            {
                TraceLoggingWrite(g_traceProvider,
                                  "xrAttachSessionActionSets",
                                  TLPArg(attachInfo->actionSets[i], "ActionSet"));
            }

            XrSessionActionSetsAttachInfo chainAttachInfo = *attachInfo;
            std::vector<XrActionSet> newActionSets;

            const auto trackerActionSet = m_Tracker.m_ActionSet;
            if (trackerActionSet != XR_NULL_HANDLE)
            {
                newActionSets.resize((size_t)chainAttachInfo.countActionSets + 1);
                memcpy(newActionSets.data(),
                       chainAttachInfo.actionSets,
                       chainAttachInfo.countActionSets * sizeof(XrActionSet));
                uint32_t nextActionSetSlot = chainAttachInfo.countActionSets;

                newActionSets[nextActionSetSlot++] = trackerActionSet;

                chainAttachInfo.actionSets = newActionSets.data();
                chainAttachInfo.countActionSets = nextActionSetSlot;
            }
            m_Tracker.m_IsActionSetAttached = true;    

            return OpenXrApi::xrAttachSessionActionSets(session, &chainAttachInfo);
        }

        XrResult xrSuggestInteractionProfileBindings(XrInstance instance,
                                                     const XrInteractionProfileSuggestedBinding* suggestedBindings)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrSuggestInteractionProfileBindings",
                              TLPArg(instance, "Instance"),
                              TLArg(getXrPath(suggestedBindings->interactionProfile).c_str(), "InteractionProfile"));
            ;
            DebugLog("suggestedBindings: %s\n", getXrPath(suggestedBindings->interactionProfile).c_str());
            for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; i++)
            {
                TraceLoggingWrite(g_traceProvider,
                                  "xrSuggestInteractionProfileBindings",
                                  TLPArg(suggestedBindings->suggestedBindings[i].action, "Action"),
                                  TLArg(getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str(), "Path"));

                DebugLog("\tbinding: %s\n", getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str());
            }

            XrInteractionProfileSuggestedBinding bindingProfiles = *suggestedBindings;
            std::vector<XrActionSuggestedBinding> bindings{};

            // override left hand pose action
            // TODO: find a way to persist original pose action?
            bindings.resize((uint32_t)bindingProfiles.countSuggestedBindings);
            memcpy(bindings.data(),
                   bindingProfiles.suggestedBindings,
                   bindingProfiles.countSuggestedBindings * sizeof(XrActionSuggestedBinding));
            for (XrActionSuggestedBinding& curBinding : bindings)
            {
                // TODO: use config manager
                if (getXrPath(curBinding.binding) == "/user/hand/left/input/grip/pose")
                {
                    curBinding.action = m_Tracker.m_TrackerPoseAction;
                    m_Tracker.m_IsBindingSuggested = true;
                }
            }

            bindingProfiles.suggestedBindings = bindings.data();
            return OpenXrApi::xrSuggestInteractionProfileBindings(instance, &bindingProfiles);
        }

        XrResult xrCreateReferenceSpace(XrSession session,
                                        const XrReferenceSpaceCreateInfo* createInfo,
                                        XrSpace* space) override
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
                    // memorize view spaces
                    DebugLog("xrCreateReferenceSpace::addViewSpace: %d\n", *space);
                    TraceLoggingWrite(g_traceProvider,
                                      "xrCreateReferenceSpace",
                                      TLArg("View_Space", "Added"));
                    m_ViewSpaces.insert(*space);
                }
                else if (XR_REFERENCE_SPACE_TYPE_LOCAL == createInfo->referenceSpaceType)
                {   
                    // view reset -> recreate tracker reference space to match
                    TraceLoggingWrite(g_traceProvider,
                                      "xrCreateReferenceSpace",
                                      TLArg("Tracker_Reference_Space", "Reset"));

                    if (XR_NULL_HANDLE != m_Tracker.m_ReferenceSpace)
                    {
                        CHECK_XRCMD(xrDestroySpace(m_Tracker.m_ReferenceSpace));
                    }
                    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
                    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                    referenceSpaceCreateInfo.poseInReferenceSpace = createInfo->poseInReferenceSpace;
                    CHECK_XRCMD(OpenXrApi::xrCreateReferenceSpace(session,
                                                                  &referenceSpaceCreateInfo,
                                                                  &m_Tracker.m_ReferenceSpace));
                    // and reset tracker reference pose                                              
                    m_Tracker.m_ResetReferencePose = true;  
                }
            }
            TraceLoggingWrite(g_traceProvider, "xrCreateReferenceSpace", TLPArg(*space, "Space"));

            return result;
        }

        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
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
                if (m_Tracker.GetPoseDelta(trackerDelta, time))
                {
                    TraceLoggingWrite(g_traceProvider,
                                      "xrLocateSpace",
                                      TLArg(xr::ToString(trackerDelta).c_str(), "Pose_Tracker"));

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
                // safe pose for use in xrEndFrame
                m_PoseCache.AddPose(time, trackerDelta);
                
                TraceLoggingWrite(g_traceProvider,
                                  "xrLocateSpace",
                                  TLArg(xr::ToString(location->pose).c_str(), "Pose_After"));
            }
            
            return XR_SUCCESS;
        }

        XrResult xrLocateViews(XrSession session,
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

            if (!m_Activated)
            {
                return OpenXrApi::xrLocateViews(session,
                                                viewLocateInfo,
                                                viewState,
                                                viewCapacityInput,
                                                viewCountOutput,
                                                views);
            }

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

            // TODO: store eye poses to avoid recalculation in xrEndFrame?
            TraceLoggingWrite(g_traceProvider, "xrLocateViews", TLArg(viewState->viewStateFlags, "ViewStateFlags"));

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
                views[i].pose = Pose::Multiply(views[i].pose, location.pose);

                TraceLoggingWrite(g_traceProvider,
                                  "xrLocateViews",
                                  TLArg(xr::ToString(views[i].pose).c_str(), "Pose_After"));
            }

            return XR_SUCCESS;
        }

        XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) override
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
            const auto trackerActionSet = m_Tracker.m_ActionSet;
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

        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override
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

                    const XrCompositionLayerProjection * projectionLayer =
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

                   
                    XrPosef reversedManipulation = Pose::Invert(m_PoseCache.GetPose(frameEndInfo->displayTime));
                    m_PoseCache.CleanUp(frameEndInfo->displayTime);
                   
                    TraceLoggingWrite(
                        g_traceProvider,
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

                        (*projectionViews)[j].pose = Pose::Multiply((*projectionViews)[j].pose , reversedManipulation);
                       
                        TraceLoggingWrite(
                            g_traceProvider,
                            "xrEndFrame_View",
                            TLArg("View_After", "Type"),
                            TLArg(xr::ToString((*projectionViews)[j].pose).c_str(), "Pose"),
                            TLArg(j, "Index"));
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

        ConfigManager m_Config;

      private:
        bool isSystemHandled(XrSystemId systemId) const
        {
            return systemId == m_systemId;
        }

        XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

        bool isViewSpace(XrSpace space) const
        {
            return m_ViewSpaces.count(space);
        }

        uint32_t GetNumViews()
        {
            return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO == m_ViewConfigType                                ? 1
                   : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO == m_ViewConfigType                            ? 2
                   : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO == m_ViewConfigType                        ? 4
                   : XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT == m_ViewConfigType ? 1
                                                                                                              : 0;
        }

        void HandleKeyboardInput(XrTime time)
        {
            // TODO: use config manager
            bool isRepeat{false};
            if (m_Input.UpdateKeyState(m_ActivateKeys, isRepeat) && !isRepeat)
            {
                // if tracker is not initialized, activate only after successful init
                m_Activated = m_Tracker.m_IsInitialized ? !m_Activated : m_Tracker.ResetReferencePose(time);
                               
                TraceLoggingWrite(g_traceProvider,
                                  "HandleKeyboardInput",
                                  TLArg(m_Activated ? "Deactivated" : "Activated", "Motion_Compensation"),
                                  TLArg(time, "Time"));
            }
            if (m_Input.UpdateKeyState(m_ResetKeys, isRepeat) && !isRepeat)
            {
                if (!m_Tracker.ResetReferencePose(time))
                {
                    // failed to update reference pose -> deactivate mc
                    m_Activated = false;
                    TraceLoggingWrite(g_traceProvider,
                                      "HandleKeyboardInput",
                                      TLArg("Deactivated_Reset", "Motion_Compensation"),
                                      TLArg(time, "Time"));
                }
                else
                {
                    TraceLoggingWrite(g_traceProvider,
                                      "HandleKeyboardInput",
                                      TLArg("Reset", "Tracker_Reference"),
                                      TLArg(time, "Time"));
                }
            }

        }

        static std::string getXrPath(XrPath path)
        {
            char buf[XR_MAX_PATH_LENGTH];
            uint32_t count;
            CHECK_XRCMD(GetInstance()->xrPathToString(GetInstance()->GetXrInstance(), path, sizeof(buf), &count, buf));
            std::string str;
            str.assign(buf, count - 1);
            return str;
        }

        void TestRotation(XrPosef* pose, XrTime time, bool reverse)
        {
            // save current location
            XrVector3f pos = pose->position;

            // determine rotation angle
            int64_t milliseconds = (time / 1000000) % 10000;
            float angle = (float)M_PI * 0.0002f * milliseconds;
            if (reverse)
            {
                angle = -angle;
            }

            // remove translation to rotate around center
            pose->position = {0, 0, 0};
            StoreXrPose(pose,
                        XMMatrixMultiply(LoadXrPose(*pose), DirectX::XMMatrixRotationRollPitchYaw(0.f, angle, 0.f)));
            // reapply translation
            pose->position = pos;
        }

        bool m_Activated{false};
        std::set<XrSpace> m_ViewSpaces{};
        XrSpace m_ViewSpace{XR_NULL_HANDLE};
        XrViewConfigurationType m_ViewConfigType{XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM}; 
        OpenXrTracker m_Tracker;
        // TODO: make tolerance configurable?
        utilities::PoseCache m_PoseCache{2};
        utilities::KeyboardInput m_Input;
        // TODO: make configurable
        std::set<int> m_ActivateKeys{VK_CONTROL, VK_END};
        std::set<int> m_ResetKeys{VK_CONTROL, VK_RETURN};
    };

    std::unique_ptr<OpenXrLayer> g_instance = nullptr;
} // namespace

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
