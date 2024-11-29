// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "utility.h"
#include "sampler.h"
#include "filter.h"
#include "modifier.h"
#include "output.h"

namespace sampler
{
    class Sampler;
}

namespace tracker
{
    class ControllerBase
    {
      public:
        virtual ~ControllerBase() = default;
        virtual bool Init();
        virtual bool GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time);
        virtual bool ResetReferencePose(XrSession session, XrTime time);

      protected:
        virtual void SetReferencePose(const XrPosef& pose);
        virtual bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) = 0;
        virtual bool GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time);
        static XrVector3f GetForwardVector(const XrQuaternionf& quaternion);
        static XrQuaternionf GetLeveledRotation(const XrVector3f& forward, float yawAdjustment);
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
        explicit TrackerBase(const std::vector<utility::DofValue>& relevant);
        ~TrackerBase() override;
        bool Init() override;
        virtual bool LazyInit(XrTime time);
        void ModifyFilterStrength(bool trans, bool increase, bool fast);
        virtual void ToggleStabilizer();
        virtual void ModifyStabilizer(bool increase, bool fast);

        [[nodiscard]] XrPosef GetReferencePose() const;
        void SetReferencePose(const XrPosef& pose) override;
        virtual void InvalidateCalibration(bool silent);
        virtual void SaveReferencePose() const;
        void SaveReferencePoseImpl(const XrPosef& refPose) const;

        virtual bool ChangeOffset(XrVector3f modification);
        virtual bool ChangeRotation(float radian);
        virtual void ApplyCorManipulation(XrSession session, XrTime time){};

        void SetModifierActive(bool active) const;

        void LogCurrentTrackerPoses(XrSession session, XrTime time, bool activated);
        bool ToggleRecording() const;

        virtual utility::DataSource* GetSource() = 0;
        virtual bool ReadSource(XrTime time, utility::Dof& dof) = 0;

        bool m_SkipLazyInit{false};
        bool m_LoadPoseFromFile{false};
        bool m_Calibrated{false};

      protected:
        void ApplyFilters(XrPosef& pose) override;
        void ApplyModifier(XrPosef& pose) override;
        bool LoadReferencePose();
        virtual std::optional<XrPosef> GetForwardView(XrSession session, XrTime time);
        static std::optional<XrPosef> GetCurrentView(XrSession session, XrTime time);
        void SetForwardRotation(const XrPosef& pose) const;
        
        
        std::vector<utility::DofValue> m_RelevantValues{};
        sampler::Sampler* m_Sampler{nullptr};

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
        void SaveReferencePose() const override;

        utility::DataSource* GetSource() override;
        bool ReadSource(XrTime time, utility::Dof& dof) override;

      protected:
        bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;

      private:
        [[nodiscard]] utility::Dof PoseToDof(const XrPosef& pose) const;
        [[nodiscard]] XrPosef DofToPose(const utility::Dof& dof) const;

        class PhysicalSource : public utility::DataSource
        {
          public:
            explicit PhysicalSource(OpenXrTracker* tracker) : m_Tracker(tracker){};
            bool Open(int64_t time) override;

          private:
            OpenXrTracker* m_Tracker{nullptr};
        };

        std::mutex m_SampleMutex;
        XrPosef m_RefToFwd{xr::math::Pose::Identity()};

        std::unique_ptr<PhysicalSource> m_Source;
        XrSession m_Session{XR_NULL_HANDLE};

        friend class PhysicalSource;
    };

    class CorManipulator;

    class VirtualTracker : public TrackerBase
    {
      public:
        explicit VirtualTracker(const std::vector<utility::DofValue>& relevant) : TrackerBase(relevant){};
        bool Init() override;
        bool LazyInit(XrTime time) override;

        bool ResetReferencePose(XrSession session, XrTime time) override;

        void ApplyCorManipulation(XrSession session, XrTime time) override;
        bool ChangeOffset(XrVector3f modification) override;
        bool ChangeRotation(float radian) override;
        void LogOffsetValues() const;

        utility::DataSource* GetSource() override;

      protected:
        void SetReferencePose(const XrPosef& pose) override;
        bool GetPose(XrPosef& trackerPose, XrSession session, XrTime time) override;
        std::optional<XrPosef> GetForwardView(XrSession session, XrTime time) override;
        virtual bool ReadData(XrTime time, utility::Dof& dof);
        virtual XrPosef DataToPose(const utility::Dof& dof) = 0;

        std::string m_Filename;
        utility::Mmf m_Mmf;
        float m_OffsetForward{0.0f}, m_OffsetDown{0.0f}, m_OffsetRight{0.0f}, m_OffsetYaw{0.0f}, m_PitchConstant{0.0f};
        XrQuaternionf m_ConstantPitchQuaternion{};

      private:
        void ApplyOffsets(const utility::Dof& dof, XrPosef& view);

        std::unique_ptr<CorManipulator> m_Manipulator{};
        bool m_NonNeutralCalibration{false};

        friend class Sampler;
    };

    class YawTracker : public VirtualTracker
    {
      public:
        YawTracker() : VirtualTracker({utility::yaw, utility::roll, utility::pitch})
        {
            m_Filename = "Local\\YawVRGEFile";
        }
        bool ReadSource(XrTime now, utility::Dof& dof) override;

      protected:
        XrPosef DataToPose(const utility::Dof& dof) override;

      private:
        struct YawData
        {
            float yaw, pitch, roll;
        };
    };

    class SixDofTracker : public VirtualTracker
    {
      protected:
        SixDofTracker(const std::vector<utility::DofValue>& relevant) : VirtualTracker(relevant)
        {}
        bool ReadSource(XrTime now, utility::Dof& dof) override;
        XrPosef DataToPose(const utility::Dof& dof) override;
        virtual void ExtractRotationQuaternion(const utility::Dof& dof, XrPosef& pose) = 0;
        static void ExtractTranslationVector(const utility::Dof& dof, XrPosef& rigPose);

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
            : SixDofTracker(
                  {utility::sway, utility::surge, utility::heave, utility::yaw, utility::roll, utility::pitch})
        {
            m_Filename = "Local\\motionRigPose";
        }

      protected:
        void ExtractRotationQuaternion(const utility::Dof& dof, XrPosef& rigPose) override;
    };

    class SrsTracker final : public SixDofTracker
    {
      public:
        SrsTracker() : SixDofTracker({utility::yaw, utility::roll, utility::pitch})
        {
            m_Filename = "Local\\SimRacingStudioMotionRigPose";
        }

      protected:
        void ExtractRotationQuaternion(const utility::Dof& dof, XrPosef& rigPose) override;
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
        std::string path;
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