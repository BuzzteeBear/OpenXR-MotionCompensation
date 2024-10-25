// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "resource.h"

// include definitions shared with c#
typedef int Int32;
typedef int64_t Int64;
#define public
#define record
#define enum enum class
#include "output.cs"
#undef public
#undef enum
#undef record  

namespace output
{
    class EventSink
    {
      public:
        static void Execute(Event event, bool silent = false);
    };

    class AudioOut
    {
      public:
        static void Execute(Event event);
        static void CountDown(int seconds);

      private:
        inline static const std::map<Event, int> m_SoundResources{{Event::Silence, 0},
                                                                  {Event::Error, ERROR_WAV},
                                                                  {Event::Critical, ERROR_WAV},
                                                                  {Event::Load, LOADED_WAV},
                                                                  {Event::Save, SAVED_WAV},
                                                                  {Event::Activated, ACTIVATED_WAV},
                                                                  {Event::Deactivated, DEACTIVATED_WAV},
                                                                  {Event::Calibrated, CALIBRATED_WAV},
                                                                  {Event::Plus, PLUS_WAV},
                                                                  {Event::Minus, MINUS_WAV},
                                                                  {Event::Max, MAX_WAV},
                                                                  {Event::Min, MIN_WAV},
                                                                  {Event::Up, UP_WAV},
                                                                  {Event::Down, DOWN_WAV},
                                                                  {Event::Forward, FORWARD_WAV},
                                                                  {Event::Back, BACK_WAV},
                                                                  {Event::Left, LEFT_WAV},
                                                                  {Event::Right, RIGHT_WAV},
                                                                  {Event::RotLeft, ROT_LEFT_WAV},
                                                                  {Event::RotRight, ROT_RIGHT_WAV},
                                                                  {Event::DebugOn, DEBUG_ON_WAV},
                                                                  {Event::DebugOff, DEBUG_OFF_WAV},
                                                                  {Event::ConnectionLost, CONNECTION_LOST_WAV},
                                                                  {Event::EyeCached, EYE_CACHED_WAV},
                                                                  {Event::EyeCalculated, EYE_CALCULATION_WAV},
                                                                  {Event::OverlayOn, OVERLAY_ON_WAV},
                                                                  {Event::OverlayOff, OVERLAY_OFF_WAV},
                                                                  {Event::ModifierOn, MODIFIER_ON_WAV},
                                                                  {Event::ModifierOff, MODIFIER_OFF_WAV},
                                                                  {Event::CalibrationLost, CALIBRATION_LOST_WAV},
                                                                  {Event::VerboseOn, VERBOSE_ON_WAV},
                                                                  {Event::VerboseOff, VERBOSE_OFF_WAV},
                                                                  {Event::RecorderOn, RECORDER_ON_WAV},
                                                                  {Event::RecorderOff, RECORDER_OFF_WAV},
                                                                  {Event::StabilizerOn, STABILIZER_ON_WAV},
                                                                  {Event::StabilizerOff, STABILIZER_OFF_WAV},
                                                                  {Event::PassthroughOn, PASSTHROUGH_ON_WAV},
                                                                  {Event::PassthroughOff, PASSTHROUGH_OFF_WAV}};
    };

    class EventMmf
    {
      public:
        EventMmf();
        ~EventMmf();

        void Execute(Event event);
      private:
        void UpdateEventMmf();
        void StopThread();

        std::set<Event> m_RelevantEvents{
            Event::Error,         Event::Critical,        Event::Initialized,    Event::Load,
            Event::Save,          Event::Activated,       Event::Deactivated,    Event::Calibrated,
            Event::DebugOn,       Event::DebugOff,        Event::ConnectionLost, Event::EyeCached,
            Event::EyeCalculated, Event::OverlayOn,       Event::OverlayOff,     Event::ModifierOn,
            Event::ModifierOff,   Event::CalibrationLost, Event::VerboseOn,      Event::VerboseOff,
            Event::RecorderOn,    Event::RecorderOff,     Event::StabilizerOn,   Event::StabilizerOff,
            Event::PassthroughOn, Event::PassthroughOff};
        std::thread* m_Thread{nullptr};
        std::atomic_bool m_StopThread{false}, m_MmfError{false};
        int64_t m_LastError{0};
        std::deque <std::pair<Event, int64_t>> m_EventQueue;
        std::mutex m_QueueMutex;
    };

    // Singleton accessor.
    EventMmf* GetEventMmf();    

   

    class StatusMmf
    {
      public:
        StatusMmf();
        ~StatusMmf() = default;

        void Execute(Event event);

    private:
        static int StatusToInt(const Status& status);

