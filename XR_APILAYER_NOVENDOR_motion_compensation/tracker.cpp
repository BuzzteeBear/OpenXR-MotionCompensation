// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "layer.h"
#include <log.h>
#include <util.h>

using namespace motion_compensation_layer;
using namespace motion_compensation_layer::log;
using namespace xr::math;

TrackerBase::~TrackerBase()
{
    if (m_TransFilter)
    {
        delete m_TransFilter;
    }
    if (m_RotFilter)
    {
        delete m_RotFilter;
    }
}

bool TrackerBase::LoadFilters()
{
    // set up filters
    int orderTrans = 2, orderRot = 2;
    float strengthTrans = 0.0f, strengthRot = 0.0f;
    if (!GetConfig()->GetInt(Cfg::TransOrder, orderTrans) || !GetConfig()->GetInt(Cfg::RotOrder, orderRot) ||
        !GetConfig()->GetFloat(Cfg::TransStrength, strengthTrans) ||
        !GetConfig()->GetFloat(Cfg::RotStrength, strengthRot))
    {
        ErrorLog("%s: error reading configured values for filters\n", __FUNCTION__);
    }
    if (1 > orderTrans || 3 < orderTrans)
    {
        ErrorLog("%s: invalid order for translational filter: %d\n", __FUNCTION__, orderTrans);
        return false;
    }
    if (1 > orderRot || 3 < orderRot)
    {
        ErrorLog("%s: invalid order for rotational filter: %d\n", __FUNCTION__, orderRot);
        return false;
    }
    // remove previous filter objects
    if (m_TransFilter)
    {
        delete m_TransFilter;
    }
    if (m_RotFilter)
    {
        delete m_RotFilter;
    }

    m_TransStrength = strengthTrans;
    m_RotStrength = strengthRot;

    Log("translational filter stages: %d\n", orderTrans);
    Log("translational filter strength: %d\n", m_TransStrength);
    m_TransFilter = 1 == orderTrans   ? new utility::SingleEmaFilter(m_TransStrength)
                    : 2 == orderTrans ? new utility::DoubleEmaFilter(m_TransStrength)
                                      : new utility::TripleEmaFilter(m_TransStrength);

    Log("rotational filter stages: %d\n", orderRot);
    Log("rotational filter strength: %d\n", m_TransStrength);
    m_RotFilter = 1 == orderRot   ? new utility::SingleSlerpFilter(m_RotStrength)
                  : 2 == orderRot ? new utility::DoubleSlerpFilter(m_RotStrength)
                                  : new utility::TripleSlerpFilter(m_RotStrength);

    return true;
}

void TrackerBase::ModifyFilterStrength(bool trans, bool increase)
{
    float* currentValue = trans ? &m_TransStrength : &m_RotStrength;
    float prevValue = *currentValue;
    float amount = (1.1f - *currentValue) * 0.05f;
    float newValue = *currentValue + (increase ? amount : -amount);
    if (trans)
    {
        *currentValue = m_TransFilter->SetStrength(newValue);
        GetConfig()->SetValue(Cfg::TransStrength, *currentValue);
        Log("translational filter strength %screased to %f\n", increase ? "in" : "de", *currentValue);
    }
    else
    {
        *currentValue = m_RotFilter->SetStrength(newValue);
        GetConfig()->SetValue(Cfg::RotStrength, *currentValue);
        Log("rotational filter strength %screased to %f\n", increase ? "in" : "de", *currentValue);
    }
    MessageBeep(*currentValue != prevValue ? MB_OK : MB_ICONERROR);
}

void TrackerBase::SetReferencePose(XrPosef pose)
{
    m_TransFilter->Reset(pose.position);
    m_RotFilter->Reset(pose.orientation);
    m_ReferencePose = pose;
    m_Calibrated = true;
}

