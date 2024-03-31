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
        ServerWrapper GetCurrentGameState() const;

        int   right_stick_fnameindex;
        bool  in_reverse_cam;
        bool  camcontrol_loaded;
        float rstickx, rsticky;
        int   prev_pitch;
        bool  captured_pitch;
        bool  enabled;

        XINPUT_STATE xboxControllerState;

public:
        void onLoad() override;
        void onUnload() override;

        void        RenderSettings() override;
        std::string GetPluginName() override;
        void        SetImGuiContext(uintptr_t ctx) override;

        void HandleValues() const;

        void onTick(std::string);
};