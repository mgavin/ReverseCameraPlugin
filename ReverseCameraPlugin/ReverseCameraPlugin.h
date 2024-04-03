#pragma once
#include <Windows.h>
#include <Xinput.h>
#include <string>
#include "bakkesmod/imgui/imgui.h"
#include "bakkesmod/imgui/imgui_internal.h"
#include "bakkesmod/imgui/imgui_searchablecombo.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/plugin/pluginwindow.h"

using std::string;
using std::vector;
class ReverseCameraPlugin :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow,
        public BakkesMod::Plugin::PluginWindow {
private:
        // https://raw.githubusercontent.com/esnar11/SkipReplay/master/source/SkipReplay.h
        std::string                    keybind;
        bool                           should_block_until_keybound = false;
        bool                           isWindowOpen                = false;
        bool                           isMinimized                 = false;
        int                            keyIndex                    = 0;
        std::unique_ptr<CVarWrapper>   keybindCvar;
        const std::vector<std::string> keys = {
                "F1",
                "F2",
                "F3",
                "F4",
                "F5",
                "F6",
                "F7",
                "F8",
                "F9",
                "F10",
                "F11",
                "F12",
                "A",
                "B",
                "C",
                "D",
                "E",
                "F",
                "G",
                "H",
                "I",
                "J",
                "K",
                "L",
                "M",
                "N",
                "O",
                "P",
                "Q",
                "R",
                "S",
                "T",
                "U",
                "V",
                "W",
                "X",
                "Y",
                "Z",
                "Escape",
                "Tab",
                "Tilde",
                "ScrollLock",
                "Pause",
                "one",
                "two",
                "three",
                "four",
                "five",
                "six",
                "seven",
                "eight",
                "nine",
                "zero",
                "Underscore",
                "Equals",
                "Backslash",
                "LeftBracket",
                "RightBracket",
                "Enter",
                "CapsLock",
                "Semicolon",
                "Quote",
                "LeftShift",
                "Comma",
                "Period",
                "Slash",
                "RightShift",
                "LeftControl",
                "LeftAlt",
                "SpaceBar",
                "RightAlt",
                "RightControl",
                "Left",
                "Up",
                "Down",
                "Right",
                "Home",
                "End",
                "Insert",
                "PageUp",
                "Delete",
                "PageDown",
                "NumLock",
                "Divide",
                "Multiply",
                "Subtract",
                "Add",
                "NumPadOne",
                "NumPadTwo",
                "NumPadThree",
                "NumPadFour",
                "NumPadFive",
                "NumPadSix",
                "NumPadSeven",
                "NumPadEight",
                "NumPadNine",
                "NumPadZero",
                "Decimal",
                "LeftMouseButton",
                "RightMouseButton",
                "ThumbMouseButton",
                "ThumbMouseButton2",
                "MouseScrollUp",
                "MouseScrollDown",
                "MouseX",
                "MouseY",
                "XboxTypeS_LeftThumbStick",
                "XboxTypeS_RightThumbStick",
                "XboxTypeS_DPad_Up",
                "XboxTypeS_DPad_Left",
                "XboxTypeS_DPad_Right",
                "XboxTypeS_DPad_Down",
                "XboxTypeS_Back",
                "XboxTypeS_Start",
                "XboxTypeS_Y",
                "XboxTypeS_X",
                "XboxTypeS_B",
                "XboxTypeS_A",
                "XboxTypeS_LeftShoulder",
                "XboxTypeS_RightShoulder",
                "XboxTypeS_LeftTrigger",
                "XboxTypeS_RightTrigger",
                "XboxTypeS_LeftTriggerAxis",
                "XboxTypeS_RightTriggerAxis",
                "XboxTypeS_LeftX",
                "XboxTypeS_LeftY",
                "XboxTypeS_RightX",
                "XboxTypeS_RightY"};
        std::vector<std::string>::const_iterator keysIt;

        struct keypress_t {
                int ControllerID;
                struct {
                        int Index;
                        int Number;
                } key;
                unsigned char EventType;
                float         AmountDepressed;
                unsigned int  bGamepad;
                unsigned int  ReturnValue;
        };

        void OnBind(std::string key);
        void OnKeyPressed(ActorWrapper aw, void * params, std::string eventName);

        bool in_goal_replay = false;

        int   right_stick_fnameindex = 0;
        bool  in_reverse_cam         = false;
        bool  invert_swivel          = false;
        float angle_setting          = 0.0;
        float rstickx = 0.0f, rsticky = 0.0f;
        bool  already_pressed      = false;
        bool  enabled              = true;
        bool  compensate_for_angle = false;

        const float  CALCULATED_REVERSE_FACTOR = 1.456f;
        XINPUT_STATE xboxControllerState       = {0};

public:
        void onLoad() override;
        void onUnload() override;

        void        RenderSettings() override;
        std::string GetPluginName() override;
        void        Render() override;
        void        SetImGuiContext(uintptr_t ctx) override;

        void        HandleValues() const;
        void        onTick(std::string);
        void        OnOpen() override;
        void        OnClose() override;
        std::string GetMenuName() override;
        std::string GetMenuTitle() override;
        bool        IsActiveOverlay() override;
        bool        ShouldBlockInput() override;
};