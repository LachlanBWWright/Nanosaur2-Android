// TouchControls.c - Virtual joystick and touch button controls for Android
// (C) 2025

#ifdef __ANDROID__

#include "game.h"
#include <SDL3/SDL.h>
#include <math.h>

// ============================================================
// CONSTANTS
// ============================================================

#define JOYSTICK_BASE_RADIUS_FRAC   0.12f   // fraction of screen height
#define JOYSTICK_THUMB_RADIUS_FRAC  0.05f
#define JOYSTICK_DEAD_ZONE          0.15f
#define BUTTON_RADIUS_FRAC          0.07f   // fraction of screen height

// Joystick position: lower left
#define JOYSTICK_CX_FRAC    0.13f
#define JOYSTICK_CY_FRAC    0.75f

// Action buttons: lower right (diamond layout)
#define BTN_RIGHT_CX_FRAC   0.90f
#define BTN_RIGHT_CY_FRAC   0.65f

// Control mode
typedef enum {
    CONTROL_MODE_JOYSTICK  = 0,
    CONTROL_MODE_GYROSCOPE = 1,
} ControlMode;

// ============================================================
// STATE
// ============================================================

static ControlMode   gControlMode     = CONTROL_MODE_JOYSTICK;
static bool          gControlsVisible = true;

// Joystick state
static float gJoystickCX, gJoystickCY, gJoystickRadius;
static float gJoystickThumbRadius;
static float gJoystickX = 0.0f, gJoystickY = 0.0f;  // -1..+1
static SDL_FingerID gJoystickFingerID = 0;
static bool  gJoystickActive = false;

// Recenter button
static float gRecenterBtnX, gRecenterBtnY, gRecenterBtnRadius;

// Toggle control mode button
static float gToggleBtnX, gToggleBtnY, gToggleBtnRadius;

// Action buttons
#define MAX_TOUCH_BUTTONS 6
typedef struct {
    float       cx, cy, radius;
    int         gameNeed;       // kNeed_* constant
    bool        pressed;
    SDL_FingerID fingerID;
    bool        fingerActive;
    const char *label;
} TouchButton;

static TouchButton gButtons[MAX_TOUCH_BUTTONS];
static int         gNumButtons = 0;

// Pause button
static float gPauseBtnX, gPauseBtnY, gPauseBtnRadius;
static bool  gPauseBtnPressed = false;

// Gyro smoothing
static float gGyroX = 0.0f, gGyroY = 0.0f;
static bool  gGyroAvailable = false;

// ============================================================
// HELPER: distance
// ============================================================
static float Distance2D(float ax, float ay, float bx, float by) {
    float dx = ax-bx, dy = ay-by;
    return sqrtf(dx*dx + dy*dy);
}

// ============================================================
// INITIALIZATION
// ============================================================

void TouchControls_Init(void) {
    int w = 0, h = 0;
    extern SDL_Window* gSDLWindow;
    SDL_GetWindowSize(gSDLWindow, &w, &h);
    if (w <= 0) w = 1280;
    if (h <= 0) h = 720;

    float fw = (float)w, fh = (float)h;

    // Joystick
    gJoystickRadius      = fh * JOYSTICK_BASE_RADIUS_FRAC;
    gJoystickThumbRadius = fh * JOYSTICK_THUMB_RADIUS_FRAC;
    gJoystickCX          = fw * JOYSTICK_CX_FRAC;
    gJoystickCY          = fh * JOYSTICK_CY_FRAC;

    // Buttons (lower-right diamond)
    float btnR   = fh * BUTTON_RADIUS_FRAC;
    float btnCX  = fw * BTN_RIGHT_CX_FRAC;
    float btnCY  = fh * BTN_RIGHT_CY_FRAC;
    float btnSpacing = btnR * 2.3f;

    gNumButtons = 0;

    // Fire (right)
    gButtons[gNumButtons++] = (TouchButton){
        .cx=btnCX+btnSpacing, .cy=btnCY, .radius=btnR,
        .gameNeed=kNeed_Fire, .label="F"
    };
    // Jetpack (top)
    gButtons[gNumButtons++] = (TouchButton){
        .cx=btnCX, .cy=btnCY-btnSpacing, .radius=btnR,
        .gameNeed=kNeed_Jetpack, .label="J"
    };
    // Prev weapon (left)
    gButtons[gNumButtons++] = (TouchButton){
        .cx=btnCX-btnSpacing, .cy=btnCY, .radius=btnR,
        .gameNeed=kNeed_PrevWeapon, .label="<"
    };
    // Next weapon (bottom)
    gButtons[gNumButtons++] = (TouchButton){
        .cx=btnCX, .cy=btnCY+btnSpacing, .radius=btnR,
        .gameNeed=kNeed_NextWeapon, .label=">"
    };

    // Pause button (top right)
    gPauseBtnRadius = fh * 0.045f;
    gPauseBtnX = fw * 0.95f;
    gPauseBtnY = fh * 0.07f;

    // Recenter button (near joystick, above)
    gRecenterBtnRadius = fh * 0.04f;
    gRecenterBtnX = gJoystickCX + gJoystickRadius + gRecenterBtnRadius * 2.0f;
    gRecenterBtnY = gJoystickCY - gJoystickRadius;

    // Toggle control mode button (near recenter)
    gToggleBtnRadius = fh * 0.04f;
    gToggleBtnX = gRecenterBtnX + gToggleBtnRadius * 2.5f;
    gToggleBtnY = gRecenterBtnY;

    // Check if gyroscope is available
    gGyroAvailable = (SDL_NumSensors() > 0);

    gJoystickX = 0.0f;
    gJoystickY = 0.0f;
}

