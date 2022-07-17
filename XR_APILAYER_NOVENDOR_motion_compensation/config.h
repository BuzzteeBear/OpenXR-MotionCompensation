#pragma once
enum ConfigKey
{
    CfgTrackerType = 0,
    CfgTrackerParam,
    CfgTransStrength,
    CfgTransOrder,
    CfgRotStrength,
    CfgRotOrder,
    CfgKeyActivate,
    CfgKeyCenter,
    CfgKeyTransInc,
    CfgKeyTransDec,
    CfgKeyRotInc,
    CfgKeyRotDec,
    CfgKeySaveConfig,
};

class ConfigManager
{
  public:
    bool Init(const std::string& application);
    bool GetBool(ConfigKey key, bool& val);
    bool GetInt(ConfigKey key, int& val);
    bool GetDouble(ConfigKey key, double& val);
    bool GetString(ConfigKey key, std::string& val);
    bool GetShortcut(ConfigKey key, std::set<std::string>& val);

    void SetValue(ConfigKey key, bool& val);
    void SetValue(ConfigKey key, int& val);
    void SetValue(ConfigKey key, double& val);
    void SetValue(ConfigKey key, const std::string& val);

    void WriteConfig();

  private:
    bool InitDirectory();

    std::string m_DllDirectory;
    // needs to include all values of enum ConfigKey
    std::map<ConfigKey, std::pair<std::string, std::string>> m_KeyMapping{
        {CfgTrackerType, {"tracker", "type"}},
        {CfgTrackerParam, {"tracker", "parameter"}},
        {CfgTransStrength, {"translation_filter", "strength"}},
        {CfgTransOrder, {"translation_filter", "order"}},
        {CfgRotStrength, {"rotation_filter", "strength"}},
        {CfgRotOrder, {"rotation_filter", "order"}},
        {CfgKeyActivate, {"shortcuts", "activate"}},
        {CfgKeyCenter, {"shortcuts", "center"}},
        {CfgKeyTransInc, {"shortcuts", "translation_increase"}},
        {CfgKeyTransDec, {"shortcuts", "translation_decrease"}},
        {CfgKeyRotInc, {"shortcuts", "rotation_increase"}},
        {CfgKeyRotDec, {"shortcuts", "rotation_decrease"}},
        {CfgKeySaveConfig, {"shortcuts", "save_config"}}};
    std::map<ConfigKey, std::string> m_Values{};
};