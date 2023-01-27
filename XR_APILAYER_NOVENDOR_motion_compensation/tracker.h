// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "utility.h"
#include "filter.h"

namespace Tracker
{
    class TrackerBase
    {
      public:
        virtual ~TrackerBase();

        virtual bool Init();
        virtual bool LazyInit(XrTime time);
        void ModifyFilterStrength(bool trans, bool increase);
        virtual bool ResetReferencePose(XrSession session, XrTime time) = 0;
        void AdjustReferencePose(const XrPosef& pose);
        XrPosef GetReferencePose(XrSession session, XrTime time) const;
        bool GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time);
        void LogCurrentTrackerPoses(XrSession session, XrTime time, bool activated);
        bool m_SkipLazyInit{false};
        bool m_Calibrated{false};
        bool m_ResetReferencePose{false};
       
        XrPosef m_LastPoseDelta{xr::math::Pose::Identity()};

      protected:
        void SetReferencePose(const XrPosef& pose);
        virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) = 0;
        virtual bool GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time);

        XrPosef m_ReferencePose{xr::math::Pose::Identity()};


      private:
        bool LoadFilters();

        bool m_ConnectionLost{false};
        bool m_PhysicalEnabled{false};
        XrTime m_LastPoseTime{0};
        float m_TransStrength{0.0f};
        float m_RotStrength{0.0f};
        Filter::FilterBase<XrVector3f>* m_TransFilter = nullptr;
        Filter::FilterBase<XrQuaternionf>* m_RotFilter = nullptr;
    };

    class OpenXrTracker : public TrackerBase
    {
      public:
        virtual bool ResetReferencePose(XrSession session, XrTime time) override;

      protected:
        virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;
    };

    class VirtualTracker : public TrackerBase
    {
      public:
        virtual bool Init() override;
        virtual bool LazyInit(XrTime time) override;
        virtual bool ResetReferencePose(XrSession session, XrTime time) override;
        bool ChangeOffset(XrVector3f modification);
        bool ChangeRotation(bool right);
        void SaveReferencePose(XrTime time) const;
        bool ToggleDebugMode(XrSession session, XrTime time);

      protected:
        virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;
        virtual bool GetVirtualPose(XrPosef& trackerPose, XrSession session, XrTime time) = 0;

        std::string m_Filename;
        utility::Mmf m_Mmf;
        float m_OffsetForward{0.0f}, m_OffsetDown{0.0f}, m_OffsetRight{0.0f};
        bool m_UpsideDown{false};
        

      private:
        bool LoadReferencePose(XrSession session, XrTime time);

        bool m_DebugMode{false}, m_LoadPoseFromFile{false};
        XrPosef m_OriginalRefPose{xr::math::Pose::Identity()};
    };

    class YawTracker : public VirtualTracker
    {
      public:
      public:
        YawTracker()
        {
            m_Filename = "Local\\YawVRGEFile";
        }
        virtual bool ResetReferencePose(XrSession session, XrTime time) override;

      protected:
        virtual bool GetVirtualPose(XrPosef& trackerPose, XrSession session, XrTime time) override;

      private:
        struct YawData
        {
            float yaw, pitch, roll, battery, rotationHeight, rotationForwardHead;
            bool sixDof, usePos;
            float autoX, autoY;
        };
    };

    class SixDofTracker : public VirtualTracker
    {
      protected:
        virtual bool GetVirtualPose(XrPosef& trackerPose, XrSession session, XrTime time) override;
       
        bool m_IsSrs{false};

      private:
        struct SixDofData
        {
            double sway;
            double surge;
            double heave;
            double yaw;
            double roll;
            double pitch;
        };
    };

    class FlyPtTracker : public SixDofTracker
    {
      public:
        FlyPtTracker()
        {
            m_Filename = "Local\\motionRigPose";
            m_IsSrs = false;
        }
    };

    class SrsTracker : public SixDofTracker
    {
      public:
        SrsTracker()
        {
            m_Filename = "Local\\SimRacingStudioMotionRigPose";
            m_IsSrs = true;
        }
    };
    
    struct ViveTrackerInfo
    {
        bool Init();
        bool active{false};
        std::string role;
        const std::string profile{"/interaction_profiles/htc/vive_tracker_htcx"};
    };

    constexpr float angleToRadian{static_cast<float>(M_PI) / 180.0f};

    void GetTracker(TrackerBase** tracker);
} // namespace Tracker