bool TrackerBase::GetPoseDelta(XrPosef& poseDelta, XrTime frameTime)
{
    // pose already calulated for requested time or unable to calculate
    if (frameTime == m_LastPoseTime)
    {
        // already calulated for requested time;
        poseDelta = m_LastPoseDelta;
        TraceLoggingWrite(g_traceProvider, "GetPoseDelta", TLArg(xr::ToString(m_LastPoseDelta).c_str(), "Last_Delta"));
        return true;
    }
    if (m_ResetReferencePose)
    {
        m_ResetReferencePose = !ResetReferencePose(frameTime);
    }
    XrPosef curPose;
    if (GetPose(curPose, frameTime))
    {
        // apply translational filter
        m_TransFilter->Filter(curPose.position);

        // apply rotational filter
        m_RotFilter->Filter(curPose.orientation);

        TraceLoggingWrite(g_traceProvider,
                          "GetPoseDelta",
                          TLArg(xr::ToString(curPose).c_str(), "Location_After_Filter"),
                          TLArg(frameTime, "Time"));

        // calculate difference toward reference pose
        poseDelta = Pose::Multiply(Pose::Invert(curPose), m_ReferencePose);

        TraceLoggingWrite(g_traceProvider, "GetPoseDelta", TLArg(xr::ToString(poseDelta).c_str(), "Delta"));

        m_LastPoseTime = frameTime;
        m_LastPoseDelta = poseDelta;
        return true;
    }
    else
    {
        return false;
    }
}

OpenXrTracker::OpenXrTracker()
{}

OpenXrTracker::~OpenXrTracker()
{}

bool OpenXrTracker::Init()
{
    return LoadFilters();
}

bool OpenXrTracker::LazyInit()
{
    bool success = true;
    if (!m_SkipLazyInit)
    {
        Log("action set attached during lazy init\n");
        std::vector<XrActionSet> actionSets;
        XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
                                                          nullptr,
                                                          0,
                                                          actionSets.data()};
        TraceLoggingWrite(g_traceProvider, "OpenXrTracker::LazyInit", TLPArg("Executed", "xrAttachSessionActionSets"));
        if (!XR_SUCCEEDED(GetInstance()->xrAttachSessionActionSets(m_Session, &actionSetAttachInfo)))
        {
            ErrorLog("%s: xrAttachSessionActionSets failed\n", __FUNCTION__);
            success = false;
        }
        else
        {
            m_SkipLazyInit = true;
        }
    }
    return success;
}

void OpenXrTracker::beginSession(XrSession session)
{
    m_Session = session;
}

void OpenXrTracker::endSession()
{
    m_Session = XR_NULL_HANDLE;
}

bool OpenXrTracker::ResetReferencePose(XrTime frameTime)
{
    XrPosef curPose;
    if (GetPose(curPose, frameTime))
    {
        SetReferencePose(curPose);
        return true;
    }
    else
    {
        ErrorLog("%s: unable to get current pose\n", __FUNCTION__);
        m_Calibrated = false;
        return false;
    }
}

bool OpenXrTracker::GetPose(XrPosef& trackerPose, XrTime frameTime)
{
    OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
    if (layer)
    {
        // Query the latest tracker pose.
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        {
            // TODO: skip xrSyncActions if other actions are active as well?
            XrActiveActionSet activeActionSets;
            activeActionSets.actionSet = layer->m_ActionSet;
            activeActionSets.subactionPath = XR_NULL_PATH;

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr};
            syncInfo.activeActionSets = &activeActionSets;
            syncInfo.countActiveActionSets = 1;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_ActionSet, "xrSyncActions"),
                              TLArg(frameTime, "Time"));
            CHECK_XRCMD(GetInstance()->xrSyncActions(m_Session, &syncInfo));
        }
        {
            XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
            XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
            getActionStateInfo.action = layer->m_TrackerPoseAction;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_TrackerPoseAction, "xrGetActionStatePose"),
                              TLArg(frameTime, "Time"));
            CHECK_XRCMD(GetInstance()->xrGetActionStatePose(m_Session, &getActionStateInfo, &actionStatePose));

            if (!actionStatePose.isActive)
            {
                ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active\n", __FUNCTION__);
                return false;
            }
        }

        CHECK_XRCMD(GetInstance()->xrLocateSpace(layer->m_TrackerSpace, layer->m_ReferenceSpace, frameTime, &location));

        if (!Pose::IsPoseValid(location.locationFlags))
        {
            ErrorLog("%s: unable to determine tracker pose - XrSpaceLocation not valid\n", __FUNCTION__);
            return false;
        }
        TraceLoggingWrite(g_traceProvider,
                          "GetPose",
                          TLArg(xr::ToString(location.pose).c_str(), "Location"),
                          TLArg(frameTime, "Time"));
        trackerPose = location.pose;

        return true;
    }
    else
    {
        ErrorLog("unable to cast instance to OpenXrLayer\n");
        return false;
    }
}

