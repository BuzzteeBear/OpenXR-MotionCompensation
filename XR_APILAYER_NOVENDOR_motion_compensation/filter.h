// Copyright(c) 2022 Sebastian Veith

#pragma once

#include <log.h>

namespace filter
{
    template <typename Value>
    class FilterBase
    {
      public:
        explicit FilterBase(const float strength, const std::string& type) : m_Type(type)
        {
            FilterBase<Value>::SetStrength(strength);
        }
        virtual ~FilterBase(){};
        virtual float SetStrength(const float strength)
        {
            const float limitedStrength = std::min(1.0f, std::max(0.0f, strength));
            openxr_api_layer::log::DebugLog("%s filter strength set: %f", m_Type.c_str(), limitedStrength);
            m_Strength = limitedStrength;
            return m_Strength;
        }
        void Filter(Value& value)
        {
            if (0.0f < m_Strength)
            {
                ApplyFilter(value);
            }
        }
        virtual void ApplyFilter(Value& value) = 0;
        virtual void Reset(const Value& value) = 0;

      protected:
        float m_Strength;
        std::string m_Type;
    };

    // translational filters
    class SingleEmaFilter : public FilterBase<XrVector3f>
    {
      public:
        explicit SingleEmaFilter(float strength);
        float SetStrength(float strength) override;
        void ApplyFilter(XrVector3f& location) override;
        void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_Alpha{1.0f - m_Strength, 1.0f - m_Strength, 1.0f - m_Strength};
        XrVector3f m_OneMinusAlpha{m_Strength, m_Strength, m_Strength};
        XrVector3f m_Ema{0, 0, 0};
        [[nodiscard]] XrVector3f EmaFunction(XrVector3f current, XrVector3f stored) const;
      private:
        float m_VerticalFactor{1.f};
    };

    class DoubleEmaFilter : public SingleEmaFilter
    {
      public:
        explicit DoubleEmaFilter(const float strength) : SingleEmaFilter(strength){};
        void ApplyFilter(XrVector3f& location) override;
        void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_EmaEma{0, 0, 0};
    };

    class TripleEmaFilter : public DoubleEmaFilter
    {
      public:
        explicit TripleEmaFilter(const float strength) : DoubleEmaFilter(strength){};
        void ApplyFilter(XrVector3f& location) override;
        void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_EmaEmaEma{0, 0, 0};
    };

    // rotational filters
    class SingleSlerpFilter : public FilterBase<XrQuaternionf>
    {
      public:
        explicit SingleSlerpFilter(const float strength) : FilterBase(strength, "rotational"){};
        void ApplyFilter(XrQuaternionf& rotation) override;
        void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_FirstStage = xr::math::Quaternion::Identity();
    };

    class DoubleSlerpFilter : public SingleSlerpFilter
    {
      public:
        explicit DoubleSlerpFilter(const float strength) : SingleSlerpFilter(strength){};
        void ApplyFilter(XrQuaternionf& rotation) override;
        void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_SecondStage = xr::math::Quaternion::Identity();
    };

    class TripleSlerpFilter : public DoubleSlerpFilter
    {
      public:
        explicit TripleSlerpFilter(const float strength) : DoubleSlerpFilter(strength){};
        void ApplyFilter(XrQuaternionf& rotation) override;
        void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_ThirdStage = xr::math::Quaternion::Identity();
    };

    class StabilizerBase
    {
      public:
        virtual ~StabilizerBase() = default;
        virtual void SetStrength(float frequency) = 0;
        virtual void SetStartTime(int64_t now) = 0;
        virtual void Insert(utility::Dof& sample, int64_t now) = 0;
        virtual void Read(utility::Dof& dof) = 0;

      protected:
        std::mutex m_SampleMutex;
    };

    class PassThroughStabilizer : public StabilizerBase
    {
      public:
        explicit PassThroughStabilizer(const std::vector<utility::DofValue>& relevant);;
        void SetStrength(float strength) override{};
        void SetStartTime(int64_t now) override{};
        void Insert(utility::Dof& dof, int64_t now) override;
        void Read(utility::Dof& dof) override;

      protected:
        std::vector<utility::DofValue> m_Relevant;
        utility::Dof m_CurrentSample{};
        utility::Dof m_Factor{};
    };

    class LowPassStabilizer : public PassThroughStabilizer
    {
      public:
        explicit LowPassStabilizer(const std::vector<utility::DofValue>& relevant);
        void Read(utility::Dof& dof) override;

      protected:
        void SetFrequencies(float strength);
        bool Disabled(const utility::Dof& dof);

        bool m_Initialized{false};
        utility::Dof m_Frequency{};

      private:
        bool m_Disabled{false}, m_Blocking{false};
    };

    class EmaStabilizer : public LowPassStabilizer
    {
      public:
        explicit EmaStabilizer(const std::vector<utility::DofValue>& relevant)
            : LowPassStabilizer(relevant){};
        void SetStrength(float strength) override;
        void SetStartTime(int64_t now) override;
        void Insert(utility::Dof& dof, int64_t now) override;

      private:
        int64_t m_LastSampleTime{};
    };

    class BiQuadStabilizer : public LowPassStabilizer
    {
      public:
        explicit BiQuadStabilizer(const std::vector<utility::DofValue>& relevant)
            : LowPassStabilizer(relevant){};
        void SetStrength(float strength) override;
        void SetStartTime(int64_t now) override;
        void Insert(utility::Dof& dof, int64_t now) override;

      private:
        void ResetFilters();
        double Filter(float dofValue, utility::DofValue value);

        class BiQuadFilter
        {
          public:
            BiQuadFilter(float frequency);
            double Filter(double value);

          private:
            float m_SamplingFrequency{600.f};

            double m_A;
            double m_D1;
            double m_D2;
            double m_W0;
            double m_W1;
            double m_W2;
        };

        std::unique_ptr<BiQuadFilter> m_Filter[9]{};
    };
} // namespace filter