#pragma once

#include "pch.h"

namespace utilities
{
    class KeyboardInput
    {
      public:

        bool UpdateKeyState(const std::set<int>& vkKeySet, bool& isRepeat);

      private:
        std::map < std::set<int>, std::pair<bool, std::chrono::steady_clock::time_point>> m_KeyStates;
        const std::chrono::milliseconds m_KeyRepeatDelay = 200ms;
    };

    class PoseCache
    {
      public:
        PoseCache(XrTime tolerance) : m_Tolerance(tolerance){};

        void AddPose(XrTime time, XrPosef pose);
        XrPosef GetPose(XrTime time) const;

        // remove outdated entries
        void CleanUp(XrTime time);

      private:
        std::map<XrTime, XrPosef> m_Cache{};
        XrTime m_Tolerance;
    };
}