YawTracker::YawTracker()
{}

YawTracker::~YawTracker()
{}

bool YawTracker::Init()
{
    return LoadFilters();
}

bool YawTracker::LazyInit()
{
    bool success = true;
    if (!m_SkipLazyInit)
    {
        Log("action set attached during lazy init\n");
        std::vector<XrActionSet> actionSets;
        XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
                                                          nullptr,
                                                          0,
                                                          actionSets.data()};
        TraceLoggingWrite(g_traceProvider, "YawTracker::LazyInit", TLPArg("Executed", "xrAttachSessionActionSets"));
        if (!XR_SUCCEEDED(GetInstance()->xrAttachSessionActionSets(m_Session, &actionSetAttachInfo)))
        {
            ErrorLog("%s: xrAttachSessionActionSets failed\n", __FUNCTION__);
            success = false;
        }
        else
        {
            m_SkipLazyInit = true;
        }
    }
    return success;
}

void YawTracker::beginSession(XrSession session)
{
    m_Session = session;
}

void YawTracker::endSession()
{
    m_Session = XR_NULL_HANDLE;
}

bool YawTracker::ResetReferencePose(XrTime frameTime)
{
    XrPosef curPose;
    if (GetPose(curPose, frameTime))
    {
        SetReferencePose(curPose);
        return true;
    }
    else
    {
        ErrorLog("%s: unable to get current pose\n", __FUNCTION__);
        m_Calibrated = false;
        return false;
    }
}

bool YawTracker::GetPose(XrPosef& trackerPose, XrTime frameTime)
{
    OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
    if (layer)
    {
        // Query the latest tracker pose.
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        {
            // TODO: skip xrSyncActions if other actions are active as well?
            XrActiveActionSet activeActionSets;
            activeActionSets.actionSet = layer->m_ActionSet;
            activeActionSets.subactionPath = XR_NULL_PATH;

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr};
            syncInfo.activeActionSets = &activeActionSets;
            syncInfo.countActiveActionSets = 1;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_ActionSet, "xrSyncActions"),
                              TLArg(frameTime, "Time"));
            CHECK_XRCMD(GetInstance()->xrSyncActions(m_Session, &syncInfo));
        }
        {
            XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
            XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
            getActionStateInfo.action = layer->m_TrackerPoseAction;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_TrackerPoseAction, "xrGetActionStatePose"),
                              TLArg(frameTime, "Time"));
            CHECK_XRCMD(GetInstance()->xrGetActionStatePose(m_Session, &getActionStateInfo, &actionStatePose));

            if (!actionStatePose.isActive)
            {
                ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active\n", __FUNCTION__);
                return false;
            }
        }

        CHECK_XRCMD(GetInstance()->xrLocateSpace(layer->m_TrackerSpace, layer->m_ReferenceSpace, frameTime, &location));

        if (!Pose::IsPoseValid(location.locationFlags))
        {
            ErrorLog("%s: unable to determine tracker pose - XrSpaceLocation not valid\n", __FUNCTION__);
            return false;
        }
        TraceLoggingWrite(g_traceProvider,
                          "GetPose",
                          TLArg(xr::ToString(location.pose).c_str(), "Location"),
                          TLArg(frameTime, "Time"));
        trackerPose = location.pose;

        return true;
    }
    else
    {
        ErrorLog("unable to cast instance to OpenXrLayer\n");
        return false;
    }
}

void GetTracker(TrackerBase** tracker)
{
    TrackerBase* previousTracker = *tracker;
    std::string trackerType;
    if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
    {
        if ("yaw" == trackerType)
        {
            Log("using yaw mapped memory file as tracker\n");
            if (previousTracker)
            {
                delete previousTracker;
            }
            *tracker = new YawTracker();
        }
        else if ("controller" == trackerType)
        {
            Log("using motion cotroller as tracker\n");
            if (previousTracker)
            {
                delete previousTracker;
            }
            *tracker = new OpenXrTracker();
            return;
        }
        else
        {
            ErrorLog("unknown tracker type: %s\n", trackerType.c_str());
        }
    }
    else
    {
        ErrorLog("unable to determine tracker type, defaulting to 'controller'\n");
    }
    if (previousTracker)
    {
        ErrorLog("retaining previous tracker type\n");
        return;
    }
    ErrorLog("defaulting to 'controller'\n");
    *tracker = new OpenXrTracker();
}