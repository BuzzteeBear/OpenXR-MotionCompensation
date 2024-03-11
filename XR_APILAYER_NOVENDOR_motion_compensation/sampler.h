// Copyright(c) 2024 Sebastian Veith

#pragma once

#include "filter.h"
#include "output.h"

class Sampler
{
  public:
    Sampler(bool (*read)(utility::Dof&, int64_t, utility::DataSource*),
            utility::DataSource* source,
            const std::shared_ptr<output::RecorderBase>& recorder);
    ~Sampler();

    void SetWindowSize(unsigned size) const;
    void StartSampling();
    void StopSampling();
    bool ReadData(utility::Dof& dof, XrTime time);

  private:
    void DoSampling();

    std::atomic_bool m_IsSampling{false};
    std::thread* m_Thread{nullptr};
    bool (*m_Read)(utility::Dof&, int64_t, utility::DataSource*);
    utility::DataSource* m_Source{nullptr};
    std::shared_ptr<filter::StabilizerBase> m_Stabilizer{};
    std::chrono::microseconds m_Interval{1ms};
    bool m_RecordSamples{false};
    std::shared_ptr<output::RecorderBase> m_Recorder{};
};
