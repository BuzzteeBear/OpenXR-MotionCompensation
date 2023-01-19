// Copyright(c) 2022 Sebastian Veith

#pragma once

enum class Cfg
{
    Enabled = 0,
    PhysicalEnabled,
    OverlayEnabled,
    TrackerType,
    TrackerSide,
    TrackerTimeout,
    TrackerCheck,
    TrackerOffsetForward,
    TrackerOffsetDown,
    TrackerOffsetRight,
    UseYawGeOffset,
    CorX,
    CorY,
    CorZ,
    CorA,
    CorB,
    CorC,
    CorD,
    UseCorPos,
    TransStrength,
    TransOrder,
    RotStrength,
    RotOrder,
    CacheUseEye,
    CacheTolerance,
    KeyActivate,
    KeyCenter,
    KeyTransInc,
    KeyTransDec,
    KeyRotInc,
    KeyRotDec,
    KeyOffForward,
    KeyOffBack,
    KeyOffUp,
    KeyOffDown,
    KeyOffRight,
    KeyOffLeft,
    KeyRotRight,
    KeyRotLeft,
    KeyOverlay,
    KeyCache,
    KeySaveConfig,
    KeySaveConfigApp,
    KeyReloadConfig,
    KeyDebugCor,
    TestRotation
};

class ConfigManager
{
  public:
    bool Init(const std::string& application);
    bool GetBool(Cfg key, bool& val);
    bool GetInt(Cfg key, int& val);
    bool GetFloat(Cfg key, float& val);
    bool GetString(Cfg key, std::string& val);
    bool GetShortcut(Cfg key, std::set<int>& val);
    std::string GetControllerSide();

    void SetValue(Cfg key, bool val);
    void SetValue(Cfg key, int val);
    void SetValue(Cfg key, float val);
    void SetValue(Cfg key, const std::string& val);

    void WriteConfig(bool forApp);

  private:

    std::string m_ApplicationIni;

    // needs to include all values of enum ConfigKey
    std::map<Cfg, std::pair<std::string, std::string>> m_Keys{
        {Cfg::Enabled, {"startup", "enabled"}},
        {Cfg::PhysicalEnabled, {"startup", "physical_enabled"}},
        {Cfg::OverlayEnabled, {"startup", "overlay_enabled"}},

        {Cfg::TrackerType, {"tracker", "type"}},
        {Cfg::TrackerSide, {"tracker", "side"}},
        
        {Cfg::TrackerTimeout, {"tracker", "connection_timeout"}},
        {Cfg::TrackerCheck, {"tracker", "connection_check"}},
        
        {Cfg::TrackerOffsetForward, {"tracker", "offset_forward"}},
        {Cfg::TrackerOffsetDown, {"tracker", "offset_down"}},
        {Cfg::TrackerOffsetRight, {"tracker", "offset_right"}},

        {Cfg::UseYawGeOffset, {"tracker", "use_yaw_ge_offset"}},

        {Cfg::CorX, {"tracker", "cor_x"}},
        {Cfg::CorY, {"tracker", "cor_y"}},
        {Cfg::CorZ, {"tracker", "cor_z"}},
        {Cfg::CorA, {"tracker", "cor_a"}},
        {Cfg::CorB, {"tracker", "cor_b"}},
        {Cfg::CorC, {"tracker", "cor_c"}},
        {Cfg::CorD, {"tracker", "cor_d"}},
        {Cfg::UseCorPos, {"tracker", "use_cor_pos"}},

        {Cfg::TransStrength, {"translation_filter", "strength"}},
        {Cfg::TransOrder, {"translation_filter", "order"}},
        {Cfg::RotStrength, {"rotation_filter", "strength"}},
        {Cfg::RotOrder, {"rotation_filter", "order"}},

        {Cfg::CacheUseEye, {"cache", "use_eye_cache"}},
        {Cfg::CacheTolerance, {"cache", "tolerance"}},

        {Cfg::KeyActivate, {"shortcuts", "activate"}},
        {Cfg::KeyCenter, {"shortcuts", "center"}},

        {Cfg::KeyTransInc, {"shortcuts", "translation_increase"}},
        {Cfg::KeyTransDec, {"shortcuts", "translation_decrease"}},
        {Cfg::KeyRotInc, {"shortcuts", "rotation_increase"}},
        {Cfg::KeyRotDec, {"shortcuts", "rotation_decrease"}},

        {Cfg::KeyOffForward, {"shortcuts", "offset_forward"}},
        {Cfg::KeyOffBack, {"shortcuts", "offset_back"}},
        {Cfg::KeyOffUp, {"shortcuts", "offset_up"}},
        {Cfg::KeyOffDown, {"shortcuts", "offset_down"}},
        {Cfg::KeyOffRight, {"shortcuts", "offset_right"}},
        {Cfg::KeyOffLeft, {"shortcuts", "offset_left"}},

        {Cfg::KeyRotRight, {"shortcuts", "rotate_right"}},
        {Cfg::KeyRotLeft, {"shortcuts", "rotate_left"}},

        {Cfg::KeyOverlay, {"shortcuts", "toggle_overlay"}},

        {Cfg::KeyCache, {"shortcuts", "toggle_cache"}},

        {Cfg::KeyDebugCor, {"shortcuts", "cor_debug_mode"}},

        {Cfg::KeySaveConfig, {"shortcuts", "save_config"}},
        {Cfg::KeySaveConfigApp, {"shortcuts", "save_config_app"}},
        {Cfg::KeyReloadConfig, {"shortcuts", "reload_config"}},

        {Cfg::TestRotation, {"debug", "testrotation"}}};

