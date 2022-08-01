// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "config.h"
#include "utility.h"
#include <log.h>
#include <util.h>

using namespace motion_compensation_layer::log;
using namespace utility;

bool ConfigManager::Init(const std::string& application)
{
    if (!InitDirectory())
    {
        return false;
    }
    // create application config file if not existing
    m_ApplicationIni = m_DllDirectory + application + ".ini";
    if ((_access(m_ApplicationIni.c_str(), 0)) == -1)
    {
        if (!WritePrivateProfileString("placeholder", "created", "1", m_ApplicationIni.c_str()) && 2 != GetLastError())
        {
            DWORD err = GetLastError();
            ErrorLog("%s: unable to create %s, error = %d : %s\n",
                     __FUNCTION__,
                     m_ApplicationIni.c_str(),
                     err,
                     LastErrorMsg(err).c_str());
        }
    }
    const std::string coreIni(m_DllDirectory + "OpenXR-MotionCompensation.ini");
    if ((_access(coreIni.c_str(), 0)) != -1)
    {
        std::string errors;
        for (const auto& entry : m_Keys)
        {
            char buffer[2048];
            int read{0};
            if (0 < GetPrivateProfileString(entry.second.first.c_str(),
                                            entry.second.second.c_str(),
                                            NULL,
                                            buffer,
                                            2047,
                                            m_ApplicationIni.c_str()))
            {
                m_Values[entry.first] = buffer;
            }
            else if ((0 < GetPrivateProfileString(entry.second.first.c_str(),
                                                  entry.second.second.c_str(),
                                                  NULL,
                                                  buffer,
                                                  2047,
                                                  coreIni.c_str())))
            {
                m_Values[entry.first] = buffer;
            }
            else
            {
                int err = GetLastError();
                errors += "unable to read key: " + entry.second.second + " in section " + entry.second.first +
                          (err ? " error: " + std::to_string(err) + ":" + LastErrorMsg(err) : "");    
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

bool ConfigManager::GetBool(Cfg key, bool& val)
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
bool ConfigManager::GetInt(Cfg key, int& val)
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
bool ConfigManager::GetFloat(Cfg key, float& val)
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
                     m_Keys[key].first,
                     e.what());
            return false;
        } 
    }
    return false;
}
bool ConfigManager::GetString(Cfg key, std::string& val)
{
    const auto it = m_Values.find(key);
    if (m_Values.end() == it)
    {
        ErrorLog("%s: unable to find value for key: %s\n", __FUNCTION__, m_Keys[key].first);
        return false;
    }
    val = it->second;
    return true;
}
bool ConfigManager::GetShortcut(Cfg key, std::set<int>& val)
{
    std::string strVal, remaining, errors;
    if (GetString(key, strVal))
    {
        size_t separator;
        remaining = strVal;
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
        ErrorLog("%s: unable to convert value (%s) for key (%s) to shortcut:\n%s",
                 __FUNCTION__,
                 strVal.c_str(),
                 m_Keys[key].first,
                 errors.c_str());
        return false;
    }
    return true;
}

void ConfigManager::SetValue(Cfg key, bool val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(Cfg key, int val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(Cfg key, float val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(Cfg key, const std::string& val)
{
    m_Values[key] = val;
}


void ConfigManager::WriteConfig(bool forApp)
{
    bool error{false};
    const std::string configFile = forApp ? m_ApplicationIni : m_DllDirectory + "OpenXR-MotionCompensation.ini";
    for (const auto key : m_KeysToSave)
    {
        const auto& keyEntry = m_Keys.find(key);
        if (m_Keys.end() != keyEntry)
        {
            const auto& valueEntry = m_Values.find(key);
            if (m_Values.end() != valueEntry)
            {
                if (!WritePrivateProfileString(keyEntry->second.first.c_str(),
                                               keyEntry->second.second.c_str(),
                                               valueEntry->second.c_str(),
                                               configFile.c_str()) &&
                    2 != GetLastError())
                {
                    error = true;
                    DWORD err = GetLastError();
                    ErrorLog("%s: unable to write value %s into key %s to section %s in %s, error = %d : %s\n",
                             __FUNCTION__,
                             valueEntry->second.c_str(),
                             keyEntry->second.second.c_str(),
                             keyEntry->second.first.c_str(),
                             configFile.c_str(),
                             err,
                             LastErrorMsg(err).c_str());
                }
            }
            else
            {
                error = true;
                ErrorLog("%s: key not found in value map: %s:%s\n",
                         __FUNCTION__,
                         keyEntry->second.first.c_str(),
                         keyEntry->second.second.c_str());
            }
        }
        else
        {
            error = true;
            ErrorLog("%s: key not found in key map: %d\n", __FUNCTION__, key);
        }
    }
    Log("current configuration saved to %s\n", m_ApplicationIni.c_str());
    MessageBeep(!error ? MB_OK : MB_ICONERROR);
}

bool ConfigManager::InitDirectory()
{
    char path[MAX_PATH];
    HMODULE hm = NULL;

    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCSTR)&LastErrorMsg,
                          &hm) == 0)
    {
        DWORD err = GetLastError();
        ErrorLog("%s: GetModuleHandle failed, error = %d : %s\n", __FUNCTION__, err, LastErrorMsg(err).c_str());
        return false;
    }
    if (GetModuleFileName(hm, path, sizeof(path)) == 0)
    {
        DWORD err = GetLastError();
        ErrorLog("%s: GetModuleFileName failed, error = %d : %s\n", __FUNCTION__, err, LastErrorMsg(err).c_str());
        return false;
    }
    std::string dllName(path); 
    size_t lastBackSlash = dllName.find_last_of("\\/");
    if (std::string::npos == lastBackSlash)
    {
        ErrorLog("%s: DllName does not contain (back)slash: %s\n", __FUNCTION__, dllName.c_str());
        return false;
    }
    m_DllDirectory = dllName.substr(0,lastBackSlash + 1);
    return true;
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