void TouchControls_Shutdown(void) {
    // Nothing to do
}

// ============================================================
// RECENTER
// ============================================================

void TouchControls_Recenter(void) {
    gJoystickX = 0.0f;
    gJoystickY = 0.0f;
    gGyroX = 0.0f;
    gGyroY = 0.0f;
}

// ============================================================
// TOGGLE CONTROL MODE
// ============================================================

void TouchControls_ToggleMode(void) {
    if (gControlMode == CONTROL_MODE_JOYSTICK) {
        gControlMode = CONTROL_MODE_GYROSCOPE;
    } else {
        gControlMode = CONTROL_MODE_JOYSTICK;
    }
    TouchControls_Recenter();
}

ControlMode TouchControls_GetMode(void) {
    return gControlMode;
}

// ============================================================
// PROCESS TOUCH EVENTS
// ============================================================

void TouchControls_ProcessFingerDown(SDL_FingerID fingerID, float fx, float fy) {
    int w = 0, h = 0;
    extern SDL_Window* gSDLWindow;
    SDL_GetWindowSize(gSDLWindow, &w, &h);
    float px = fx * (float)w;
    float py = fy * (float)h;

    // Pause button
    if (Distance2D(px, py, gPauseBtnX, gPauseBtnY) < gPauseBtnRadius * 1.5f) {
        gPauseBtnPressed = true;
        return;
    }

    // Recenter button
    if (Distance2D(px, py, gRecenterBtnX, gRecenterBtnY) < gRecenterBtnRadius * 1.5f) {
        TouchControls_Recenter();
        return;
    }

    // Toggle button
    if (Distance2D(px, py, gToggleBtnX, gToggleBtnY) < gToggleBtnRadius * 1.5f) {
        TouchControls_ToggleMode();
        return;
    }

    // Joystick (only in joystick mode)
    if (gControlMode == CONTROL_MODE_JOYSTICK && !gJoystickActive) {
        float joystickHitRadius = gJoystickRadius * 1.5f;
        if (Distance2D(px, py, gJoystickCX, gJoystickCY) < joystickHitRadius) {
            gJoystickActive = true;
            gJoystickFingerID = fingerID;
            // Update joystick position
            float dx = (px - gJoystickCX) / gJoystickRadius;
            float dy = (py - gJoystickCY) / gJoystickRadius;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 1.0f) { dx /= len; dy /= len; }
            gJoystickX = (fabsf(dx) > JOYSTICK_DEAD_ZONE) ? dx : 0.0f;
            gJoystickY = (fabsf(dy) > JOYSTICK_DEAD_ZONE) ? dy : 0.0f;
            return;
        }
    }

    // Action buttons
    for (int i = 0; i < gNumButtons; i++) {
        if (!gButtons[i].fingerActive &&
            Distance2D(px, py, gButtons[i].cx, gButtons[i].cy) < gButtons[i].radius * 1.3f)
        {
            gButtons[i].pressed = true;
            gButtons[i].fingerActive = true;
            gButtons[i].fingerID = fingerID;
            return;
        }
    }
}

void TouchControls_ProcessFingerMotion(SDL_FingerID fingerID, float fx, float fy) {
    int w = 0, h = 0;
    extern SDL_Window* gSDLWindow;
    SDL_GetWindowSize(gSDLWindow, &w, &h);
    float px = fx * (float)w;
    float py = fy * (float)h;

    if (gJoystickActive && gJoystickFingerID == fingerID) {
        float dx = (px - gJoystickCX) / gJoystickRadius;
        float dy = (py - gJoystickCY) / gJoystickRadius;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 1.0f) { dx /= len; dy /= len; }
        gJoystickX = (fabsf(dx) > JOYSTICK_DEAD_ZONE) ? dx : 0.0f;
        gJoystickY = (fabsf(dy) > JOYSTICK_DEAD_ZONE) ? dy : 0.0f;
    }
}

