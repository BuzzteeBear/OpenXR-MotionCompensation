// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "pch.h"
#include "utility.h"

class TrackerBase
{
  public:
    virtual ~TrackerBase();

    virtual bool Init() = 0;
    virtual void beginSession(XrSession session) = 0;
    virtual bool LazyInit() = 0;
    bool LoadFilters();
    void ModifyFilterStrength(bool trans, bool increase);
    virtual bool ResetReferencePose(XrTime frameTime) = 0;
    bool GetPoseDelta(XrPosef& poseDelta, XrTime frameTime);
    virtual void endSession() = 0;

    bool m_SkipLazyInit{false};
    bool m_Calibrated{false};
    bool m_ResetReferencePose{false};

  protected:
    void SetReferencePose(XrPosef pose);
    virtual bool GetPose(XrPosef& trackerPose, XrTime frameTime) = 0;
   
    XrSession m_Session{XR_NULL_HANDLE};

  private:
    XrPosef m_ReferencePose{xr::math::Pose::Identity()};
    XrPosef m_LastPoseDelta{xr::math::Pose::Identity()};
    XrTime m_LastPoseTime{0};
    float m_TransStrength{0.0f};
    float m_RotStrength{0.0f};
    utility::FilterBase<XrVector3f>* m_TransFilter = nullptr;
    utility::FilterBase<XrQuaternionf>* m_RotFilter = nullptr;
};


class OpenXrTracker : public TrackerBase
{
  public:
    OpenXrTracker();
    ~OpenXrTracker();
    virtual bool Init() override;
    virtual bool LazyInit() override;
    virtual void beginSession(XrSession session) override;
    virtual void endSession() override;
    virtual bool ResetReferencePose(XrTime frameTime) override;

    XrActionSet m_ActionSet{XR_NULL_HANDLE};
    XrAction m_TrackerPoseAction{XR_NULL_HANDLE};
    XrSpace m_TrackerSpace{XR_NULL_HANDLE};
    XrSpace m_ReferenceSpace{XR_NULL_HANDLE};

  protected:
    virtual bool GetPose(XrPosef& trackerPose, XrTime frameTime) override;
};

void GetTracker(TrackerBase** tracker);