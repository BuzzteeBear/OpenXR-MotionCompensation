// Copyright(c) 2024 Sebastian Veith

#pragma once
#include "filter.h"

class Sampler
{
  public:
    Sampler(bool (*read)(utility::Dof&, int64_t, utility::DataSource*), utility::DataSource* source)
        : m_Read(read), m_Source(source){}
    ~Sampler();

    void StartSampling();
    void StopSampling();
    bool ReadData(utility::Dof& dof, XrTime time);

  private:
    void DoSampling();

    std::atomic_bool m_IsSampling{false};
    std::thread* m_thread{nullptr};
    bool (*m_Read)(utility::Dof&, int64_t, utility::DataSource*);
    utility::DataSource* m_Source{nullptr};
    // TODO: read window size from config
    std::shared_ptr<filter::StabilizerBase> m_Stabilizer{std::make_shared<filter::WeightedMedianStabilizer>(
        std::vector<utility::DofValue>{utility::yaw, utility::roll, utility::pitch},
        100000000)};
    inline static std::chrono::duration m_Interval{std::chrono::milliseconds(2)};
};