void TouchControls_ProcessFingerUp(SDL_FingerID fingerID, float fx, float fy) {
    (void)fx; (void)fy;

    if (gJoystickActive && gJoystickFingerID == fingerID) {
        gJoystickActive = false;
        gJoystickX = 0.0f;
        gJoystickY = 0.0f;
    }

    for (int i = 0; i < gNumButtons; i++) {
        if (gButtons[i].fingerActive && gButtons[i].fingerID == fingerID) {
            gButtons[i].pressed = false;
            gButtons[i].fingerActive = false;
        }
    }

    gPauseBtnPressed = false;
}

// ============================================================
// GYROSCOPE UPDATE
// ============================================================

void TouchControls_UpdateGyro(void) {
    if (gControlMode != CONTROL_MODE_GYROSCOPE) return;

    // Try to read gyroscope sensor
    SDL_SensorID *sensors = NULL;
    int count = 0;
    sensors = SDL_GetSensors(&count);
    if (!sensors || count == 0) return;

    for (int i = 0; i < count; i++) {
        if (SDL_GetSensorType(sensors[i]) == SDL_SENSOR_GYRO) {
            SDL_Sensor *sensor = SDL_OpenSensor(sensors[i]);
            if (sensor) {
                float data[3] = {0};
                SDL_GetSensorData(sensor, data, 3);
                // Integrate gyro data (simplified - roll=X axis, pitch=Y axis)
                float dt = 1.0f / 60.0f;
                gGyroX = SDL_clamp(gGyroX + data[2] * dt * 0.5f, -1.0f, 1.0f); // yaw
                gGyroY = SDL_clamp(gGyroY + data[1] * dt * 0.5f, -1.0f, 1.0f); // pitch
                SDL_CloseSensor(sensor);
            }
            break;
        }
    }
    SDL_free(sensors);
}

// ============================================================
// QUERIES
// ============================================================

float TouchControls_GetJoystickX(void) {
    if (gControlMode == CONTROL_MODE_GYROSCOPE) return gGyroX;
    return gJoystickX;
}

float TouchControls_GetJoystickY(void) {
    if (gControlMode == CONTROL_MODE_GYROSCOPE) return gGyroY;
    return gJoystickY;
}

bool TouchControls_IsButtonPressed(int gameNeed) {
    for (int i = 0; i < gNumButtons; i++) {
        if (gButtons[i].gameNeed == gameNeed && gButtons[i].pressed)
            return true;
    }
    return false;
}

bool TouchControls_IsPausePressed(void) {
    bool p = gPauseBtnPressed;
    gPauseBtnPressed = false;  // one-shot
    return p;
}

// ============================================================
// DRAW (simple overlay using GLES)
// ============================================================

// Forward declarations for GLES bridge drawing helpers
extern void bridge_Begin(GLenum mode);
extern void bridge_End(void);
extern void bridge_Vertex2f(float x, float y);
extern void bridge_Color4f(float r, float g, float b, float a);
extern void bridge_MatrixMode(GLenum mode);
extern void bridge_LoadIdentity(void);
extern void bridge_Ortho(double l, double r, double b, double t, double n, double f);
extern void bridge_Enable(GLenum cap);
extern void bridge_Disable(GLenum cap);

#define BRIDGE_GL_TRIANGLE_FAN 0x0006
#define BRIDGE_GL_LINE_LOOP    0x0003
#define BRIDGE_GL_TRIANGLES    0x0004

#define PI_F 3.14159265358979f

static void DrawCircleFilled(float cx, float cy, float radius, int segments,
                              float r, float g, float b, float a) {
    bridge_Color4f(r, g, b, a);
    bridge_Begin(BRIDGE_GL_TRIANGLE_FAN);
    bridge_Vertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI_F * (float)i / (float)segments;
        bridge_Vertex2f(cx + cosf(angle) * radius, cy + sinf(angle) * radius);
    }
    bridge_End();
}

static void DrawCircleOutline(float cx, float cy, float radius, int segments,
                               float r, float g, float b, float a) {
    bridge_Color4f(r, g, b, a);
    bridge_Begin(BRIDGE_GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * PI_F * (float)i / (float)segments;
        bridge_Vertex2f(cx + cosf(angle) * radius, cy + sinf(angle) * radius);
    }
    bridge_End();
}

