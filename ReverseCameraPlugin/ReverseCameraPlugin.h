#pragma once
#include <Windows.h>
#include <Xinput.h>
#include <string>
#include "bakkesmod/imgui/imgui.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

class ReverseCameraPlugin :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow {
private:
        inline ServerWrapper GetCurrentGameState() const;
        inline int           fucking_fuckit(int num) const;

        int   right_stick_fnameindex = 0;
        bool  in_reverse_cam         = false;
        bool  is_on_wall             = false;
        float rstickx = 0.0f, rsticky = 0.0f;
        bool  already_pressed = false;
        bool  enabled         = true;

        const float  CALCULATED_REVERSE_FACTOR = 1.456f;
        XINPUT_STATE xboxControllerState       = {0};

public:
        void onLoad() override;
        void onUnload() override;

        void        RenderSettings() override;
        std::string GetPluginName() override;
        void        SetImGuiContext(uintptr_t ctx) override;

        void HandleValues() const;
        void onTick(std::string);
};
