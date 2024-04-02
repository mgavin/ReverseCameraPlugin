#include "ReverseCameraPlugin.h"
#define _USE_MATH_DEFINES
#include <math.h>

#include <format>
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/gfx/GfxDataTrainingWrapper.h"
#include "HookedEvents.h"

std::shared_ptr<CVarManagerWrapper> _globalCVarManager;

BAKKESMOD_PLUGIN(ReverseCameraPlugin,
                 "ReverseCameraPlugin",
                 "2.2.8",
                 /*UNUSED*/ NULL);

template<typename S, typename... Args>
void LOG(const S & format_str, Args &&... args) {
        _globalCVarManager->log(
                std::vformat(format_str, std::make_format_args(args...)));
}

/** main program start */

void ReverseCameraPlugin::onLoad() {
        _globalCVarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        HookedEvents::AddHookedEvent(
                "Function TAGame.GFxData_Settings_TA.SetInvertSwivelPitch",
                [this](std::string eventName) {
                        invert_swivel = !(gameWrapper->GetSettings()
                                                  .GetCameraSaveSettings()
                                                  .InvertSwivelPitch);
                });

        HookedEvents::AddHookedEvent(
                "Function TAGame.GFxData_Settings_TA.SetCameraAngle",
                [this](std::string eventName) {
                        CameraWrapper cam = gameWrapper->GetCamera();
                        if (!cam.IsNull()) {
                                angle_setting = std::fabs(
                                        cam.GetCameraSettings().Pitch);
                        }
                });

        cvarManager
                ->registerCvar(
                        "ReverseCamera_Enabled",
                        std::to_string(false),
                        "Dictates whether the Reverse Camera plugin is enabled (true/false)")
                .addOnValueChanged([this](std::string oldValue,
                                          CVarWrapper newValue) {
                        enabled = newValue.getBoolValue();
                        if (enabled) {
                                HookedEvents::AddHookedEvent(
                                        "Function Engine.GameViewportClient.Tick",
                                        bind(&ReverseCameraPlugin::onTick,
                                             this,
                                             std::placeholders::_1));
                                HookedEvents::AddHookedEvent(
                                        "Function ProjectX.Camera_X.ClampPOV",
                                        std::bind(&ReverseCameraPlugin::
                                                          HandleValues,
                                                  this));
                        } else {
                                HookedEvents::RemoveHook(
                                        "Function Engine.GameViewportClient.Tick");
                                HookedEvents::RemoveHook(
                                        "Function ProjectX.Camera_X.ClampPOV");
                        }
                });

        cvarManager
                ->registerCvar(
                        "ReverseCamera_Cam_Angle_Compensate",
                        std::to_string(false),
                        "Flag for plugin to compensate the pitch for camera's angle")
                .addOnValueChanged(
                        [this](std::string oldValue, CVarWrapper newValue) {
                                compensate_for_angle = newValue.getBoolValue();

                                // INSURANCE
                                CameraWrapper cam = gameWrapper->GetCamera();
                                if (!cam.IsNull()) {
                                        angle_setting = std::fabs(
                                                cam.GetCameraSettings().Pitch);
                                }
                        });

        /* TODOOOOOOOOOOOOOOOOOOO: HANDLING DIFFERENT
         * KEYBINDS? */
        right_stick_fnameindex =
                gameWrapper->GetFNameIndexByString("XboxTypeS_RightThumbStick");

        invert_swivel = gameWrapper->GetSettings()
                                .GetCameraSaveSettings()
                                .InvertSwivelPitch;
        CameraWrapper cam = gameWrapper->GetCamera();
        if (!cam.IsNull()) {
                angle_setting = std::fabs(cam.GetCameraSettings().Pitch);
        }
}

void ReverseCameraPlugin::onUnload() {
        // destroy things
        // dont throw here
}

/*
 *
 * TODO: ADD HANDLING FOR DIFFERENT KEYBINDS
 *
 */
