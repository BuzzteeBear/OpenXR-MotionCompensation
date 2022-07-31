// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "pch.h"
#include "utility.h"
#include "filter.h"

class TrackerBase
{
  public:
    virtual ~TrackerBase();

    virtual bool Init();
    virtual bool LazyInit(XrTime time);
    void ModifyFilterStrength(bool trans, bool increase);
    virtual bool ResetReferencePose(XrSession session, XrTime time) = 0;
    bool GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time);
    bool m_SkipLazyInit{false};
    bool m_Calibrated{false};
    bool m_ResetReferencePose{false};

  protected:
    void SetReferencePose(XrPosef pose);
    virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) = 0;
    virtual bool GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time);

    XrPosef m_ReferencePose{xr::math::Pose::Identity()};

  private:
    bool LoadFilters();

    XrPosef m_LastPoseDelta{xr::math::Pose::Identity()};
    XrTime m_LastPoseTime{0};
    float m_TransStrength{0.0f};
    float m_RotStrength{0.0f};
    Filter::FilterBase<XrVector3f>* m_TransFilter = nullptr;
    Filter::FilterBase<XrQuaternionf>* m_RotFilter = nullptr;
};


class OpenXrTracker : public TrackerBase
{
  public:
    OpenXrTracker();
    ~OpenXrTracker();
    
    virtual bool ResetReferencePose(XrSession session, XrTime time) override;

  protected:
    virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;
};

class YawTracker : public TrackerBase
{
  public:
    YawTracker();
    ~YawTracker();
    virtual bool Init() override;
    virtual bool LazyInit(XrTime time) override;
    virtual bool ResetReferencePose(XrSession session, XrTime time) override;
    bool ToggleDebugMode(XrSession session, XrTime time);

  protected:
    virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;

  private:
    struct YawData
    {
        float yaw, pitch, roll, battery, rotationHeight, rotationForwardHead;
        bool sixDof, usePos;
        float autoX, autoY;
    };

    float m_OffsetForward{0.0f}, m_OffsetDown{0.0f}, m_OffsetRight{0.0f};
    utility::Mmf m_Mmf;
    bool m_DebugMode{false};
    XrPosef m_OriginalRefPose{xr::math::Pose::Identity()};
};

void GetTracker(TrackerBase** tracker);