    std::set<Cfg> m_KeysToSave{Cfg::TransStrength,
                               Cfg::RotStrength,
                               Cfg::TrackerOffsetForward,
                               Cfg::TrackerOffsetDown,
                               Cfg::TrackerOffsetRight,
                               Cfg::CacheUseEye,
                               Cfg::CorX,
                               Cfg::CorY,
                               Cfg::CorZ,
                               Cfg::CorA,
                               Cfg::CorB,
                               Cfg::CorC,
                               Cfg::CorD};

    std::map<std::string, int> m_ShortCuts{{"BACK", VK_BACK},
                                           {"TAB", VK_TAB},
                                           {"CLR", VK_CLEAR},
                                           {"RETURN", VK_RETURN},
                                           {"SHIFT", VK_SHIFT},
                                           {"CTRL", VK_CONTROL},
                                           {"ALT", VK_MENU},
                                           {"PAUSE", VK_PAUSE},
                                           {"CAPS", VK_CAPITAL},
                                           {"ESC", VK_ESCAPE},
                                           {"SPACE", VK_SPACE},
                                           {"PGUP", VK_PRIOR},
                                           {"PGDN", VK_NEXT},
                                           {"END", VK_END},
                                           {"HOME", VK_HOME},
                                           {"LEFT", VK_LEFT},
                                           {"UP", VK_UP},
                                           {"RIGHT", VK_RIGHT},
                                           {"DOWN", VK_DOWN},
                                           {"SELECT", VK_SELECT},
                                           {"PRINT", VK_PRINT},
                                           {"PRTSC", VK_SNAPSHOT},
                                           {"EXEC", VK_EXECUTE},
                                           {"INS", VK_INSERT},
                                           {"DEL", VK_DELETE},
                                           {"HELP", VK_HELP},
                                           {"0", 0x30},
                                           {"1", 0x31},
                                           {"2", 0x32},
                                           {"3", 0x33},
                                           {"4", 0x34},
                                           {"5", 0x35},
                                           {"6", 0x36},
                                           {"7", 0x37},
                                           {"8", 0x38},
                                           {"9", 0x39},
                                           {"A", 0x41},
                                           {"B", 0x42},
                                           {"C", 0x43},
                                           {"D", 0x44},
                                           {"E", 0x45},
                                           {"F", 0x46},
                                           {"G", 0x47},
                                           {"H", 0x48},
                                           {"I", 0x49},
                                           {"J", 0x4A},
                                           {"K", 0x4B},
                                           {"L", 0x4C},
                                           {"M", 0x4D},
                                           {"N", 0x4E},
                                           {"O", 0x4F},
                                           {"P", 0x50},
                                           {"Q", 0x51},
                                           {"R", 0x52},
                                           {"S", 0x53},
                                           {"T", 0x54},
                                           {"U", 0x55},
                                           {"V", 0x56},
                                           {"W", 0x57},
                                           {"X", 0x58},
                                           {"Y", 0x59},
                                           {"Z", 0x5A},
                                           {"NUM0", VK_NUMPAD0},
                                           {"NUM1", VK_NUMPAD1},
                                           {"NUM2", VK_NUMPAD2},
                                           {"NUM3", VK_NUMPAD3},
                                           {"NUM4", VK_NUMPAD4},
                                           {"NUM5", VK_NUMPAD5},
                                           {"NUM6", VK_NUMPAD6},
                                           {"NUM7", VK_NUMPAD7},
                                           {"NUM8", VK_NUMPAD8},
                                           {"NUM9", VK_NUMPAD9},
                                           {"NUMMULTIPLY", VK_MULTIPLY},
                                           {"NUMADD", VK_ADD},
                                           {"NUMSEPARATOR", VK_SEPARATOR},
                                           {"NUMSUBTRACT", VK_SUBTRACT},
                                           {"NUMDECIMAL", VK_DECIMAL},
                                           {"NUMDIVIDE", VK_DIVIDE},
                                           {"F1", VK_F1},
                                           {"F2", VK_F2},
                                           {"F3", VK_F3},
                                           {"F4", VK_F4},
                                           {"F5", VK_F5},
                                           {"F6", VK_F6},
                                           {"F7", VK_F7},
                                           {"F8", VK_F8},
                                           {"F9", VK_F9},
                                           {"F10", VK_F10},
                                           {"F11", VK_F11},
                                           {"F12", VK_F12},
                                           {"NUMLOCK", VK_NUMLOCK},
                                           {"SCROLL", VK_SCROLL},
                                           {"LSHIFT", VK_LSHIFT},
                                           {"RSHIFT", VK_RSHIFT},
                                           {"LCTRL", VK_LCONTROL},
                                           {"RCTRL", VK_RCONTROL},
                                           {"LALT", VK_LMENU},
                                           {"RALT", VK_RMENU},
                                           {"SEMICOLON", VK_OEM_1},
                                           {"PLUS", VK_OEM_PLUS},
                                           {"COMMA", VK_OEM_COMMA},
                                           {"MINUS", VK_OEM_MINUS},
                                           {"PERIOD", VK_OEM_PERIOD},
                                           {"SLASH", VK_OEM_2},
                                           {"BACKQUOTE", VK_OEM_3},
                                           {"OPENBRACKET", VK_OEM_4},
                                           {"BACKSLASH", VK_OEM_5},
                                           {"CLOSEBRACKET", VK_OEM_6},
                                           {"QUOTE", VK_OEM_7},
                                           {"GAMEPAD_A", VK_GAMEPAD_A},
                                           {"GAMEPAD_B", VK_GAMEPAD_B},
                                           {"GAMEPAD_X", VK_GAMEPAD_X},
                                           {"GAMEPAD_Y", VK_GAMEPAD_Y},
                                           {"GAMEPAD_RIGHT_SHOULDER", VK_GAMEPAD_RIGHT_SHOULDER},
                                           {"GAMEPAD_LEFT_SHOULDER", VK_GAMEPAD_LEFT_SHOULDER},
                                           {"GAMEPAD_LEFT_TRIGGER", VK_GAMEPAD_LEFT_TRIGGER},
                                           {"GAMEPAD_RIGHT_TRIGGER", VK_GAMEPAD_RIGHT_TRIGGER},
                                           {"GAMEPAD_DPAD_UP", VK_GAMEPAD_DPAD_UP},
                                           {"GAMEPAD_DPAD_DOWN", VK_GAMEPAD_DPAD_DOWN},
                                           {"GAMEPAD_DPAD_LEFT", VK_GAMEPAD_DPAD_LEFT},
                                           {"GAMEPAD_DPAD_RIGHT", VK_GAMEPAD_DPAD_RIGHT},
                                           {"GAMEPAD_START", VK_GAMEPAD_MENU},
                                           {"GAMEPAD_VIEW", VK_GAMEPAD_VIEW},
                                           {"GAMEPAD_LEFT_THUMBSTICK_BUTTON", VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON},
                                           {"GAMEPAD_RIGHT_THUMBSTICK_BUTTON", VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON},
                                           {"GAMEPAD_LEFT_THUMBSTICK_UP", VK_GAMEPAD_LEFT_THUMBSTICK_UP},
                                           {"GAMEPAD_LEFT_THUMBSTICK_DOWN", VK_GAMEPAD_LEFT_THUMBSTICK_DOWN},
                                           {"GAMEPAD_LEFT_THUMBSTICK_RIGHT", VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT},
                                           {"GAMEPAD_LEFT_THUMBSTICK_LEFT", VK_GAMEPAD_LEFT_THUMBSTICK_LEFT},
                                           {"GAMEPAD_RIGHT_THUMBSTICK_UP", VK_GAMEPAD_RIGHT_THUMBSTICK_UP},
                                           {"GAMEPAD_RIGHT_THUMBSTICK_DOWN", VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN},
                                           {"GAMEPAD_RIGHT_THUMBSTICK_RIGHT", VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT},
                                           {"GAMEPAD_RIGHT_THUMBSTICK_LEFT", VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT}};
    std::map<Cfg, std::string> m_Values{};
};
// Singleton accessor.
ConfigManager* GetConfig();