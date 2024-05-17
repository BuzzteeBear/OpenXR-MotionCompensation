// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "config.h"
#include "layer.h"
#include "output.h"
#include <log.h>

using namespace openxr_api_layer;
using namespace log;
using namespace utility;

bool ConfigManager::Init(const std::string& application)
{
    TraceLocalActivity(local);
    TraceLoggingWriteStart(local, "ConfigManager::Init", TLArg(application.c_str(), "Application"));

    // create application config file if not existing
    const auto& enabledKey = m_Keys.find(Cfg::Enabled);
    if (enabledKey == m_Keys.cend())
    {
        ErrorLog("%s: unable to find internal enable entry", __FUNCTION__);
        return false;
    }
    m_UsesOpenComposite = application.rfind("OpenComposite", 0) == 0;
    m_ApplicationIni = localAppData.string() + "\\" + application + ".ini";
    if (!application.empty() && _access(m_ApplicationIni.c_str(), 0) == -1)
    {
        if (!WritePrivateProfileString(enabledKey->second.first.c_str(),
                                       enabledKey->second.second.c_str(),
                                       "1",
                                       m_ApplicationIni.c_str()) &&
            2 != GetLastError())
        {
            ErrorLog("%s: unable to create %s, error: %s",
                     __FUNCTION__,
                     m_ApplicationIni.c_str(),
                     LastErrorMsg().c_str());
        }
    }
    const std::string coreIni(localAppData.string() + "\\" + "OpenXR-MotionCompensation.ini");
    if ((_access(coreIni.c_str(), 0)) != -1)
    {
        // check global deactivation flag
        char buffer[2048];
        if (0 < GetPrivateProfileString(enabledKey->second.first.c_str(),
                                        enabledKey->second.second.c_str(),
                                        nullptr,
                                        buffer,
                                        2047,
                                        coreIni.c_str()) &&
            std::string(buffer) != "1")
        {
            m_Values[Cfg::Enabled] = buffer;
            Log("motion compensation disabled globally");
            TraceLoggingWriteStop(local, "ConfigManager::Init", TLArg("Success", "Exit"));
            return true;
        }

        std::string errors;
        for (const auto& entry : m_Keys)
        {
            const std::string section = entry.second.first;
            const std::string key =
                entry.second.second + (m_UsesOpenComposite && m_CorValues.contains(entry.first) ? "_oc" : "");

            if (0 <
                GetPrivateProfileString(section.c_str(), key.c_str(), nullptr, buffer, 2047, m_ApplicationIni.c_str()))
            {
                TraceLoggingWriteTagged(local,
                                        "ConfigManager::Init",
                                        TLArg(section.c_str(), "Section"),
                                        TLArg(key.c_str(), "Key"),
                                        TLArg(buffer, "Value"),
                                        TLArg(application.c_str(), "Config"));
                m_Values[entry.first] = buffer;
            }
            else if ((0 <
                      GetPrivateProfileString(section.c_str(), key.c_str(), nullptr, buffer, 2047, coreIni.c_str())))
            {
                TraceLoggingWriteTagged(local,
                                        "ConfigManager::Init",
                                        TLArg(section.c_str(), "Section"),
                                        TLArg(key.c_str(), "Key"),
                                        TLArg(buffer, "Value"),
                                        TLArg("Default", "Config"));
                m_Values[entry.first] = buffer;
            }
            else
            {
                errors += "unable to read key: " + entry.second.second + " in section " + entry.second.first +
                          ", error: " + LastErrorMsg();
            }
        }
        if (!errors.empty())
        {
            ErrorLog("%s: unable to read configuration: %s", __FUNCTION__, errors.c_str());
            TraceLoggingWriteStop(local, "ConfigManager::Init", TLArg("Failure", "Exit"));
            return false;
        }
    }
    else
    {
        ErrorLog("%s: unable to find config file %s", __FUNCTION__, coreIni.c_str());
        TraceLoggingWriteStop(local, "ConfigManager::Init", TLArg("Failure", "Exit"));
        return false;
    }

    TraceLoggingWriteStop(local, "ConfigManager::Init", TLArg("Success", "Exit"));
    return true;
}

bool ConfigManager::GetBool(const Cfg key, bool& val)
{
    std::string strVal;
    if (GetString(key, strVal))
    {
        try
        {
            val = stoi(strVal);
            return true;
        }
        catch (std::exception& e)
        {
            ErrorLog("%s: unable to convert value (%s) for key (%s) to integer: %s",
                     __FUNCTION__,
                     strVal.c_str(),
                     m_Keys[key].first,
                     e.what());
        }
    }
    return false;
}

bool ConfigManager::GetInt(const Cfg key, int& val)
{
    std::string strVal;
    if (GetString(key, strVal))
    {
        try
        {
            val = stoi(strVal);
            return true;
        }
        catch (std::exception& e)
        {
            ErrorLog("%s: unable to convert value (%s) for key (%s) to integer: %s",
                     __FUNCTION__,
                     strVal.c_str(),
                     m_Keys[key].first.c_str(),
                     e.what());
        }
    }
    return false;
}

bool ConfigManager::GetFloat(const Cfg key, float& val)
{
    std::string strVal;
    if (GetString(key, strVal))
    {
        try
        {
            val = stof(strVal);
            return true;
        }
        catch (std::exception& e)
        {
            ErrorLog("%s: unable to convert value (%s) for key (%s) to double: %s",
                     __FUNCTION__,
                     strVal.c_str(),
                     m_Keys[key].first.c_str(),
                     e.what());
            return false;
        }
    }
    return false;
}

