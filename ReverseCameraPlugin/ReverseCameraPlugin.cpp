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
                 "1.0.0",
                 /*UNUSED*/ NULL);

template<typename S, typename... Args>
void LOG(const S & format_str, Args &&... args) {
        _globalCVarManager->log(
                std::vformat(format_str, std::make_format_args(args...)));
}

/** main program start */

void ReverseCameraPlugin::onLoad() {
        _globalCVarManager = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        // attempt to load parent program
        cvarManager->executeCommand("plugin load CameraControl", false);
        CheckCameraControlLoaded();

        HookedEvents::AddHookedEvent(
                "Function ProjectX.Camera_X.ClampPOV",
                std::bind(&ReverseCameraPlugin::HandleValues, this));
        HookedEvents::AddHookedEvent(
                "Function TAGame.PlayerController_TA.PressRearCamera",
                [&](std::string eventName) { isInRearCam = true; });
        HookedEvents::AddHookedEvent(
                "Function TAGame.PlayerController_TA.ReleaseRearCamera",
                [&](std::string eventName) { isInRearCam = false; });
        HookedEvents::AddHookedEvent(
                "Function TAGame.CameraState_BallCam_TA.BeginCameraState",
                [&](std::string eventName) { isInBallCam = true; });
        HookedEvents::AddHookedEvent(
                "Function TAGame.CameraState_BallCam_TA.EndCameraState",
                [&](std::string eventName) { isInBallCam = false; });

        cvarManager
                ->registerCvar(
                        "ReverseCamera_Enabled", std::to_string(false),
                        "Dictates whether the Reverse Camera plugin is enabled (true/false)")
                .addOnValueChanged(
                        [this](std::string cvar, CVarWrapper cvarValue) {
                                enabled = cvarValue.getBoolValue();
                                cvarManager->getCvar("CamControl_Enable")
                                        .setValue(cvarValue.getBoolValue());
                        });
}

void ReverseCameraPlugin::onUnload() {
        // destroy things
        // dont throw here
        ResetCameraCVars();
}

void ReverseCameraPlugin::RenderSettings() {
        // for imgui plugin window

        if (!camcontrol_loaded) {
                ImGui::Text(
                        "This plugin relies on another one called \"CameraControl\" provided by CameraControl.dll");
                ImGui::Text(
                        "Camera control plugin isn't loaded or does not exist.");
                ImGui::Text(
                        "See %s for a vanilla version or plugin author for chocolate.",
                        "https://bakkesplugins.com/plugins/view/71");

                if (ImGui::Button("Check again if CameraControl is loaded.")) {
                        CheckCameraControlLoaded();
                }
                return;
        }

        CVarWrapper enabled_cvar =
                cvarManager->getCvar("ReverseCamera_Enabled");
        bool enable = enabled_cvar.getBoolValue();
        if (ImGui::Checkbox("Enable that shit?", &enable)) {
                enabled_cvar.setValue(enable);
        }
}

