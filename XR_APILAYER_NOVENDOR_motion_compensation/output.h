// Copyright(c) 2022 Sebastian Veith

#pragma once
#include "resource.h"

namespace output
{
    enum class Event
    {
        Error = -1,
        Load,
        Save,
        Activated,
        Deactivated,
        Calibrated,
        Plus,
        Minus,
        Max,
        Min,
        Up,
        Down,
        Forward,
        Back,
        Left,
        Right,
        RotLeft,
        RotRight,
        DebugOn,
        DebugOff,
        ConnectionLost,
        EyeCached,
        EyeCalculated,
        OverlayOn,
        OverlayOff,
        ModifierOn,
        ModifierOff,
        CalibrationLost,
        VerboseOn,
        VerboseOff,
        RecorderOn,
        RecorderOff,
        StabilizerOn,
        StabilizerOff
    };

    class AudioOut
    {
      public:
        static void Execute(Event event);
        static void CountDown(int seconds);
      private:
        inline static const std::map<Event, int> m_SoundResources{{Event::Error, ERROR_WAV},
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
                                                                  {Event::StabilizerOff, STABILIZER_OFF_WAV}};
    };

    constexpr uint32_t m_RecorderMax{36000}; // max 10 min @ 100 frames/s

    enum RecorderDofInput
    {
        Sampled = 0,
        Read,
        Momentary
    };

    enum RecorderPoseInput
    {
        Reference = 0,
        Input,
        Filtered,
        Modified,
        Delta
    };

    struct DofSample
    {
        utility::Dof sampled{};
        utility::Dof read{};
        utility::Dof momentary{};
    };

    struct PoseSample
    {
        XrPosef input{};
        XrPosef filtered{};
        XrPosef modified{};
        XrPosef delta{};
        XrPosef reference{};
    };

    class RecorderBase
    {
      public:
        RecorderBase() = default;
        virtual ~RecorderBase() = default;
        virtual bool Toggle(bool isCalibrated) = 0;
        virtual void AddFrameTime(XrTime time) = 0;
        virtual void AddPose(const XrPosef& pose, RecorderPoseInput type) = 0;
        virtual void AddDofValues(const utility::Dof& dofValues, RecorderDofInput type) = 0;
        virtual void Write(bool newLine = true) = 0;
    };

    class NoRecorder final : public RecorderBase
    {
      public:
        bool Toggle(bool isCalibrated) override;
        void AddFrameTime(XrTime time) override{};
        void AddPose(const XrPosef& pose, RecorderPoseInput type) override{};
        void AddDofValues(const utility::Dof& dofValues, RecorderDofInput type) override{};
        void Write(bool newLine = true) override{};
    };

    class PoseRecorder : public RecorderBase
    {
      public:
        PoseRecorder();
        ~PoseRecorder() override;
        bool Toggle(bool isCalibrated) override;
        void AddFrameTime(XrTime time) override;
        void AddPose(const XrPosef& pose, RecorderPoseInput type) override;
        void AddDofValues(const utility::Dof& dofValues, RecorderDofInput type) override{};
        void Write(bool newLine = true) override;

      protected:
        bool m_Started{false}, m_PoseRecorded{false};
        std::ofstream m_FileStream;
        XrTime m_FrameTime{};
        std::string m_HeadLine{"Time; FrameTime; "
                               "X_Input; X_Filtered; X_Modified; X_Reference; X_Delta;"
                               "Y_Input; Y_Filtered; Y_Modified; Y_Reference; Y_Delta;"
                               "Z_Input; Z_Filtered; Z_Modified; Z_Reference; Z_Delta;"
                               "A_Input; A_Filtered; A_Modified; A_Reference; A_Delta;"
                               "B_Input; B_Filtered; B_Modified; B_Reference; B_Delta;"
                               "C_Input; C_Filtered; C_Modified; C_Reference; C_Delta;"
                               "D_Input; D_Filtered; D_Modified; D_Reference; D_Delta"};
        std::mutex m_RecorderMutex;

    private:
        virtual bool Start();
        virtual void Stop();

        PoseSample m_Poses{};
        uint32_t m_Counter{0};
    };

    class PoseAndDofRecorder final : public PoseRecorder
    {
      public:
        PoseAndDofRecorder()
        {
            m_HeadLine += "; Sway_Sampled; Sway_Read; Sway_Momentary; Surge_Sampled; Surge_Read; Surge_Momentary; "
                          "Heave_Sampled; Heave_Read; Heave_Momentary; Yaw_Sampled; Yaw_Read; Yaw_Momentary; "
                          "Pitch_Sampled; Pitch_Read; Pitch_Momentary; Roll_Sampled; Roll_Read; Roll_Momentary";
        }
        void AddDofValues(const utility::Dof& dof, RecorderDofInput type) override;
        void Write(bool newLine = true) override;

      private:
        DofSample m_DofValues{};
    };

} // namespace output