        std::set<Event> m_RelevantEvents{Event::Error,
                                         Event::Critical,
                                         Event::Initialized,
                                         Event::Load,
                                         Event::Save,
                                         Event::Activated,
                                         Event::Deactivated,
                                         Event::Calibrated,
                                         Event::ConnectionLost,
                                         Event::ModifierOff,
                                         Event::CalibrationLost,
                                         Event::Plus,
                                         Event::Minus,
                                         Event::Max,
                                         Event::Min,
                                         Event::Up,
                                         Event::Down,
                                         Event::Forward,
                                         Event::Back,
                                         Event::Left,
                                         Event::Right,
                                         Event::RotLeft,
                                         Event::RotRight,
                                         Event::EyeCached,
                                         Event::EyeCalculated,
                                         Event::ModifierOn,
                                         Event::ModifierOff,
                                         Event::VerboseOn,
                                         Event::VerboseOff,
                                         Event::StabilizerOn,
                                         Event::StabilizerOff};
        utility::Mmf m_Mmf{};
        Status m_Status{};
    };

    // Singleton accessor.
    StatusMmf* GetStatusMmf();




    constexpr uint32_t m_RecorderMax{36000}; // max 10 min @ 100 frames/s

    enum RecorderDofInput
    {
        Sampled = 0,
        Read,
        Momentary
    };

    enum RecorderPoseInput
    {
        Unfiltered = 0,
        Filtered,
        Modified,
        Reference
    };

    struct DofSample
    {
        utility::Dof sampled{};
        utility::Dof read{};
        utility::Dof momentary{};
    };

    class RecorderBase
    {
      public:
        RecorderBase() = default;
        virtual ~RecorderBase() = default;
        virtual void SetFwdToStage(const XrPosef& pose) = 0;
        virtual bool Toggle(bool isCalibrated) = 0;
        virtual void AddFrameTime(XrTime time) = 0;
        virtual void AddPose(const XrPosef& pose, RecorderPoseInput type) = 0;
        virtual void AddDofValues(const utility::Dof& dofValues, RecorderDofInput type) = 0;
        virtual void Write(bool sampled = false, bool newLine = true) = 0;

        std::atomic_bool m_Sampling{false};
    };

    class NoRecorder final : public RecorderBase
    {
      public:
        bool Toggle(bool isCalibrated) override;
        void SetFwdToStage(const XrPosef& pose) override{};
        void AddFrameTime(XrTime time) override{};
        void AddPose(const XrPosef& pose, RecorderPoseInput type) override{};
        void AddDofValues(const utility::Dof& dofValues, RecorderDofInput type) override{};
        void Write(bool sampled, bool newLine) override{};
    };

    class PoseRecorder : public RecorderBase
    {
      public:
        PoseRecorder();
        ~PoseRecorder() override;
        void SetFwdToStage(const XrPosef& pose) override;
        bool Toggle(bool isCalibrated) override;
        void AddFrameTime(XrTime time) override;
        void AddPose(const XrPosef& pose, RecorderPoseInput type) override;
        void AddDofValues(const utility::Dof& dofValues, RecorderDofInput type) override{};
        void Write(bool sampled, bool newLine) override;

      protected:
        std::atomic_bool m_Started{false}, m_PoseRecorded{false};
        bool m_RecordSamples{false};

        std::ofstream m_FileStream;
        XrTime m_FrameTime{};
        std::string m_HeadLine{"Elapsed (ms); Time; FrameTime; "
                               "Sway_Unfiltered; Sway_Filtered; Sway_Modified;"
                               "Surge_Unfiltered; Surge_Filtered; Surge_Modified;"
                               "Heave_Unfiltered; Heave_Filtered; Heave_Modified;"
                               "Yaw_Unfiltered; Yaw_Filtered; Yaw_Modified;"
                               "Roll_Unfiltered; Roll_Filtered; Roll_Modified;"
                               "Pitch_Unfiltered; Pitch_Filtered; Pitch_Modified"};
        std::mutex m_RecorderMutex;
        int64_t m_StartTime{0};

      private:
        virtual bool Start();
        virtual void Stop();

        XrPosef m_StageToFwd{xr::math::Pose::Identity()};
        std::pair<XrVector3f, XrVector3f> m_Poses[3]{};
        XrPosef m_Ref{xr::math::Pose::Identity()}, m_InvertedRef{xr::math::Pose::Identity()};
        uint32_t m_Counter{0};
    };

    class PoseAndDofRecorder final : public PoseRecorder
    {
      public:
        PoseAndDofRecorder()
        {
            m_HeadLine += "; Sway_Sampled; Sway_Read; Sway_Momentary; Surge_Sampled; Surge_Read; Surge_Momentary; "
                          "Heave_Sampled; Heave_Read; Heave_Momentary; Yaw_Sampled; Yaw_Read; Yaw_Momentary; "
                          "Roll_Sampled; Roll_Read; Roll_Momentary; Pitch_Sampled; Pitch_Read; Pitch_Momentary";
        }
        void AddDofValues(const utility::Dof& dof, RecorderDofInput type) override;
        void Write(bool sampled = false, bool newLine = true) override;

      private:
        DofSample m_DofValues{};
    };
} // namespace output