void ReverseCameraPlugin::onTick(std::string eventName) {
        PlayerControllerWrapper pcw = gameWrapper->GetPlayerController();
        CameraWrapper           cam = gameWrapper->GetCamera();
        ServerWrapper           sw  = GetCurrentGameState();

        if (!pcw.IsNull() && !cam.IsNull() && !sw.IsNull()) {
                if (pcw.GetbUsingGamepad()) {
                        // is right stick pressed?
                        //  if so = RearCam, otherwise Cleanup
                        const bool stick_is_pressed = gameWrapper->IsKeyPressed(
                                right_stick_fnameindex);
                        if (stick_is_pressed && !already_pressed) {
                                in_reverse_cam = true;
                                cam.SetSwivelDieRate(0.0f);
                                already_pressed = true;
                        } else if (!stick_is_pressed && already_pressed) {
                                Rotator rot =
                                        cam.GetDesiredSwivel(rsticky, rstickx);
                                cam.SetCurrentSwivel(rot);
                                cam.UpdateSwivel(0.0f);
                                cam.SetSwivelDieRate(2.0f);
                                in_reverse_cam  = false;
                                already_pressed = false;
                        }
                        const int PLAYER_NUMBER = 0;
                        std::memset(
                                &xboxControllerState, 0, sizeof(XINPUT_STATE));
                        DWORD result = XInputGetState(PLAYER_NUMBER,
                                                      &xboxControllerState);
                        if (result ==
                            ERROR_DEVICE_NOT_CONNECTED) {  // controller
                                                           // isn't
                                                           // connected
                                rstickx = rsticky = 0.0f;
                        } else {
                                rstickx = static_cast<float>(
                                                  xboxControllerState.Gamepad
                                                          .sThumbRX) /
                                          SHRT_MAX;
                                rsticky = -1.0f *
                                          static_cast<float>(
                                                  xboxControllerState.Gamepad
                                                          .sThumbRY) /
                                          SHRT_MAX;
                        }
                } else {
                        // it's not a gamepad... it might be a keyboard
                        // and mouse
                }
        }
}

void ReverseCameraPlugin::HandleValues() const {
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        CameraWrapper cam    = gameWrapper->GetCamera();
        if (server.IsNull() || cam.IsNull()) {
                return;
        }

        if (enabled && in_reverse_cam) {
                Rotator rot = cam.GetDesiredSwivel(
                        ((invert_swivel) ? -1 : 1) * rsticky, rstickx);
                Rotator rtot  = rot + Rotator{32767, 0, 32767};
                Rotator rtotc = rtot;
                if (compensate_for_angle) {
                        rtot += {180 * 2 * static_cast<int>(angle_setting),
                                 0,
                                 0};
                        LOG("ANGLE SETTING: {}, CALCULATION: {}, PITCH START: {}, PITCH END {}",
                            static_cast<int>(angle_setting),
                            static_cast<int>(angle_setting) * 180 * 2,
                            rtotc.Pitch,
                            rtot.Pitch);
                }

                cam.SetCurrentSwivel(rtot);
                cam.UpdateSwivel(0);
        }
}

// declared inline because
// https://stackoverflow.com/questions/46163607/avoid-memory-allocation-with-stdfunction-and-member-function
// led me to believe crashing with allocation errors was because of
// ServerWrapper's instantiation with the memory_address so if it
// happens inline instead, in the anonymous function / bound member
// function, instead of being dynamically allocated and returned, it
// would be in the function... something like that

// which may or may not be the case because of fucking variables and stuff.
// (I got paranoid because gw->LocalCar() may have been null at one point.
// I'm not even entirely exactly sure what `inline` does.
inline ServerWrapper ReverseCameraPlugin::GetCurrentGameState() const {
        if (gameWrapper->IsInReplay())
                return gameWrapper->GetGameEventAsReplay().memory_address;
        else if (gameWrapper->IsInOnlineGame())
                return gameWrapper->GetOnlineGame();
        else
                return gameWrapper->GetGameEventAsServer();
}

void ReverseCameraPlugin::RenderSettings() {
        // for imgui plugin window
        CVarWrapper enabled_cvar =
                cvarManager->getCvar("ReverseCamera_Enabled");
        bool enable = enabled_cvar.getBoolValue();
        if (ImGui::Checkbox("ENABLE?????????????", &enable)) {
                enabled_cvar.setValue(enable);
        }

        CVarWrapper compensate_cvar =
                cvarManager->getCvar("ReverseCamera_Cam_Angle_Compensate");
        bool compensate = compensate_cvar.getBoolValue();
        if (ImGui::Checkbox("Compensate pitch for camera angle?",
                            &compensate)) {
                compensate_cvar.setValue(compensate);
        }
        ImGui::NewLine();
        ImGui::Separator();
        ImGui::NewLine();
        ImGui::Text("MAYBE SOME FUCKING TEXT CAN GO HERE EXPLAINING THINGS");
}

// Don't call this yourself, BM will call this function with a pointer
// to the current ImGui context
void ReverseCameraPlugin::SetImGuiContext(uintptr_t ctx) {
        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

std::string ReverseCameraPlugin::GetPluginName() {
        return "Reverse Camera Plugin";
}
