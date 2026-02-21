// touchcontrols.h - Virtual joystick and touch button controls for Android

#pragma once

#ifdef __ANDROID__

#include <stdbool.h>
#include <SDL3/SDL.h>

// Control mode enum (matching TouchControls.c)
typedef enum {
    CONTROL_MODE_JOYSTICK  = 0,
    CONTROL_MODE_GYROSCOPE = 1,
} ControlMode;

void  TouchControls_Init(void);
void  TouchControls_Shutdown(void);
void  TouchControls_Draw(void);
void  TouchControls_Recenter(void);
void  TouchControls_ToggleMode(void);
ControlMode TouchControls_GetMode(void);
void  TouchControls_UpdateGyro(void);

void  TouchControls_ProcessFingerDown(SDL_FingerID fingerID, float fx, float fy);
void  TouchControls_ProcessFingerMotion(SDL_FingerID fingerID, float fx, float fy);
void  TouchControls_ProcessFingerUp(SDL_FingerID fingerID, float fx, float fy);

float TouchControls_GetJoystickX(void);
float TouchControls_GetJoystickY(void);
bool  TouchControls_IsButtonPressed(int gameNeed);
bool  TouchControls_IsPausePressed(void);

#endif // __ANDROID__
