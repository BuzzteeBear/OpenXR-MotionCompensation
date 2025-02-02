// Copyright(c) 2022 Sebastian Veith

#pragma once

#include <log.h>

namespace openxr_api_layer
{
    class OpenXrLayer;
}

namespace input
{
    class InputHandler;
    class CorEstimatorCmd;
    class CorEstimatorResult;
}

namespace output
{
    class PoseMmf;
}

namespace utility
{
    constexpr float floatPi{static_cast<float>(M_PI)};
    constexpr float angleToRadian{floatPi / 180.0f};

    typedef struct dof
    {
        float data[6]{};
    } Dof;

    enum DofValue
    {
        sway = 0,
        surge,
        heave,
        yaw,
        roll,
        pitch
    };

    XrVector3f ToEulerAngles(XrQuaternionf q);

    class AutoActivator
    {
      public:
        explicit AutoActivator(const std::shared_ptr<input::InputHandler>& input);
        void ActivateIfNecessary(XrTime time);

      private:
        std::shared_ptr<input::InputHandler> m_Input;
        bool m_Activate{false};
        bool m_Countdown{false};
        int m_SecondsLeft{0};
        XrTime m_ActivationTime{0};
    };

    template <typename Sample>
    class Cache
    {
      public:
        explicit Cache(std::string type, Sample fallback) : m_Fallback(fallback), m_SampleType(std::move(type)) {};

        void SetTolerance(const XrTime tolerance)
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::SetTolerance", TLArg(tolerance, "Tolerance"));

            m_Tolerance = tolerance;

            TraceLoggingWriteStop(local, "Cache::SetTolerance");
        }

        void AddSample(XrTime time, Sample sample, const bool override)
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::AddSample", TLArg(m_SampleType.c_str(), "Type"), TLArg(time, "Time"));

