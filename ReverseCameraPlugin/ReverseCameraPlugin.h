#pragma once
#include <string>
#include "bakkesmod/imgui/imgui.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

class ReverseCameraPlugin :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow {
private:
        ServerWrapper GetCurrentGameState() const;

        Vector project_v1_on_v2(Vector vec1, Vector vec2);
        Quat AngleAxisRotation(float angle, Vector axis);
        Quat GetQuatFromMatrix(Vector fwd, Vector right, Vector up);
        Quat rotToQuat(Rotator rot);

        bool isInRearCam;
        bool isInBallCam;
        Vector FOCUS;
        Rotator ROTATION, SWIVEL;
        float DISTANCE, FOV;
        bool camcontrol_loaded;
        bool wasSupersonic = false;
        float interpStepsIntoSupersonic = 0;
        float interpStepsOutOfSupersonic = 0;

        bool cvars_nulled_out;

public:
        void onLoad() override;
        void onUnload() override;

        void RenderSettings() override;
        std::string GetPluginName() override;
        void SetImGuiContext(uintptr_t ctx) override;

        bool CheckCameraControlLoaded();

        // These functions are mostly copied from the "camera control template",
        // just to make things easier to copy
        bool CanCreateValues() const;
        bool IsCVarNull(std::string cvarName) const;
        void ResetCameraCVars();

        void HandleValues();

        bool enabled;
};