// Don't call this yourself, BM will call this function with a pointer to the
// current ImGui context
void ReverseCameraPlugin::SetImGuiContext(uintptr_t ctx) {
        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

bool ReverseCameraPlugin::CheckCameraControlLoaded() {
        // try to find "CameraControl" from the loaded plugins.
        PluginManagerWrapper pmw = gameWrapper->GetPluginManager();
        const auto & plugins = *pmw.GetLoadedPlugins();
        camcontrol_loaded =
                (std::find_if(
                         begin(plugins), end(plugins),
                         [this](const std::shared_ptr<
                                 BakkesMod::Plugin::LoadedPlugin> & plugin) {
                                 return std::string{plugin->_filename} ==
                                        "cameracontrol";
                         }) != end(plugins));
        if (camcontrol_loaded) {
                ResetCameraCVars();
        }
        return camcontrol_loaded;
}

std::string ReverseCameraPlugin::GetPluginName() {
        return "Reverse Camera Plugin";
}

// basically the following functions are doing things like in the other
// templates using the "camera control" plugin

void ReverseCameraPlugin::HandleValues() {
        if (!CanCreateValues())
                return;

        ServerWrapper server = gameWrapper->GetCurrentGameState();
        CameraWrapper cam = gameWrapper->GetCamera();
        if (server.IsNull() || cam.IsNull()) {
                if (!cvars_nulled_out) {
                        ResetCameraCVars();
                }
                return;
        }

        CarWrapper car = gameWrapper->GetLocalCar();
        if (car.IsNull()) {
                // if you don't have an active car (or server or camera), you're
                // probably not in a place where the camera needs to be changed
                // (ie the menu, spectating, etc)
                //
                // I just want to avoid calling
                // this every call of ClampPOV... hence the bool
                if (!cvars_nulled_out) {
                        ResetCameraCVars();
                }
        } else {  // do work

                Vector carSpeedVec = car.GetVelocity();
                Rotator carRotationValue = car.GetRotation();
                if (gameWrapper->IsInOnlineGame()) {
                        union ex {
                                unsigned int x;
                                float f;
                        };
                        carSpeedVec.X = ex{(*reinterpret_cast<unsigned int *>(
                                                    &carSpeedVec.X) ^
                                            0xdeadbeef)}
                                                .f;
                        carSpeedVec.Y = ex{(*reinterpret_cast<unsigned int *>(
                                                    &carSpeedVec.Y) ^
                                            0xdefec8ed)}
                                                .f;
                        carSpeedVec.Z = ex{(*reinterpret_cast<unsigned int *>(
                                                    &carSpeedVec.Z) ^
                                            0xf005ba11)}
                                                .f;

                        carRotationValue.Pitch ^=
                                0xdeadbef;  // it's like the 3rd to last was
                                            // removed
                        carRotationValue.Yaw ^=
                                0xdefec8d;  // 2nd to last was removed
                        carRotationValue.Roll ^=
                                0xf005ba1;  // last was removed... kay
                }

                int rearCamMult = 1;
                if (isInRearCam) {
                        rearCamMult = -1;
                }

                Vector carLocation = car.GetLocation();
                carRotationValue = Rotator{0, 0, 0};
                Quat carQuat = rotToQuat(carRotationValue);
                // Quat carQuat2 =
                //         rotToQuat(Rotator{rearCamMult *
                //         carRotationValue.Pitch,
                //                           carRotationValue.Yaw,
                //                           rearCamMult *
                //                           carRotationValue.Roll});
                Quat carQuat2 = rotToQuat(Rotator{carRotationValue.Pitch,
                                                  carRotationValue.Yaw,
                                                  carRotationValue.Roll});

                // FOCUS
                // Vector baseCameraPosition = {42, 0, 35};
                // Vector alignedCameraPosition =
                //        sp::rotateVectorWithQuat(baseCameraPosition, carQuat);
                // FOCUS = car.GetLocation() + alignedCameraPosition;
                // FOCUS = car.GetLocation();

                // ROTATION
                Rotator baseCameraRotation = {0, 0, 0};
                Quat baseCameraQuat = rotToQuat(
                        Rotator{SWIVEL.Pitch * 2, SWIVEL.Yaw, SWIVEL.Roll} +
                        baseCameraRotation);
                Rotator alignedCameraQuat =
                        sp::quatToRot(carQuat2 * baseCameraQuat);

                BallWrapper ball = server.GetBall();
                if (!ball.IsNull() && isInBallCam && !isInRearCam) {
                        Vector newQuatFwd = sp::quatToFwd(carQuat);
                        Vector newQuatRight = sp::quatToRight(carQuat);
                        Vector newQuatUp = sp::quatToUp(carQuat);

                        Vector ballLocation = ball.GetLocation();
                        Vector cameraLocation = cam.GetLocation();

                        Vector targetDirection = {
                                ballLocation.X - cameraLocation.X,
                                ballLocation.Y - cameraLocation.Y,
                                ballLocation.Z - cameraLocation.Z};
                        targetDirection.normalize();
                        Vector targetDirectionUpComponent =
                                project_v1_on_v2(targetDirection, newQuatUp);
                        Vector rejectedTargetDirection =
                                targetDirection - targetDirectionUpComponent;
                        rejectedTargetDirection.normalize();

                        // rotate newQuatFwd and newQuatRight around newQuatUp
                        // so that newQuatFwd is aligned to
                        // rejectedTargetDirection
                        float a = (rejectedTargetDirection - newQuatFwd)
                                          .magnitude();
                        float b = rejectedTargetDirection.magnitude();
                        float c = newQuatFwd.magnitude();
                        float upRot = acos((b * b + c * c - a * a) / 2 * b * c);
                        if (Vector::dot(rejectedTargetDirection,
                                        newQuatRight) <= 0)
                                upRot *= -1;
                        Quat upRotQuat = AngleAxisRotation(upRot, newQuatUp);
                        newQuatFwd =
                                sp::rotateVectorWithQuat(newQuatFwd, upRotQuat);
                        newQuatRight = sp::rotateVectorWithQuat(newQuatRight,
                                                                upRotQuat);

                        // rotate newQuatFwd and newQuatUp around newQuatRight
                        // so that newQuatFwd is aligned to targetDirection
                        a = (targetDirection - newQuatFwd).magnitude();
                        b = targetDirection.magnitude();
                        c = newQuatFwd.magnitude();
                        float rightRot =
                                acos((b * b + c * c - a * a) / 2 * b * c);
                        if (Vector::dot(targetDirection, newQuatUp) >= 0)
                                rightRot *= -1;
                        Quat rightRotQuat =
                                AngleAxisRotation(rightRot, newQuatRight);
                        newQuatFwd = sp::rotateVectorWithQuat(newQuatFwd,
                                                              rightRotQuat);
                        newQuatUp = sp::rotateVectorWithQuat(newQuatUp,
                                                             rightRotQuat);

                        Quat newQuat = GetQuatFromMatrix(
                                newQuatFwd, newQuatRight, newQuatUp);
                        alignedCameraQuat =
                                sp::quatToRot(newQuat * baseCameraQuat);
                }
                ROTATION = alignedCameraQuat - SWIVEL;

                // DISTANCE
                DISTANCE = 0;

                // FOV
                CameraWrapper camera = gameWrapper->GetCamera();
                ProfileCameraSettings playerCamSettings =
                        camera.GetCameraSettings();
                float baseFOV = playerCamSettings.FOV;
                float carSpeed = sqrt(carSpeedVec.X * carSpeedVec.X +
                                      carSpeedVec.Y * carSpeedVec.Y +
                                      carSpeedVec.Z * carSpeedVec.Z);
                float supersonic = 2200;
                float speedPerc = carSpeed / supersonic;
                float newFOV = 0;

                bool isSupersonic = false;
                if (car.GetbSuperSonic() == 1) {
                        isSupersonic = true;
                }

                float interpsteps = 25;
                if (isSupersonic) {
                        if (interpStepsIntoSupersonic < interpsteps) {
                                interpStepsIntoSupersonic++;
                        }
                        newFOV =
                                baseFOV + 5 +
                                (5 * (interpStepsIntoSupersonic / interpsteps));
                        wasSupersonic = true;
                        interpStepsOutOfSupersonic = 0;
                } else {
                        interpStepsIntoSupersonic = 0;
                        if (wasSupersonic) {
                                if (interpStepsOutOfSupersonic < interpsteps) {
                                        interpStepsOutOfSupersonic++;
                                }
                                newFOV = baseFOV + 10 -
                                         (5 * (interpStepsOutOfSupersonic /
                                               interpsteps));
                                if (interpStepsOutOfSupersonic >= interpsteps) {
                                        wasSupersonic = false;
                                }
                        } else {
                                newFOV = baseFOV + 5 * speedPerc;
                        }
                }
                FOV = newFOV;

                // DEBUG
                LOG("DISTANCE= {} FOV= {{{}, BASEFOV: {}, NEWFOV: {}}} ROTATION= {{Pitch: {}, Yaw: {}, Roll: {}}} SWIVEL FOCUS= {{X: {}, Y: {}, Z: {}}} | SPEEDPERC= {}  CARSPEED= {} | CAR VELOCITY: {{X:{}, Y:{}, Z:{}}} | ISPLAYEROWNED? {}",
                    DISTANCE, FOV, baseFOV, newFOV, ROTATION.Pitch,
                    ROTATION.Yaw, ROTATION.Roll, FOCUS.X, FOCUS.Y, FOCUS.Z,
                    speedPerc, carSpeed, carSpeedVec.X, carSpeedVec.Y,
                    carSpeedVec.Z, car.IsPlayerOwned());

                // set values
                cvarManager->getCvar("CamControl_FOV")
                        .setValue(
                                std::vformat("{}", std::make_format_args(FOV)));
                cvarManager->getCvar("CamControl_Rotation")
                        .setValue(std::vformat(
                                "{},{},{}",
                                std::make_format_args(ROTATION.Pitch,
                                                      ROTATION.Yaw,
                                                      ROTATION.Roll)));
                cvars_nulled_out = false;
        }
}

// MATH
Vector ReverseCameraPlugin::project_v1_on_v2(Vector vec1, Vector vec2) {
        float dot = Vector::dot(vec1, vec2);
        float vec2magnitude = vec2.magnitude();
        return (vec2 * dot / (vec2magnitude * vec2magnitude));
}
Quat ReverseCameraPlugin::AngleAxisRotation(float angle, Vector axis) {
        // DEGREES
        Quat result;
        float angDiv2 = angle * 0.5f;
        result.W = cos(angDiv2);
        result.X = axis.X * sin(angDiv2);
        result.Y = axis.Y * sin(angDiv2);
        result.Z = axis.Z * sin(angDiv2);

        return result;
}
Quat ReverseCameraPlugin::GetQuatFromMatrix(Vector fwd,
                                            Vector right,
                                            Vector up) {
        // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
        // W, X, Y, Z
        Quat q(1, 0, 0, 0);

        //{ fwd.X, right.X, up.X }
        //{ fwd.Y, right.Y, up.Y }  ------->  newQuat
        //{ fwd.Z, right.Z, up.Z }
        float a[3][3];

        a[0][0] = fwd.X, a[0][1] = right.X, a[0][2] = up.X;
        a[1][0] = fwd.Y, a[1][1] = right.Y, a[1][2] = up.Y;
        a[2][0] = fwd.Z, a[2][1] = right.Z, a[2][2] = up.Z;

        float trace = a[0][0] + a[1][1] + a[2][2];
        if (trace > 0) {
                float s = 0.5f / sqrtf(trace + 1.0f);
                q.W = 0.25f / s;
                q.X = (a[2][1] - a[1][2]) * s;
                q.Y = (a[0][2] - a[2][0]) * s;
                q.Z = (a[1][0] - a[0][1]) * s;
        } else {
                if (a[0][0] > a[1][1] && a[0][0] > a[2][2]) {
                        float s = 2.0f *
                                  sqrtf(1.0f + a[0][0] - a[1][1] - a[2][2]);
                        q.W = (a[2][1] - a[1][2]) / s;
                        q.X = 0.25f * s;
                        q.Y = (a[0][1] + a[1][0]) / s;
                        q.Z = (a[0][2] + a[2][0]) / s;
                } else if (a[1][1] > a[2][2]) {
                        float s = 2.0f *
                                  sqrtf(1.0f + a[1][1] - a[0][0] - a[2][2]);
                        q.W = (a[0][2] - a[2][0]) / s;
                        q.X = (a[0][1] + a[1][0]) / s;
                        q.Y = 0.25f * s;
                        q.Z = (a[1][2] + a[2][1]) / s;
                } else {
                        float s = 2.0f *
                                  sqrtf(1.0f + a[2][2] - a[0][0] - a[1][1]);
                        q.W = (a[1][0] - a[0][1]) / s;
                        q.X = (a[0][2] + a[2][0]) / s;
                        q.Y = (a[1][2] + a[2][1]) / s;
                        q.Z = 0.25f * s;
                }
        }

        return q;
}
Quat ReverseCameraPlugin::rotToQuat(Rotator rot) {
        float uRotToDeg = 182.044449f;
        float DegToRadDiv2 = (M_PI / 180.0f) / 2.0f;
        float sinPitch = sin((rot.Pitch / uRotToDeg) * DegToRadDiv2);
        float cosPitch = cos((rot.Pitch / uRotToDeg) * DegToRadDiv2);
        float sinYaw = sin((rot.Yaw / uRotToDeg) * DegToRadDiv2);
        float cosYaw = cos((rot.Yaw / uRotToDeg) * DegToRadDiv2);
        float sinRoll = sin((rot.Roll / uRotToDeg) * DegToRadDiv2);
        float cosRoll = cos((rot.Roll / uRotToDeg) * DegToRadDiv2);

        Quat convertedQuat;
        convertedQuat.X =
                (cosRoll * sinPitch * sinYaw) - (sinRoll * cosPitch * cosYaw);
        convertedQuat.Y = ((-cosRoll) * sinPitch * cosYaw) -
                          (sinRoll * cosPitch * sinYaw);
        convertedQuat.Z =
                (cosRoll * cosPitch * sinYaw) - (sinRoll * sinPitch * cosYaw);
        convertedQuat.W =
                (cosRoll * cosPitch * cosYaw) + (sinRoll * sinPitch * sinYaw);

        return convertedQuat;
}

bool ReverseCameraPlugin::CanCreateValues() const {
        if (!enabled || IsCVarNull("CamControl_Swivel_READONLY") ||
            IsCVarNull("CamControl_Focus") ||
            IsCVarNull("CamControl_Rotation") ||
            IsCVarNull("CamControl_Distance") || IsCVarNull("CamControl_FOV"))
                return false;
        else
                return true;
}

bool ReverseCameraPlugin::IsCVarNull(std::string cvarName) const {
        CVarWrapper cvar = cvarManager->getCvar(cvarName);
        return cvar.IsNull();
}

void ReverseCameraPlugin::ResetCameraCVars() {
        cvarManager->getCvar("CamControl_Distance").setValue("NULL");
        cvarManager->getCvar("CamControl_FOV").setValue("NULL");
        cvarManager->getCvar("CamControl_Rotation").setValue("NULL");
        cvarManager->getCvar("CamControl_Focus").setValue("NULL");

        cvars_nulled_out = true;
}

ServerWrapper ReverseCameraPlugin::GetCurrentGameState() const {
        if (gameWrapper->IsInReplay())
                return gameWrapper->GetGameEventAsReplay().memory_address;
        else if (gameWrapper->IsInOnlineGame())
                return gameWrapper->GetOnlineGame();
        else
                return gameWrapper->GetGameEventAsServer();
}