            std::unique_lock lock(m_CacheLock);
            if (m_Cache.contains(time))
            {
                if (!override)
                {
                    DebugLog("AddSample(%s) at %lld: omitted", m_SampleType.c_str(), time);
                    TraceLoggingWriteStop(local, "Cache::AddSample", TLArg(true, "Omitted"));
                    return;
                }
                DebugLog("AddSample(%s) at %lld: overriden", m_SampleType.c_str(), time);
                TraceLoggingWriteTagged(local, "Cache::AddSample", TLArg(true, "Override"));
            }
            else
            {
                DebugLog("AddSample(%s) at %lld: inserted", m_SampleType.c_str(), time);
            }
            m_Cache[time] = sample;
            TraceLoggingWriteStop(local, "Cache::AddSample");
        }

        Sample GetSample(XrTime time)
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::GetSample", TLArg(m_SampleType.c_str(), "Type"), TLArg(time, "Time"));

            std::unique_lock lock(m_CacheLock);

            auto it = m_Cache.lower_bound(time);
            const bool itIsEnd = m_Cache.end() == it;
            if (!itIsEnd)
            {
                if (it->first == time)
                {
                    // exact entry found
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Exact", "Match"),
                                          TLArg(it->first, "Time"));

                    DebugLog("GetSample(%s) at %lld: exact match found", m_SampleType.c_str(), time);

                    m_ReportError = true;
                    return it->second;
                }
                if (it->first <= time + m_Tolerance)
                {
                    // succeeding entry is within tolerance
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Later", "Match"),
                                          TLArg(it->first, "Time"));
                    DebugLog("GetSample(%s) at %lld: later match found: %lld", m_SampleType.c_str(), time, it->first);

                    m_ReportError = true;
                    return it->second;
                }
            }
            const bool itIsBegin = m_Cache.begin() == it;
            if (!itIsBegin)
            {
                auto lowerIt = it;
                --lowerIt;
                if (lowerIt->first >= time - m_Tolerance)
                {
                    // preceding entry is within tolerance
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Earlier", "Match"),
                                          TLArg(lowerIt->first, "Time"));
                    DebugLog("GetSample(%s) at %lld: earlier match found: %lld",
                             m_SampleType.c_str(),
                             time,
                             lowerIt->first);

                    m_ReportError = true;
                    return lowerIt->second;
                }
            }

            auto ErrOut = [this, time](const std::string& msg,
                                       const std::optional<XrTime> alternative,
                                       const bool disableReport = true) {
                if (m_ReportError)
                {
                    const std::string alt =
                        alternative.has_value() ? ": t = " + std::to_string(alternative.value()) : "";
                    ErrorLog("GetSample(%s) at %lld: %s%s", m_SampleType.c_str(), time, msg.c_str(), alt.c_str());
                    m_ReportError = !disableReport;
                }
            };

            ErrOut("unable to find sample", {}, false);

            if (!itIsEnd)
            {
                if (!itIsBegin)
                {
                    auto lowerIt = it;
                    --lowerIt;
                    // both entries are valid -> select better match
                    auto returnIt = (time - lowerIt->first < it->first - time ? lowerIt : it);

                    ErrOut("using best match", returnIt->first);
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Estimated Both", "Match"),
                                          TLArg(it->first, "Time"));
                    return returnIt->second;
                }
                // higher entry is first in cache -> use it
                ErrOut("using best match", it->first);
                TraceLoggingWriteStop(local,
                                      "Cache::GetSample",
                                      TLArg(m_SampleType.c_str(), "Type"),
                                      TLArg("Estimated Later", "Match"),
                                      TLArg(it->first, "Time"));
                return it->second;
            }
            if (!itIsBegin)
            {
                auto lowerIt = it;
                --lowerIt;
                // lower entry is last in cache-> use it
                ErrOut("using best match", lowerIt->first);
                TraceLoggingWriteStop(local,
                                      "Cache::GetSample",
                                      TLArg(m_SampleType.c_str(), "Type"),
                                      TLArg("Estimated Earlier", "Match"),
                                      TLArg(lowerIt->first, "Time"));
                return lowerIt->second;
            }
            // cache is empty -> return fallback
            ErrOut("using fallback!!!", {});
            TraceLoggingWriteStop(local,
                                  "Cache::GetSample",
                                  TLArg(m_SampleType.c_str(), "Type"),
                                  TLArg("Fallback", "Match"));
            return m_Fallback;
        }

        // remove outdated entries
        void CleanUp(const XrTime time)
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::CleanUp", TLArg(m_SampleType.c_str(), "Type"), TLArg(time, "Time"));

            std::unique_lock lock(m_CacheLock);

            auto it = m_Cache.lower_bound(time - m_Tolerance);
            if (m_Cache.begin() != it)
            {
                --it;
                if (m_Cache.end() != it && m_Cache.begin() != it)
                {
                    TraceLoggingWriteTagged(local, "Cache::CleanUp", TLArg(it->first, "Eraaed"));
                    m_Cache.erase(m_Cache.begin(), it);
                }
            }

            TraceLoggingWriteStop(local, "Cache::CleanUp");
        }

      private:
        std::map<XrTime, Sample> m_Cache{};
        mutable std::mutex m_CacheLock;
        Sample m_Fallback;
        XrTime m_Tolerance{2000000};
        std::string m_SampleType;
        bool m_ReportError{true};
    };

    class DataSource
    {
      public:
        virtual ~DataSource() = default;
        virtual bool Open(int64_t time) = 0;
    };

    class Mmf : public DataSource
    {
      public:
        Mmf();
        ~Mmf() override;
        void SetWriteable(unsigned fileSize);
        void SetName(const std::string& name);
        bool Open(int64_t time) override;
        bool Read(void* buffer, size_t size, int64_t time);
        bool Write(void* buffer, size_t size);
        void Close();

      private:
        bool ReadWrite(void* buffer, size_t size, bool write, int64_t time = 0);
        XrTime m_Check{1000000000}; // reopen mmf once a second by default
        XrTime m_LastRefresh{0};
        std::string m_Name;
        HANDLE m_FileHandle{nullptr};
        DWORD m_FileSize{0};
        void* m_View{nullptr};
        bool m_WriteAccess{false};
        bool m_ConnectionLost{false};
        std::mutex m_MmfLock;
    };

    class CorEstimator
    {
      public:
        explicit CorEstimator(openxr_api_layer::OpenXrLayer* layer);
        bool Init();
        void Execute(XrTime time);
        bool TransmitHmd() const;
        std::vector<std::tuple<int, XrPosef, float>> GetAxes();
        std::vector<std::pair<XrPosef, int>> GetSamples();

      private:
        bool m_Enabled{false}, m_Active{false}, m_UseTracker{false};
        std::unique_ptr<input::CorEstimatorCmd> m_CmdMmf{};
        std::unique_ptr<input::CorEstimatorResult> m_ResultMmf{};
        std::unique_ptr<output::PoseMmf> m_PoseMmf{};
        openxr_api_layer::OpenXrLayer* m_Layer = nullptr;
        std::vector<std::pair<XrPosef, int>> m_Samples{};
        std::vector<std::tuple<int, XrPosef, float>> m_Axes{};
    };

    static inline bool endsWith(const std::string& str, const std::string& substr)
    {
        const auto pos = str.find(substr);
        return pos != std::string::npos && pos == str.size() - substr.size();
    }

    std::string LastErrorMsg();

} // namespace utility