static void DrawLetter(float cx, float cy, float size, const char *letter) {
    // Simple placeholder - just draw a small indicator dot
    (void)letter;
    DrawCircleFilled(cx, cy, size * 0.3f, 8, 0.9f, 0.9f, 0.9f, 0.6f);
}

void TouchControls_Draw(void) {
    if (!gControlsVisible) return;

    int w = 0, h = 0;
    extern SDL_Window* gSDLWindow;
    SDL_GetWindowSize(gSDLWindow, &w, &h);

    // Save GL state and set up 2D ortho projection
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    bridge_MatrixMode(0x1701); // GL_PROJECTION
    bridge_LoadIdentity();
    bridge_Ortho(0, w, h, 0, -1, 1);
    bridge_MatrixMode(0x1700); // GL_MODELVIEW
    bridge_LoadIdentity();

    // Draw joystick (if in joystick mode)
    if (gControlMode == CONTROL_MODE_JOYSTICK) {
        // Base circle
        DrawCircleFilled(gJoystickCX, gJoystickCY, gJoystickRadius, 32,
                         0.3f, 0.3f, 0.3f, 0.15f);
        DrawCircleOutline(gJoystickCX, gJoystickCY, gJoystickRadius, 32,
                          0.7f, 0.7f, 0.7f, 0.35f);

        // Thumb indicator
        float thumbX = gJoystickCX + gJoystickX * gJoystickRadius * 0.7f;
        float thumbY = gJoystickCY + gJoystickY * gJoystickRadius * 0.7f;
        DrawCircleFilled(thumbX, thumbY, gJoystickThumbRadius, 16,
                         0.6f, 0.6f, 0.6f, 0.5f);
        DrawCircleOutline(thumbX, thumbY, gJoystickThumbRadius, 16,
                          0.9f, 0.9f, 0.9f, 0.6f);
    } else {
        // Gyro mode indicator
        DrawCircleFilled(gJoystickCX, gJoystickCY, gJoystickRadius * 0.5f, 32,
                         0.2f, 0.4f, 0.8f, 0.25f);
        DrawCircleOutline(gJoystickCX, gJoystickCY, gJoystickRadius * 0.5f, 32,
                          0.4f, 0.6f, 1.0f, 0.5f);
    }

    // Draw recenter button
    DrawCircleFilled(gRecenterBtnX, gRecenterBtnY, gRecenterBtnRadius, 16,
                     0.2f, 0.6f, 0.2f, 0.35f);
    DrawCircleOutline(gRecenterBtnX, gRecenterBtnY, gRecenterBtnRadius, 16,
                      0.4f, 0.9f, 0.4f, 0.6f);

    // Draw toggle button
    float tR = (gControlMode == CONTROL_MODE_GYROSCOPE) ? 0.2f : 0.5f;
    float tG = (gControlMode == CONTROL_MODE_GYROSCOPE) ? 0.4f : 0.3f;
    float tB = (gControlMode == CONTROL_MODE_GYROSCOPE) ? 0.8f : 0.3f;
    DrawCircleFilled(gToggleBtnX, gToggleBtnY, gToggleBtnRadius, 16,
                     tR, tG, tB, 0.35f);
    DrawCircleOutline(gToggleBtnX, gToggleBtnY, gToggleBtnRadius, 16,
                      tR*1.5f, tG*1.5f, tB*1.5f, 0.6f);

    // Draw action buttons
    for (int i = 0; i < gNumButtons; i++) {
        float pr = gButtons[i].pressed ? 0.8f : 0.3f;
        float pg = gButtons[i].pressed ? 0.3f : 0.3f;
        float pb = gButtons[i].pressed ? 0.3f : 0.3f;
        float pa = gButtons[i].pressed ? 0.55f : 0.25f;

        DrawCircleFilled(gButtons[i].cx, gButtons[i].cy, gButtons[i].radius, 16,
                         pr, pg, pb, pa);
        DrawCircleOutline(gButtons[i].cx, gButtons[i].cy, gButtons[i].radius, 16,
                          0.8f, 0.8f, 0.8f, 0.5f);
        DrawLetter(gButtons[i].cx, gButtons[i].cy, gButtons[i].radius, gButtons[i].label);
    }

    // Draw pause button
    DrawCircleFilled(gPauseBtnX, gPauseBtnY, gPauseBtnRadius, 16,
                     0.2f, 0.2f, 0.2f, 0.3f);
    DrawCircleOutline(gPauseBtnX, gPauseBtnY, gPauseBtnRadius, 16,
                      0.7f, 0.7f, 0.7f, 0.5f);

    // Restore GL state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

#endif // __ANDROID__
