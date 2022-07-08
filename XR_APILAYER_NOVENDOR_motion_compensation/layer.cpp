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

        ~OpenXrLayer()
        {
            if (m_Tracker)
            {
                delete m_Tracker;
            }
        };

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
            Log("Application: %s\n", GetApplicationName().c_str());
            Log("Using OpenXR runtime: %s\n", runtimeName.c_str());

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

        XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo) override
        {
            XrSessionActionSetsAttachInfo chainAttachInfo = *attachInfo;
            std::vector<XrActionSet> newActionSets;
            if (m_Tracker)
            {
                const auto trackerActionSet = m_Tracker->m_ActionSet;
                if (trackerActionSet != XR_NULL_HANDLE)
                {
                    newActionSets.resize((size_t)chainAttachInfo.countActionSets + 1);
                    memcpy(newActionSets.data(),
                           chainAttachInfo.actionSets,
                           chainAttachInfo.countActionSets * sizeof(XrActionSet));
                    uint32_t nextActionSetSlot = chainAttachInfo.countActionSets;

                    newActionSets[nextActionSetSlot++] = trackerActionSet;

                    chainAttachInfo.actionSets = newActionSets.data();
                    chainAttachInfo.countActionSets++;
                }
            }

            return OpenXrApi::xrAttachSessionActionSets(session, &chainAttachInfo);
        }

        XrResult xrSuggestInteractionProfileBindings(XrInstance instance,
            const XrInteractionProfileSuggestedBinding* suggestedBindings)
        {
            DebugLog("suggestedBindings: %s\n", getXrPath(suggestedBindings->interactionProfile).c_str());         
            const XrActionSuggestedBinding* it = suggestedBindings->suggestedBindings;
            for (size_t i = 0 ; i < suggestedBindings->countSuggestedBindings ; i++)
            {
                DebugLog("\tbinding: %s\n", getXrPath(it->binding).c_str());
                it++;
            }

            XrInteractionProfileSuggestedBinding bindingProfiles = *suggestedBindings;
            std::vector<XrActionSuggestedBinding> bindings{};
            
            if (m_Tracker)
            {
                // override left hand pose action
                // TODO: find a way to persist original pose action?
                bindings.resize((size_t)bindingProfiles.countSuggestedBindings);
                memcpy(bindings.data(),
                       bindingProfiles.suggestedBindings,
                       bindingProfiles.countSuggestedBindings * sizeof(XrActionSuggestedBinding));
                for (XrActionSuggestedBinding& curBinding : bindings)
                {
                    if (getXrPath(curBinding.binding) == "/user/hand/left/input/grip/pose")
                    {
                        curBinding.action = m_Tracker->m_TrackerPoseAction;
                    }
                }
                bindingProfiles.suggestedBindings = bindings.data();
            }
            return OpenXrApi::xrSuggestInteractionProfileBindings(instance, &bindingProfiles);    
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

                    m_Tracker = new OpenXrTracker(this);
                    m_Tracker->beginSession(*session);                    
                }

                TraceLoggingWrite(g_traceProvider, "xrCreateSession", TLPArg(*session, "Session"));
            }

            return result;
        }

        XrResult xrCreateReferenceSpace(XrSession session,
                                        const XrReferenceSpaceCreateInfo* createInfo,
                                        XrSpace* space) override
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrCreateReferenceSpace",
                              TLPArg(session, "Session"),
                              TLPArg(createInfo, "CreateInfo"));

            const XrResult result = OpenXrApi::xrCreateReferenceSpace(session, createInfo, space);
            if (XR_SUCCEEDED(result))
            {
                // memorize
                if (XR_REFERENCE_SPACE_TYPE_VIEW == createInfo->referenceSpaceType)
                {
                    // TODO: update view space member variable? update tracker reference space?
                    DebugLog("xrCreateReferenceSpace::addViewSpace: %d\n", *space);
                    m_ViewSpaces.insert(*space);
                }
            }

            return result;
        }

        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
        {
            TraceLoggingWrite(g_traceProvider,
                              "xrLocateSpace",
                              TLPArg(space, "Space"),
                              TLPArg(baseSpace, "baseSpace"),
                              TLArg(time, "Time"));
            DebugLog("xrLocateSpace: %d %d\n", space, baseSpace);
            const XrResult result = OpenXrApi::xrLocateSpace(space, baseSpace, time, location);
            if (isViewSpace(space) || isViewSpace(baseSpace))
            {
                bool spaceIsViewSpace = isViewSpace(space);
                bool baseSpaceIsViewSpace = isViewSpace(baseSpace);

                XrPosef trackerDelta = Pose::Identity();
                bool deltaSuccess = m_Tracker->getPoseDelta(trackerDelta, time);
                /*
                if (spaceIsViewSpace && !baseSpaceIsViewSpace)
                {
                    location->pose = Pose::Multiply(location->pose, trackerDelta);
                                    }
                if (baseSpaceIsViewSpace && !spaceIsViewSpace)
                {
                    // TODO: verify calculation
                    location->pose = Pose::Multiply(trackerDelta, Pose::Invert(location->pose));
                }
                */

                // test rotation
                // save current location
                XrVector3f pos = location->pose.position;

                /*
                int64_t milliseconds = (time / 1000000) % 10000;
                float angle = (float)M_PI * 0.0002f * milliseconds;
                */

                float angle = (float)M_PI * 0.002f * (m_Counter++ %1000);
               
                if (spaceIsViewSpace && !baseSpaceIsViewSpace)
                {
                    location->pose.position = {0, 0, 0};
                    StoreXrPose(&location->pose,
                                XMMatrixMultiply(LoadXrPose(location->pose),
                                                 DirectX::XMMatrixRotationRollPitchYaw(-angle, 0.f, 0.f)));
                    location->pose.position = pos;
                }
                if (baseSpaceIsViewSpace && !spaceIsViewSpace)
                {
                    location->pose.position = {0, 0, 0};
                    StoreXrPose(&location->pose,
                                XMMatrixMultiply(LoadXrPose(location->pose),
                                                 DirectX::XMMatrixRotationRollPitchYaw(angle, 0.f, 0.f)));
                    location->pose.position = pos;
                }

                DebugLog("output pose\nx: %f y: %f z: %f\na: %f b: %f, c: %f d: %f\n",
                         location->pose.position.x,
                         location->pose.position.y,
                         location->pose.position.z,
                         location->pose.orientation.w,
                         location->pose.orientation.x,
                         location->pose.orientation.y,
                         location->pose.orientation.z);  
                
            }
            return result;
        }

        XrResult xrLocateViews(XrSession session,
                               const XrViewLocateInfo* viewLocateInfo,
                               XrViewState* viewState,
                               uint32_t viewCapacityInput,
                               uint32_t* viewCountOutput,
                               XrView* views)
        {
            // call xrLocateViews of next layer
            CHECK_XRCMD(OpenXrApi::xrLocateViews(session,
                                                 viewLocateInfo,
                                                 viewState,
                                                 viewCapacityInput,
                                                 viewCountOutput,
                                                 views));
            
            // manipulate reference space location
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
            CHECK_XRCMD(xrLocateSpace(m_ViewSpace, viewLocateInfo->space, viewLocateInfo->displayTime, &location));

            // determine eye offset
            XrViewLocateInfo offsetLocateViewInfo{viewLocateInfo->type,
                                                  nullptr,
                                                  viewLocateInfo->viewConfigurationType,
                                                  viewLocateInfo->displayTime,
                                                  m_ViewSpace};
            uint32_t manipulatedCount;
            std::vector<XrView> m_ManipulatedViews;
            m_ManipulatedViews.resize(*viewCountOutput, {XR_TYPE_VIEW});
            CHECK_XRCMD(OpenXrApi::xrLocateViews(session,
                                                 &offsetLocateViewInfo,
                                                 viewState,
                                                 viewCapacityInput,
                                                 &manipulatedCount,
                                                 m_ManipulatedViews.data()));
            CHECK(manipulatedCount == *viewCountOutput);
            
            XrView* curView = views;
            for (uint32_t i = 0; i < *viewCountOutput; i++)
            {
                XrPosef offsetPose = Pose::Multiply(curView->pose, Pose::Invert(m_ManipulatedViews[i].pose));
                curView->pose = Pose::Multiply(offsetPose, location.pose);
                curView++;
                /*
                DebugLog("offset pose %d:\nx: %f y: %f z: %f\na: %f b: %f, c: %f d: %f\n",
                         i,
                         offsetPose.position.x,
                         offsetPose.position.y,
                         offsetPose.position.z,
                         offsetPose.orientation.w,
                         offsetPose.orientation.x,
                         offsetPose.orientation.y,
                         offsetPose.orientation.z);  
                         */
            }

            return XR_SUCCESS;
        }

        XrSpace m_ViewSpace{XR_NULL_HANDLE};

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

        std::set<XrSpace> m_ViewSpaces{};

        const std::string getXrPath(XrPath path)
        {
            char buf[XR_MAX_PATH_LENGTH];
            uint32_t count;
            CHECK_XRCMD(xrPathToString(GetXrInstance(), path, sizeof(buf), &count, buf));
            std::string str;
            str.assign(buf, count - 1);
            return str;
        }

        OpenXrTracker* m_Tracker = nullptr;

        size_t m_Counter{0};
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
