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
        virtual void InsertSample(utility::Dof& dof) = 0;
        virtual void Stabilize(utility::Dof& dof) = 0;
    };

    class NoStabilizer : public StabilizerBase
    {
      public:
        void InsertSample(utility::Dof& dof) override{};
        void Stabilize(utility::Dof& dof) override{};
    };

    class MedianStabilizer : public StabilizerBase
    {
      public:
        MedianStabilizer(bool threeDofOnly);
        void InsertSample(utility::Dof& dof) override;
        void InsertSample(utility::Dof& dof, DWORD time);
        void Stabilize(utility::Dof & dof) override;

      private:
        DWORD m_WindowSize{0}, m_WindowHalf{0};
        std::vector<utility::DofValue> m_RelevantValues;
        std::mutex m_SampleMutex;

        std::map<DWORD, std::vector<float>> m_Samples;
        void RemoveOutdated(DWORD now);
        std::multimap<float, DWORD> GetSorted(utility::DofValue dof, DWORD now);
    };

    class SamplerBase
    {
      public:
        virtual ~SamplerBase() = default;
        virtual void InsertSample(utility::Dof& sample, int64_t time) = 0;
        virtual utility::Dof GetValue() = 0;

      protected:
        std::mutex m_SampleMutex;
    };

    class PassThroughSampler : public SamplerBase
    {
      public:
        explicit PassThroughSampler(const std::vector<utility::DofValue>& relevantValues)
            : m_RelevantValues(relevantValues){};
        void InsertSample(utility::Dof& sample, int64_t time) override;
        utility::Dof GetValue() override;

      protected:
        std::vector<utility::DofValue> m_RelevantValues;

      private:
        utility::Dof m_PassThrough{};
    };

    class MedianSampler : public PassThroughSampler
    {
      public:
        explicit MedianSampler(const std::vector<utility::DofValue>& relevantValues, unsigned windowSize)
            : PassThroughSampler(relevantValues), m_WindowSize(windowSize){};
        void InsertSample(utility::Dof& sample, int64_t time) override;
        utility::Dof GetValue() override;

      protected:
        unsigned m_WindowSize{0};

      private:
        std::deque<float> m_Samples[6]{};
    };

    class WeightedMedianSampler : public MedianSampler
    {
      public:
        explicit WeightedMedianSampler(const std::vector<utility::DofValue>& relevantValues, unsigned windowSize)
            : MedianSampler(relevantValues, windowSize), m_WindowHalf{windowSize / 2} {};
        void InsertSample(utility::Dof& sample, int64_t time) override;
        utility::Dof GetValue() override;

      private:
        std::deque<std::pair<float, int64_t>> m_Samples[6]{};
        unsigned m_WindowHalf{0};
    };

} // namespace filter