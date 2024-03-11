// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "utility.h"
#include "sampler.h"
#include "filter.h"
#include "modifier.h"
#include "output.h"

namespace tracker
{
    class ControllerBase
    {
      public:
        virtual ~ControllerBase() = default;
        virtual bool Init();
        virtual bool GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time);
        virtual bool ResetReferencePose(XrSession session, XrTime time);

        bool m_XrSyncCalled{false};

      protected:
        virtual void SetReferencePose(const XrPosef& pose);
        virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) = 0;
        virtual bool GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time);
        static XrVector3f GetForwardVector(const XrQuaternionf& quaternion, bool inverted = false);
        static XrQuaternionf GetYawRotation(const XrVector3f& forward, float yawAdjustment);
        static float GetYawAngle(const XrVector3f& forward);

        XrPosef m_ReferencePose{xr::math::Pose::Identity()};
        XrPosef m_LastPose{xr::math::Pose::Identity()};
        XrPosef m_LastPoseDelta{xr::math::Pose::Identity()};
        XrTime m_LastPoseTime{0};
        bool m_FallBackUsed{false};
        bool m_ConnectionLost{false};
        std::shared_ptr<output::RecorderBase> m_Recorder{std::make_shared<output::NoRecorder>()};
        
      private:
        virtual void ApplyFilters(XrPosef& trackerPose){};
        virtual void ApplyModifier(XrPosef& trackerPose){};

        bool m_PhysicalEnabled{false};
    };
   
    class TrackerBase : public ControllerBase
    {
      public:
        TrackerBase()
        {
            
        }
        ~TrackerBase() override;
        bool Init() override;
        virtual bool LazyInit(XrTime time);
        void ModifyFilterStrength(bool trans, bool increase, bool fast);
        virtual void ToggleStabilizer();
        virtual void ModifyStabilizer(bool increase, bool fast);
        [[nodiscard]] XrPosef GetReferencePose() const;
        void SetReferencePose(const XrPosef& pose) override;
        virtual void InvalidateCalibration();
        void SetModifierActive(const bool active) const;
        void LogCurrentTrackerPoses(XrSession session, XrTime time, bool activated);
        virtual bool ChangeOffset(XrVector3f modification);
        virtual bool ChangeRotation(float radian);
        virtual void SaveReferencePose(XrTime time) const{};
        virtual void ApplyCorManipulation(XrSession session, XrTime time){};
        bool ToggleRecording() const;

        bool m_SkipLazyInit{false};
        bool m_Calibrated{false};
       
      protected:
        void ApplyFilters(XrPosef& pose) override;
        void ApplyModifier(XrPosef& pose) override;
        bool CalibrateForward(XrSession session, XrTime time, float yawOffset);
        void SetForwardRotation(const XrPosef& pose) const;

        XrVector3f m_Forward{0.f, 0.f, 1.f};
        XrVector3f m_Right{-1.f, 0.f, 0.f};
        XrVector3f m_Up{0.f, 1.f, 0.f};
        XrPosef m_ForwardPose{xr::math::Pose::Identity()};

      private:
        bool LoadFilters();

        float m_TransStrength{0.0f};
        float m_RotStrength{0.0f};
        filter::FilterBase<XrVector3f>* m_TransFilter = nullptr;
        filter::FilterBase<XrQuaternionf>* m_RotFilter = nullptr;

        std::shared_ptr<modifier::TrackerModifier> m_TrackerModifier{};
    };

    class OpenXrTracker final : public TrackerBase
    {
      public:
        OpenXrTracker();
        bool ResetReferencePose(XrSession session, XrTime time) override;

      protected:
        bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;
    };

    class CorManipulator;

    class VirtualTracker : public TrackerBase
    {
      public:
        VirtualTracker();
        bool Init() override;
        bool LazyInit(XrTime time) override;
        bool ResetReferencePose(XrSession session, XrTime time) override;
        bool ChangeOffset(XrVector3f modification) override;
        bool ChangeRotation(float radian) override;
        void SaveReferencePose(XrTime time) const override;
        void LogOffsetValues() const;
        void ApplyCorManipulation(XrSession session, XrTime time) override;

      protected:
        void SetReferencePose(const XrPosef& pose) override;
        bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;
        virtual bool ReadData(XrTime time, utility::Dof& dof) = 0;
        virtual XrPosef DataToPose(const utility::Dof& dof) = 0;

        Sampler* m_Sampler{nullptr};
        std::string m_Filename;
        utility::Mmf m_Mmf;
        float m_OffsetForward{0.0f}, m_OffsetDown{0.0f}, m_OffsetRight{0.0f}, m_OffsetYaw{0.0f}, m_PitchConstant{0.0f};

      private:
        bool LoadReferencePose(XrSession session, XrTime time);

        std::unique_ptr<CorManipulator> m_Manipulator{};
        bool m_LoadPoseFromFile{false};
    };

    class YawTracker : public VirtualTracker
    {
      public:
        YawTracker();
        ~YawTracker() override;
        bool ResetReferencePose(XrSession session, XrTime time) override;
        void InvalidateCalibration() override;
        void ToggleStabilizer() override;
        void ModifyStabilizer(bool increase, bool fast) override;
        static bool ReadMmf(utility::Dof& dof, XrTime now, utility::DataSource* source);

      protected:
        bool ReadData(XrTime time, utility::Dof& dof) override;
        XrPosef DataToPose(const utility::Dof& dof) override;

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
        bool ReadData(XrTime time, utility::Dof& dof) override;

      private:
        struct SixDof
        {
            double sway;
            double surge;
            double heave;
            double yaw;
            double roll;
            double pitch;
        };
    };

    class FlyPtTracker final : public SixDofTracker
    {
      public:
        FlyPtTracker()
        {
            m_Filename = "Local\\motionRigPose";
        }

      protected:
        XrPosef DataToPose(const utility::Dof& dof) override;
    };

    class SrsTracker final : public SixDofTracker
    {
      public:
        SrsTracker()
        {
            m_Filename = "Local\\SimRacingStudioMotionRigPose";
        }

      protected:
        XrPosef DataToPose(const utility::Dof& dof) override;
    };

    class CorManipulator : public ControllerBase
    {
      public:
        explicit CorManipulator(VirtualTracker* tracker) : m_Tracker(tracker){};
        void ApplyManipulation(XrSession session, XrTime time);

      protected:
        bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;

      private:
        void GetButtonState(XrSession session, bool& moveButton, bool& positionButton);
        void ApplyPosition() const;
        void ApplyRotation(const XrPosef& poseDelta) const;
        void ApplyTranslation() const;

        VirtualTracker* m_Tracker{nullptr};
        bool m_Initialized{true};
        bool m_MoveActive{false};
        bool m_PositionActive{false};
    };
    
    struct ViveTrackerInfo
    {
        bool Init();

        bool active{false};
        std::string role;
        inline static std::set<std::string> validRoles{"left_foot",
                                                       "left_shoulder",
                                                       "left_elbow",
                                                       "left_knee",
                                                       "right_foot",
                                                       "right_shoulder",
                                                       "right_elbow",
                                                       "right_knee",
                                                       "waist",
                                                       "chest",
                                                       "camera",
                                                       "keyboard"};
    };

    std::unique_ptr<tracker::TrackerBase> GetTracker();
} // namespace tracker