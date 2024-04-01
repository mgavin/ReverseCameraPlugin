#include "ReverseCameraPlugin.h"
#define _USE_MATH_DEFINES
#include <math.h>

#include <format>
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/gfx/GfxDataTrainingWrapper.h"
#include "HelperFunctions.h"
#include "HookedEvents.h"

std::shared_ptr<CVarManagerWrapper> _globalCVarManager;

BAKKESMOD_PLUGIN(ReverseCameraPlugin,
                 "ReverseCameraPlugin",
                 "2.0.7",
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

        /* TODOOOOOOOOOOOOOOOOOOO: HANDLING DIFFERENT KEYBINDS? */
        right_stick_fnameindex =
                gameWrapper->GetFNameIndexByString("XboxTypeS_RightThumbStick");

        cvarManager->registerNotifier(
                "rvcam_FUCK",
                [this](std::vector<std::string> args) {
                        float         neat = 0.0f;
                        CameraWrapper cam  = gameWrapper->GetCamera();
                        Rotator       tmpr;
                        for (int i = 0; i <= 20; ++i) {
                                tmpr = cam.GetDesiredSwivel(neat, neat);
                                LOG("cam GetDesiredSwivel({0:0.3f}, {0:0.3f}) = {{Pitch: {1:}, Yaw: {2:}}}",
                                    neat,
                                    tmpr.Pitch,
                                    tmpr.Yaw);
                                neat += 0.1f;
                        }

                        // calclulated best fit for YAW since "32767 is reliably
                        // REVERSED PITCH doesn't conform to the same formula
                        // for YAW, but it follows at a slower RATE THIS IS THE
                        // CALCULATED RATE FOR YAW, SO it should be "REVERSE "
                        // for pitch
                        const float MAGIC_NUMBER = 1.4563758;
                        tmpr = cam.GetDesiredSwivel(MAGIC_NUMBER, MAGIC_NUMBER);
                        LOG("cam GetDesiredSwivel({0:0.3f}, {0:0.3f}) = {{Pitch: {1:}, Yaw: {2:}}}",
                            MAGIC_NUMBER,
                            tmpr.Pitch,
                            tmpr.Yaw);
                        // THIS OUTPUTS
                        // cam GetDesiredSwivel(1.456, 1.456)
                        //                          = {Pitch: 8010, Yaw: 32767}
                        //  BEAUTIFUL
                },
                "desired deltas",
                NULL);
}

void ReverseCameraPlugin::onUnload() {
        // destroy things
        // dont throw here
}

/*
 *
 * TODO: ADD HANDLING FOR DIFFERENT KEYBINDS
 *
 * THERES A BUG WITH THE X-AXIS! FUCK!
 *
 */
void ReverseCameraPlugin::onTick(std::string eventName) {
        PlayerControllerWrapper pcw = gameWrapper->GetPlayerController();
        CameraWrapper           cam = gameWrapper->GetCamera();
        ServerWrapper           sw  = GetCurrentGameState();

        // if (gameWrapper->IsInReplay()) {
        //         sw = gameWrapper->GetGameEventAsReplay().memory_address;
        // } else if (gameWrapper->IsInOnlineGame()) {
        //         sw = gameWrapper->GetOnlineGame();
        // } else {
        //         sw = gameWrapper->GetGameEventAsServer();
        // }

        if (!pcw.IsNull() && !cam.IsNull() && !sw.IsNull()) {
                if (pcw.GetbUsingGamepad()) {
                        // is right stick pressed? if so = RearCam, otherwise
                        // Cleanup
                        is_on_wall = gameWrapper->GetLocalCar().IsOnWall();

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
                        // it's not a gamepad... it might be a keyboard and
                        // mouse
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
                // cvarManager->executeCommand(
                //         "bm_playground_camera_print_getters", false);
                Rotator rot = cam.GetDesiredSwivel(rsticky, rstickx);
                cam.SetCurrentSwivel(
                        rot +
                        Rotator{(is_on_wall) ? REVERSE_ROT_DELTA.Pitch : 0,
                                REVERSE_ROT_DELTA.Yaw,
                                0});
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