bool ConfigManager::GetString(const Cfg key, std::string& val)
{
    const auto it = m_Values.find(key);
    if (m_Values.end() == it)
    {
        ErrorLog("%s: unable to find value for key: [%s] %s ",
                 __FUNCTION__,
                 m_Keys[key].first.c_str(),
                 m_Keys[key].second.c_str());
        return false;
    }
    val = it->second;
    auto x = m_Keys[key];
    TraceLoggingWrite(g_traceProvider,
                      "ConfigManager::GetString",
                      TLArg(m_Keys[key].first.c_str(), "Section"),
                      TLArg(m_Keys[key].second.c_str(), "Key"),
                      TLArg(val.c_str(), "Value"));
    return true;
}

bool ConfigManager::GetShortcut(const Cfg key, std::set<int>& val)
{
    std::string strVal, errors;
    if (GetString(key, strVal))
    {
        if ("NONE" == strVal)
        {
            Log("keyboard shortcut is set to 'NONE': %s ", m_Keys[key].second.c_str());
            return true;
        }
        size_t separator;
        std::string remaining = strVal;
        do
        {
            separator = remaining.find_first_of("+");
            const std::string singleKey = remaining.substr(0, separator);
            auto it = m_ShortCuts.find(singleKey);
            if (it == m_ShortCuts.end())
            {
                errors +=
                    std::string(errors.empty() ? "" : "\n") + "unable to find virtual key number for: " + singleKey;
            }
            else
            {
                val.insert(it->second);
            }
            remaining.erase(0, separator + 1);
        } while (std::string::npos != separator);
    }
    if (!errors.empty())
    {
        ErrorLog("%s: unable to convert value (%s) for key (%s) to shortcut: %s",
                 __FUNCTION__,
                 strVal.c_str(),
                 m_Keys[key].first.c_str(),
                 errors.c_str());
        return false;
    }
    return true;
}

bool ConfigManager::IsVirtualTracker()
{
    std::string type;
    GetString(Cfg::TrackerType, type);
    return "srs" == type || "flypt" == type || "yaw" == type;
}

std::string ConfigManager::GetControllerSide()
{
    std::string side{"left"};
    if (IsVirtualTracker())
    {
        // default to left controller for virtual trackers
        return side;
    }
    if (!GetString(Cfg::TrackerSide, side))
    {
        ErrorLog("%s: unable to determine controller side. Defaulting to %s", __FUNCTION__, side.c_str());
    }
    if ("right" != side && "left" != side)
    {
        ErrorLog("%s: invalid controller side: %s. Defaulting to 'left'", __FUNCTION__, side.c_str());
        side = "left";
    }
    return side;
}

void ConfigManager::SetValue(const Cfg key, const bool val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(const Cfg key, const int val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(const Cfg key, const float val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(const Cfg key, const std::string& val)
{
    m_Values[key] = val;
    TraceLoggingWrite(g_traceProvider,
                      "ConfigManager::SetValue",
                      TLArg(m_Keys[key].first.c_str(), "Section"),
                      TLArg(m_Keys[key].second.c_str(), "Key"),
                      TLArg(val.c_str(), "Value"));
}

void ConfigManager::WriteConfig(const bool forApp)
{
    TraceLocalActivity(local);
    TraceLoggingWriteStart(local, "ConfigManager::WriteConfig", TLArg(forApp, "AppSpecific"));

    bool error{false};
    const std::string configFile =
        forApp ? m_ApplicationIni : localAppData.string() + "\\" + "OpenXR-MotionCompensation.ini";
    for (const auto key : m_KeysToSave)
    {
        if (const auto& keyEntry = m_Keys.find(key); m_Keys.end() != keyEntry)
        {
            const std::string section = keyEntry->second.first;
            const std::string keyName =
                keyEntry->second.second + (m_UsesOpenComposite && m_CorValues.contains(keyEntry->first) ? "_oc" : "");
            if (const auto& valueEntry = m_Values.find(key); m_Values.end() != valueEntry)
            {
                if (!WritePrivateProfileString(section.c_str(),
                                               keyName.c_str(),
                                               valueEntry->second.c_str(),
                                               configFile.c_str()) &&
                    2 != GetLastError())
                {
                    error = true;
                    ErrorLog("%s: unable to write value %s into key %s to section %s in %s, error: %s",
                             __FUNCTION__,
                             valueEntry->second.c_str(),
                             keyName.c_str(),
                             section.c_str(),
                             configFile.c_str(),
                             LastErrorMsg().c_str());
                }
                else
                {
                    TraceLoggingWriteTagged(local,
                                            "ConfigManager::WriteConfig",
                                            TLArg(section.c_str(), "Section"),
                                            TLArg(keyName.c_str(), "Key"),
                                            TLArg(valueEntry->second.c_str(), "Value"));
                }
            }
            else
            {
                error = true;
                ErrorLog("%s: key not found in value map: %s:%s", __FUNCTION__, section.c_str(), keyName.c_str());
            }
        }
        else
        {
            error = true;
            ErrorLog("%s: key not found in key map: %d", __FUNCTION__, key);
        }
    }
    Log("current configuration %saved to %s", error ? "could not be " : "", configFile.c_str());
    output::AudioOut::Execute(!error ? output::Event::Save : output::Event::Error);

    TraceLoggingWriteStop(local, "ConfigManager::WriteConfig", TLArg(!error, "Success"));
}

std::unique_ptr<ConfigManager> g_config = nullptr;

ConfigManager* GetConfig()
{
    if (!g_config)
    {
        g_config = std::make_unique<ConfigManager>();
    }
    return g_config.get();
}