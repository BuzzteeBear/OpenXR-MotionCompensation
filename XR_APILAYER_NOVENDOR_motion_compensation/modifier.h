#pragma once

namespace modifier
{
    class ModifierBase
    {
      public:
        virtual ~ModifierBase() = default;
        void SetActive(bool apply);
        void SetFwdToStage(const XrPosef& pose);
        virtual void Apply(XrPosef& target, const XrPosef& reference) const = 0;

      protected:
        bool m_ApplyTranslation{false}, m_ApplyRotation{false};
        float m_Pitch{1.f}, m_Roll{1.f}, m_Yaw{1.f}, m_Sway{1.f}, m_Heave{1.f}, m_Surge{1.f};
        XrPosef m_FwdToStage{xr::math::Pose::Identity()}, m_StageToFwd{xr::math::Pose::Identity()};
    };

    class TrackerModifier final : public ModifierBase
    {
      public:
        TrackerModifier();
        void Apply(XrPosef& target, const XrPosef& reference) const override;
    };

    class HmdModifier final : public ModifierBase
    {
      public:
        HmdModifier();
        void Apply(XrPosef& target, const XrPosef& reference) const override;
    };
} // namespace modifier