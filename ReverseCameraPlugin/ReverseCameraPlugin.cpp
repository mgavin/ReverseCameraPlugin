#include "ReverseCameraPlugin.h"
#define _USE_MATH_DEFINES
#include <math.h>

#include <format>
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/gfx/GfxDataTrainingWrapper.h"
#include "HookedEvents.h"

std::shared_ptr<CVarManagerWrapper> _globalCVarManager;

BAKKESMOD_PLUGIN(
        ReverseCameraPlugin,
        "ReverseCameraPlugin",
        "2.3.16",
        /*UNUSED*/ NULL);

template<typename S, typename... Args>
void LOG(const S & format_str, Args &&... args) {
        _globalCVarManager->log(std::vformat(format_str, std::make_format_args(args...)));
}

/** main program start */

void ReverseCameraPlugin::onLoad() {
        _globalCVarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        HookedEvents::AddHookedEvent("Function ReplayDirector_TA.Playing.BeginState", [this](std::string eventName) {
                in_goal_replay = true;

                CameraWrapper cam = gameWrapper->GetCamera();
                if (!cam.IsNull()) {
                        cam.SetbEnableFading(1);
                }
        });

        HookedEvents::AddHookedEvent("Function ReplayDirector_TA.Playing.EndState", [this](std::string eventName) {
                in_goal_replay = false;

                CameraWrapper cam = gameWrapper->GetCamera();
                if (!cam.IsNull()) {
                        cam.SetbEnableFading(1);
                }
        });

        HookedEvents::AddHookedEvent(
                "Function TAGame.GFxData_Settings_TA.SetInvertSwivelPitch",
                [this](std::string eventName) {
                        invert_swivel = !(gameWrapper->GetSettings().GetCameraSaveSettings().InvertSwivelPitch);
                });

        HookedEvents::AddHookedEvent(
                "Function TAGame.GFxData_Settings_TA.SetCameraAngle",
                [this](std::string eventName) {
                        //  because if you go too fast in the menu, it may get called at inappropriate times,
                        //  objects may not get updated quickly enough. would be nice for a WAITFORMENU();
                        // nah we just wait for the operations to complete and call it here.
                        gameWrapper->Execute([this](GameWrapper * gw) {
                                CameraWrapper cam = gw->GetCamera();
                                if (!cam.IsNull()) {
                                        angle_setting = std::fabs(cam.GetCameraSettings().Pitch);
                                }
                        });
                });
        HookedEvents::AddHookedEvent("Function TAGame.GameEvent_Soccar_TA.InitGame", [this](std::string eventName) {
                // call when the game is being initialized, when we know there should be a camera loaded.
                gameWrapper->Execute([this](GameWrapper * gw) {
                        CameraWrapper cam = gw->GetCamera();
                        if (!cam.IsNull()) {
                                angle_setting = std::fabs(cam.GetCameraSettings().Pitch);
                        }
                });
        });

        keybindCvar = std::make_unique<CVarWrapper>(
                cvarManager->registerCvar("ReverseCamera_Keybind", "0", "Reverse Camera keybind", false));
        cvarManager
                ->registerCvar(
                        "ReverseCamera_Enabled",
                        std::to_string(false),
                        "Dictates whether the Reverse Camera plugin is enabled (true/false)")
                .addOnValueChanged([this](std::string oldValue, CVarWrapper newValue) {
                        enabled = newValue.getBoolValue();
                        if (enabled) {
                                gameWrapper->Execute([this](GameWrapper * gw) {
                                        CameraWrapper cam = gw->GetCamera();
                                        if (!cam.IsNull()) {
                                                angle_setting = std::fabs(cam.GetCameraSettings().Pitch);
                                        }
                                });
                                HookedEvents::AddHookedEvent(
                                        "Function Engine.GameViewportClient.Tick",
                                        bind(&ReverseCameraPlugin::onTick, this, std::placeholders::_1));
                                HookedEvents::AddHookedEvent(
                                        "Function ProjectX.Camera_X.ClampPOV",
                                        std::bind(&ReverseCameraPlugin::HandleValues, this));
                        } else {
                                HookedEvents::RemoveHook("Function Engine.GameViewportClient.Tick");
                                HookedEvents::RemoveHook("Function ProjectX.Camera_X.ClampPOV");
                        }
                });

        cvarManager
                ->registerCvar(
                        "ReverseCamera_Cam_Angle_Compensate",
                        std::to_string(false),
                        "Flag for plugin to compensate the pitch for camera's angle")
                .addOnValueChanged([this](std::string oldValue, CVarWrapper newValue) {
                        compensate_for_angle = newValue.getBoolValue();
                });

        /* TODOOOOOOOOOOOOOOOOOOO: HANDLING DIFFERENT
         * KEYBINDS? */
        right_stick_fnameindex = gameWrapper->GetFNameIndexByString("XboxTypeS_RightThumbStick");

        invert_swivel = gameWrapper->GetSettings().GetCameraSaveSettings().InvertSwivelPitch;
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
        ServerWrapper           sw  = gameWrapper->GetCurrentGameState();

        if (!pcw.IsNull() && !cam.IsNull() && !sw.IsNull()) {
                if (pcw.GetbUsingGamepad()) {
                        // is right stick pressed?
                        //  if so = RearCam, otherwise Cleanup
                        const bool stick_is_pressed = gameWrapper->IsKeyPressed(right_stick_fnameindex);
                        if (stick_is_pressed && !already_pressed) {
                                in_reverse_cam = true;
                                cam.SetSwivelDieRate(0.0f);
                                already_pressed = true;
                        } else if (!stick_is_pressed && already_pressed) {
                                if (in_goal_replay) {
                                        cam.SetbEnableFading(0);
                                }
                                // this fixes the camera correcting its swivel
                                // after releasing the button
                                Rotator rot = cam.GetDesiredSwivel(
                                        ((invert_swivel && !in_goal_replay) ? 1 : -1) * rsticky,
                                        rstickx);
                                cam.SetCurrentSwivel(rot);
                                cam.UpdateSwivel(0.0f);
                                cam.SetSwivelDieRate(2.0f);
                                in_reverse_cam  = false;
                                already_pressed = false;
                        }
                        const int PLAYER_NUMBER = 0;
                        std::memset(&xboxControllerState, 0, sizeof(XINPUT_STATE));
                        DWORD result = XInputGetState(PLAYER_NUMBER, &xboxControllerState);
                        if (result == ERROR_DEVICE_NOT_CONNECTED) {  // controller
                                                                     // isn't
                                                                     // connected
                                rstickx = rsticky = 0.0f;
                        } else {
                                rstickx = static_cast<float>(xboxControllerState.Gamepad.sThumbRX) / SHRT_MAX;
                                rsticky = -1.0f * static_cast<float>(xboxControllerState.Gamepad.sThumbRY) / SHRT_MAX;
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
                Rotator rot   = cam.GetDesiredSwivel(((invert_swivel) ? -1 : 1) * rsticky, rstickx);
                Rotator rtot  = rot + Rotator {32767, 0, 32767};
                Rotator rtotc = rtot;
                if (compensate_for_angle) {
                        rtot += {180 * 2 * static_cast<int>(angle_setting), 0, 0};
                }
                if (in_goal_replay) {
                        const int MAGIC_REPLAY_ANGLE  = 15;
                        rtot                         += {180 * 2 * MAGIC_REPLAY_ANGLE, 0, 0};
                }

                cam.SetCurrentSwivel(rtot);
                cam.UpdateSwivel(0.0f);
        };
}

void ReverseCameraPlugin::RenderSettings() {
        // for imgui plugin window

        // overview options, checkboxes
        CVarWrapper enabled_cvar = cvarManager->getCvar("ReverseCamera_Enabled");
        bool        enable       = enabled_cvar.getBoolValue();
        if (ImGui::Checkbox("ENABLE?????????????", &enable)) {
                enabled_cvar.setValue(enable);
        }

        CVarWrapper compensate_cvar = cvarManager->getCvar("ReverseCamera_Cam_Angle_Compensate");
        bool        compensate      = compensate_cvar.getBoolValue();
        if (ImGui::Checkbox("Compensate pitch for camera angle?", &compensate)) {
                compensate_cvar.setValue(compensate);
        }

        ImGui::NewLine();
        ImGui::Separator();
        ImGui::NewLine();

        // middle section... explanations?
        ImGui::Text("MAYBE SOME FUCKING TEXT CAN GO HERE EXPLAINING THINGS");

        ImGui::NewLine();
        ImGui::Separator();
        ImGui::NewLine();

        // bottom section... more specific customization options?
        static const char * keyText  = "Key List";
        static const char * hintText = "Type to Filter";

        ImGui::TextUnformatted("Press the Set Keybind button below to bind command ReverseCamera_Keybind to a key:");
        if (ImGui::SearchableCombo("##keybind combo", &keyIndex, keys, keyText, hintText, 20))
                OnBind(keys[keyIndex]);

        ImGui::SameLine();
        if (ImGui::ButtonEx("Set Keybind", ImVec2(0, 0), ImGuiButtonFlags_AlignTextBaseLine)) {
                should_block_until_keybound = true;
                gameWrapper->Execute([this](GameWrapper * gw) {
                        cvarManager->executeCommand("closemenu settings; openmenu reversecamerabind");
                        HookedEvents::AddHookedEventWithCaller<ActorWrapper>(
                                "Function TAGame.GameViewportClient_TA.HandleKeyPress",
                                std::bind(
                                        &ReverseCameraPlugin::OnKeyPressed,
                                        this,
                                        std::placeholders::_1,
                                        std::placeholders::_2,
                                        std::placeholders::_3));
                });
        }

        ImGui::SameLine();
        if (ImGui::ButtonEx("Unbind", ImVec2(0, 0), ImGuiButtonFlags_AlignTextBaseLine)) {
                if (keyIndex != -1) {
                        keybindCvar->setValue("0");
                        cvarManager->executeCommand("unbind " + keys[keyIndex]);
                        gameWrapper->Toast("ReverseCameraPlugin", "ReverseCamera_Keybind is now unbound!", "", 5.0f);
                        keyIndex = -1;
                }
        }

        ImGui::TextUnformatted("Bind a key to associate with reversing the camera. Fucking clean this up later.");
        ImGui::Separator();
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 65);
        // ImGui::Image(logo->GetImGuiTex(), {logo->GetSizeF().X, logo->GetSizeF().Y});
        ImGui::TextUnformatted("WOW KEYBIND CODE TAKEN FROM SKIP REPLAY PLUGIN");
        ImGui::TextUnformatted("IDK IM KEEPING THEIR LITTLE NOTE - v1.5 made by Esnar#0600 and Insane#0418");
}

// Don't call this yourself, BM will call this function with a pointer
// to the current ImGui context
void ReverseCameraPlugin::SetImGuiContext(uintptr_t ctx) {
        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

std::string ReverseCameraPlugin::GetPluginName() {
        return "Reverse Camera Plugin";
}

void ReverseCameraPlugin::Render() {
        // creating a translucent background that refuses inputs
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(30.f, 30.0f, 30.0f, 0.1f));
        ImGui::Begin(
                "translucent_background",
                &isWindowOpen,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration |
                        ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::PopStyleColor();
        ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(140, 40));
        ImGui::SetNextWindowFocus();
        ImGui::Begin(
                "Set Keybind",
                &isWindowOpen,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(1.6f);
        ImGui::TextUnformatted("Press any key");
        ImGui::End();
        ImGui::End();
}

void ReverseCameraPlugin::OnBind(std::string key) {
        if (key != (keybind = keybindCvar->getStringValue())) {
                std::string toastMsg = "ReverseCamera_Keybind bound to " + keys[keyIndex];
                if (keybind != "0") {
                        toastMsg += " and " + keybind + " is now unbound";
                        cvarManager->executeCommand("unbind " + keybind);
                }
                toastMsg += "!";
                keybindCvar->setValue(key);
                cvarManager->executeCommand("bind " + key + " ReverseCamera_Keybind");
                gameWrapper->Toast("ReverseCamera", toastMsg, "", 5.0f);
        }
}

void ReverseCameraPlugin::OnKeyPressed(ActorWrapper aw, void * params, std::string eventName) {
        std::string key = gameWrapper->GetFNameByIndex(((keypress_t *)params)->key.Index);
        keyIndex = ((keysIt = find(keys.begin(), keys.end(), key)) != keys.end()) ? (int)(keysIt - keys.begin()) : -1;
        cvarManager->executeCommand("closemenu reversecamerabind; openmenu settings");
        HookedEvents::RemoveHook("Function TAGame.GameViewportClient_TA.HandleKeyPress");
        should_block_until_keybound = false;
        //
        // OnBind(keys[keyIndex]); // wait, what the fuck.it crashed when I used my controller.
        // AND PRESSED THE A BUTTON ?

        // maybe this will help the "set timeout" grabbing of the angle setting :/
        gameWrapper->Execute([this](GameWrapper * gw) { OnBind(keys[keyIndex]); });
}

void ReverseCameraPlugin::OnOpen() {
        isWindowOpen = true;
}

void ReverseCameraPlugin::OnClose() {
        isWindowOpen = false;
}

std::string ReverseCameraPlugin::GetMenuName() {
        return "reversecamerabind";
}

std::string ReverseCameraPlugin::GetMenuTitle() {
        return "";
}

bool ReverseCameraPlugin::ShouldBlockInput() {
        return false;
        if (should_block_until_keybound) {
                return true;
        } else {
                return false;
        }
}

bool ReverseCameraPlugin::IsActiveOverlay() {
        return true;
}