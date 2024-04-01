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

        for (int i = 0; i < 16384; ++i) {
                LOG("fucking fuck it: i: {}, i + 20000 = {}",
                    i,
                    fucking_fuckit(20000 + i));
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
                        // is right stick pressed?
                        //  if so = RearCam, otherwise Cleanup
                        CarWrapper cw = gameWrapper->GetLocalCar();
                        if (!cw.IsNull()) {
                                is_on_wall = cw.IsOnWall();
                        }

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

inline int ReverseCameraPlugin::fucking_fuckit(int num) const {
        // all operations will fit inside a signed i32
        const int RANGE_MAX = 16383;
        const int RANGE_MIN = -16384;
        if (num > RANGE_MAX) {
                num += RANGE_MAX;
                num %= abs(RANGE_MAX) + abs(RANGE_MIN);
                return RANGE_MIN + num;
        }

        if (num < RANGE_MIN) {
                num -= RANGE_MIN;
                num  = (abs(num) % (abs(RANGE_MAX) + abs(RANGE_MIN)));
                return RANGE_MAX - num;
        }

        return num;
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
                /*
                 *
                 *
                 *
                 *
                 *  THE PROPBELM IS:::::::::::::::::::::::::::::::::::::
                 * I NEED TO CALCULATE IT, THEN CALCULATE IT PAST THE POINT OF
                 * INVALID RANGE [-32768,-16384]U[16384,32767] LIKE "FIX" THE
                 * ROTATION VALUE BUT I CANT TOUCH THE FUCKING ROTATION I NEED
                 * TO PRE-CALCULATE IT ON THE SWIVEL
                 *
                 * LIKE, WILL THE ROTATION + SWIVEL BE OUTSIDE OF THE RANGE? OR
                 * LIKE, REVERSE ENGINEER "ROTATION POTENTIAL" FROM
                 * GETDESIREDSWIVEL .... UGHHHHHHHHHHHHH
                 *
                 *
                 *
                 *
                 *
                 *
                 *
                 *
                 *
                 *
                 */
                Rotator rot = cam.GetDesiredSwivel(rsticky, rstickx);
                Rotator reverse_rot1 =
                        cam.GetDesiredSwivel(CALCULATED_REVERSE_FACTOR * 1.0f,
                                             CALCULATED_REVERSE_FACTOR * 1.0f);
                Rotator reverse_rot2 =
                        cam.GetDesiredSwivel(CALCULATED_REVERSE_FACTOR * -1.0f,
                                             CALCULATED_REVERSE_FACTOR * -1.0f);
                LOG("reverse_rot1: {{Pitch: {}, YAW: {}, Roll: {}}}",
                    reverse_rot1.Pitch,
                    reverse_rot1.Yaw,
                    reverse_rot1.Roll);
                LOG("reverse_rot2: {{Pitch: {}, YAW: {}, Roll: {}}}",
                    reverse_rot2.Pitch,
                    reverse_rot2.Yaw,
                    reverse_rot2.Roll);

                Rotator rtot =
                        rot +
                        Rotator{(is_on_wall) ? reverse_rot1.Pitch + 32767 : 0,
                                reverse_rot1.Yaw,
                                0};
                LOG("rotot pitch: {}", rtot.Pitch);
                // rtot.Pitch = fucking_fuckit(rtot.Pitch);
                LOG("rotot pitch: {}", rtot.Pitch);
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
