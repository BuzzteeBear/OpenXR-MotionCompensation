#include "pch.h"

#include "config.h"
#include "utility.h"
#include <log.h>
#include <util.h>

using namespace motion_compensation_layer::log;
using namespace utilities;

bool ConfigManager::Init(const std::string& application)
{
    if (!InitDirectory())
    {
        return false;
    }
    // create application config file if not existing
    const std::string applicationIni(m_DllDirectory + application + ".ini");
    if ((_access(applicationIni.c_str(), 0)) == -1)
    {
        if (!WritePrivateProfileString("placeholder", "created", "1", applicationIni.c_str()) && 2 != GetLastError())
        {
            int err = GetLastError();
            ErrorLog("ConfigManager::Init: unable to create %s, error = %d : %s\n",
                     applicationIni.c_str(),
                     err,
                     LastErrorMsg(err).c_str());
        }
    }
    const std::string coreIni(m_DllDirectory + "OpenXR-MotionCompensation.ini");
    if ((_access((m_DllDirectory + application + ".ini").c_str(), 0)) != -1)
    {
        std::string errors;
        for (const auto& entry : m_KeyMapping)
        {
            char buffer[1024];
            int read{0};
            if (0 < GetPrivateProfileString(entry.second.first.c_str(),
                                            entry.second.second.c_str(),
                                            NULL,
                                            buffer,
                                            1023,
                                            applicationIni.c_str()))
            {
                m_Values.insert({entry.first, buffer});
            }
            else if ((0 < GetPrivateProfileString(entry.second.first.c_str(),
                                                  entry.second.second.c_str(),
                                                  NULL,
                                                  buffer,
                                                  1023,
                                                  coreIni.c_str())))
            {
                m_Values.insert({entry.first, buffer});
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
            ErrorLog("ConfigManager::Init: unable to read configuration:\n%s", errors.c_str());
            return false;
        }
    }
    else
    {
        ErrorLog("ConfigManager::Init: unable to find config file %s%s.ini\n",
                 m_DllDirectory.c_str(),
                 application.c_str());
        return false;
    }
    
    return true;
}

bool ConfigManager::GetBool(ConfigKey key, bool& val)
{
    std::string strVal;
    if (!GetString(key, strVal))
    {
        try
        {
            val = stoi(strVal);
        }
        catch (std::exception e)
        {
            ErrorLog("ConfigManager::GetBool: unable to convert value (%s) for key (%s) to integer: %s\n",
                     strVal.c_str(),
                     m_KeyMapping[key].first,
                     e.what());
            return false;
        } 
    }
    return true;
}
bool ConfigManager::GetInt(ConfigKey key, int& val)
{
    std::string strVal;
    if (!GetString(key, strVal))
    {
        try
        {
            val = stoi(strVal);
        }
        catch (std::exception e)
        {
            ErrorLog("ConfigManager::GetInt: unable to convert value (%s) for key (%s) to integer: %s\n",
                     strVal.c_str(),
                     m_KeyMapping[key].first,
                     e.what());
            return false;
        } 
    }
    return true;
}
bool ConfigManager::GetDouble(ConfigKey key, double& val)
{
    std::string strVal;
    if (!GetString(key, strVal))
    {
        try
        {
            val = stod(strVal);
        }
        catch (std::exception e)
        {
            ErrorLog("ConfigManager::GetDouble: unable to convert value (%s) for key (%s) to double: %s\n",
                     strVal.c_str(),
                     m_KeyMapping[key].first,
                     e.what());
            return false;
        } 
    }
    return true;
}
bool ConfigManager::GetString(ConfigKey key, std::string& val)
{
    const auto it = m_Values.find(key);
    if (m_Values.end() == it)
    {
        return false;
    }
    val = it->second;
    return true;
}
bool ConfigManager::GetShortcut(ConfigKey key, std::set<std::string>& val)
{
    // TODO: implement
    return false;
}

void ConfigManager::SetValue(ConfigKey key, bool& val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(ConfigKey key, int& val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(ConfigKey key, double& val)
{
    SetValue(key, std::to_string(val));
}
void ConfigManager::SetValue(ConfigKey key, const std::string& val)
{
    m_Values[key] = val;
}

bool ConfigManager::InitDirectory()
{
    char path[MAX_PATH];
    HMODULE hm = NULL;

    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCSTR)&LastErrorMsg,
                          &hm) == 0)
    {
        int err = GetLastError();
        ErrorLog("ConfigManager::InitDirectory: GetModuleHandle failed, error = %d : %s\n",
                 err,
                 LastErrorMsg(err).c_str());
        return false;
    }
    if (GetModuleFileName(hm, path, sizeof(path)) == 0)
    {
        int err = GetLastError();
        ErrorLog("ConfigManager::InitDirectory: GetModuleFileName failed, error = %d : %s\n",
                 err,
                 LastErrorMsg(err).c_str());
        return false;
    }
    std::string dllName(path); 
    size_t lastBackSlash = dllName.find_last_of("\\/");
    if (std::string::npos == lastBackSlash)
    {
        ErrorLog("ConfigManager::InitDirectory: DllName does not contain (back)slash: %s\n", dllName.c_str());
        return false;
    }
    m_DllDirectory = dllName.substr(0,lastBackSlash + 1);
    return true;
}