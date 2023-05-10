// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "config.h"
#include "layer.h"
#include "feedback.h"
#include "utility.h"
#include <log.h>

using namespace motion_compensation_layer::log;
using namespace utility;

bool ConfigManager::Init(const std::string& application)
{
    // create application config file if not existing
    const auto& enabledKey = m_Keys.find(Cfg::Enabled);
    if (enabledKey == m_Keys.cend())
    {
        ErrorLog("unable to find internal enable entry\n");
        return false;
    }
    m_UsesOpenComposite = application.rfind("OpenComposite", 0) == 0;
    m_ApplicationIni = motion_compensation_layer::localAppData.string() + "\\" + application + ".ini";
    if ((_access(m_ApplicationIni.c_str(), 0)) == -1)
    {

        if (!WritePrivateProfileString(enabledKey->second.first.c_str(),
                                       enabledKey->second.second.c_str(),
                                       "1",
                                       m_ApplicationIni.c_str()) &&
            2 != GetLastError())
        {
            ErrorLog("%s: unable to create %s, error: %s\n",
                     __FUNCTION__,
                     m_ApplicationIni.c_str(),
                     LastErrorMsg().c_str());
        }
    }
    const std::string coreIni(motion_compensation_layer::localAppData.string() + "\\" + "OpenXR-MotionCompensation.ini");
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
            Log("motion compensation disabled globally\n");
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
                m_Values[entry.first] = buffer;
            }
            else if ((0 <
                      GetPrivateProfileString(section.c_str(), key.c_str(), nullptr, buffer, 2047, coreIni.c_str())))
            {
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
            ErrorLog("%s: unable to read configuration: %s\n", __FUNCTION__, errors.c_str());
            return false;
        }
    }
    else
    {
        ErrorLog("%s: unable to find config file %s\n", __FUNCTION__, coreIni.c_str());
        return false;
    }
    
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
        catch (std::exception e)
        {
            ErrorLog("%s: unable to convert value (%s) for key (%s) to integer: %s\n",
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
        catch (std::exception e)
        {
            ErrorLog("%s: unable to convert value (%s) for key (%s) to integer: %s\n",
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
        catch (std::exception e)
        {
            ErrorLog("%s: unable to convert value (%s) for key (%s) to double: %s\n",
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
        ErrorLog("%s: unable to find value for key: [%s] %s \n",
                 __FUNCTION__,
                 m_Keys[key].first.c_str(), m_Keys[key].second.c_str());
        return false;
    }
    val = it->second;
    return true;
}
bool ConfigManager::GetShortcut(const Cfg key, std::set<int>& val)
{
    std::string strVal, errors;
    if (GetString(key, strVal))
    {
        size_t separator;
        std::string remaining = strVal;
        do
        {
            separator = remaining.find_first_of("+");
            const std::string key = remaining.substr(0, separator);
            auto it = m_ShortCuts.find(key);
            if (it == m_ShortCuts.end())
            {
                errors += "unable to find virtual key number for: " + key + "\n";
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
        ErrorLog("%s: unable to convert value (%s) for key (%s) to shortcut: %s\n",
                 __FUNCTION__,
                 strVal.c_str(),
                 m_Keys[key].first.c_str(),
                 errors.c_str());
        return false;
    }
    return true;
}
std::string ConfigManager::GetControllerSide()
{
    std::string side{"left"};
    if (!GetString(Cfg::TrackerSide, side))
    {
        ErrorLog("%s: unable to determine controller side. Defaulting to %s\n", __FUNCTION__, side.c_str());
    }
    if ("right" != side && "left" != side)
    {
        ErrorLog("%s: invalid controller side: %s. Defaulting to 'left'\n", __FUNCTION__, side.c_str());
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
}

void ConfigManager::WriteConfig(const bool forApp)
{
    bool error{false};
    const std::string configFile =
        forApp ? m_ApplicationIni
               : motion_compensation_layer::localAppData.string() + "\\" + "OpenXR-MotionCompensation.ini";
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
                    DWORD err = GetLastError();
                    ErrorLog("%s: unable to write value %s into key %s to section %s in %s, error: %s\n",
                             __FUNCTION__,
                             valueEntry->second.c_str(),
                             keyName.c_str(),
                             section.c_str(),
                             configFile.c_str(),
                             LastErrorMsg().c_str());
                }
            }
            else
            {
                error = true;
                ErrorLog("%s: key not found in value map: %d:%s\n", __FUNCTION__, section.c_str(), keyName.c_str());
            }
        }
        else
        {
            error = true;
            ErrorLog("%s: key not found in key map: %d\n", __FUNCTION__, key);
        }
    }
    Log("current configuration %ssaved to %s\n", error ? "could not be " : "", m_ApplicationIni.c_str());
    GetAudioOut()->Execute(!error ? Feedback::Event::Save : Feedback::Event::Error);
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