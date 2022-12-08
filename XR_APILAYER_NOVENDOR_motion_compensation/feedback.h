// Copyright(c) 2022 Sebastian Veith

#pragma once
namespace Feedback
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
        ConnectionLost
    };

    class AudioOut
    {
      public:
        bool Init();
        void Execute(Event feedback);

      private:
        std::map<Event, int> m_SoundResources;
    };

} // namespace Feedback

// Singleton accessor.
Feedback::AudioOut* GetAudioOut();