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
        virtual void SetWindowSize(unsigned size) = 0;
        virtual void InsertSample(utility::Dof& sample, int64_t time) = 0;
        virtual void Stabilize(utility::Dof& dof) = 0;

      protected:
        std::mutex m_SampleMutex;
    };

    class PassThroughStabilizer : public StabilizerBase
    {
      public:
        explicit PassThroughStabilizer(const std::vector<utility::DofValue>& relevantValues)
            : m_RelevantValues(relevantValues){};
        void SetWindowSize(unsigned size) override{};
        void InsertSample(utility::Dof& sample, int64_t time) override;
        void Stabilize(utility::Dof& dof) override;

      protected:
        std::vector<utility::DofValue> m_RelevantValues;

      private:
        utility::Dof m_PassThrough{};
    };

    class MedianStabilizer : public PassThroughStabilizer
    {
      public:
        explicit MedianStabilizer(const std::vector<utility::DofValue>& relevantValues, unsigned windowSize)
            : PassThroughStabilizer(relevantValues), m_WindowSize(windowSize){};
        void SetWindowSize(unsigned size) override
        {
            m_WindowSize = size;
            openxr_api_layer::log::DebugLog("stabilizer averaging time: %u ns", size);
        }
        void InsertSample(utility::Dof& sample, int64_t time) override;
        void Stabilize(utility::Dof& dof) override;

      protected:
        std::atomic<unsigned> m_WindowSize{0};
        std::multimap<float, int64_t> m_Samples[6]{};
    };

    class WeightedMedianStabilizer : public MedianStabilizer
    {
      public:
        explicit WeightedMedianStabilizer(const std::vector<utility::DofValue>& relevantValues, unsigned windowSize)
            : MedianStabilizer(relevantValues, windowSize), m_WindowHalf{windowSize / 2} {};
        void SetWindowSize(unsigned size) override
        {
            MedianStabilizer::SetWindowSize(size);
            m_WindowHalf = size / 2;
        }
        void Stabilize(utility::Dof& dof) override;

      private:
        unsigned m_WindowHalf{0};
    };

} // namespace filter