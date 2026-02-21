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
#define JOYSTICK_CY_FRAC    0.80f

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

// Toggle control mode button (top-left, rectangular - clearly not a joystick)
static float gToggleBtnX, gToggleBtnY, gToggleBtnW, gToggleBtnH;
// Recenter button (top-left, rectangular - clearly not a joystick)
static float gRecenterBtnX, gRecenterBtnY, gRecenterBtnW, gRecenterBtnH;

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

static bool PointInRect(float px, float py, float rx, float ry, float rw, float rh) {
    return px >= rx && px <= rx+rw && py >= ry && py <= ry+rh;
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

    // Toggle control mode button: top-left rectangular button  (NOT a circle, so it can't be confused with the joystick)
    gToggleBtnW = fh * 0.10f;
    gToggleBtnH = fh * 0.06f;
    gToggleBtnX = fw * 0.02f;
    gToggleBtnY = fh * 0.02f;

    // Recenter button: top-left, just to the right of the toggle button
    gRecenterBtnW = fh * 0.09f;
    gRecenterBtnH = fh * 0.06f;
    gRecenterBtnX = gToggleBtnX + gToggleBtnW + fw * 0.01f;
    gRecenterBtnY = gToggleBtnY;

    // Check if gyroscope is available
    {
        int sensorCount = 0;
        SDL_SensorID *sensorList = SDL_GetSensors(&sensorCount);
        gGyroAvailable = false;
        if (sensorList) {
            for (int s = 0; s < sensorCount && !gGyroAvailable; s++) {
                if (SDL_GetSensorTypeForID(sensorList[s]) == SDL_SENSOR_GYRO)
                    gGyroAvailable = true;
            }
            SDL_free(sensorList);
        }
    }

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

    // Toggle button (rectangular)
    if (PointInRect(px, py, gToggleBtnX, gToggleBtnY, gToggleBtnW, gToggleBtnH)) {
        TouchControls_ToggleMode();
        return;
    }

    // Recenter button (rectangular)
    if (PointInRect(px, py, gRecenterBtnX, gRecenterBtnY, gRecenterBtnW, gRecenterBtnH)) {
        TouchControls_Recenter();
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
        if (SDL_GetSensorTypeForID(sensors[i]) == SDL_SENSOR_GYRO) {
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
        if (gButtons[i].pressed) {
            if (gButtons[i].gameNeed == gameNeed) return true;
            // Emulate gamepad dual-function buttons (same button serves different purposes
            // in-game vs. in menus, matching the default gamepad bindings in InputDefaults.c).
            // GB(SOUTH)  = Jetpack in-game  / UIConfirm in menus
            // GB(WEST)   = Fire in-game     / UIDelete in menus
            // GB(EAST)   = NextWeapon       / UIBack in menus
            if (gButtons[i].gameNeed == kNeed_Jetpack  && gameNeed == kNeed_UIConfirm) return true;
            if (gButtons[i].gameNeed == kNeed_Fire     && gameNeed == kNeed_UIDelete)  return true;
            if (gButtons[i].gameNeed == kNeed_NextWeapon && gameNeed == kNeed_UIBack)  return true;
        }
    }
    // Pause button doubles as UIPause.
    // Clearing gPauseBtnPressed here is safe: UpdateInputNeeds calls this function
    // exactly once per frame for kNeed_UIPause, so it's consumed in one place per frame.
    if (gameNeed == kNeed_UIPause && gPauseBtnPressed) {
        gPauseBtnPressed = false;
        return true;
    }
    // Joystick D-pad emulation for menu navigation (digital thresholded)
    float jx = gJoystickX, jy = gJoystickY;
    if (gameNeed == kNeed_UIUp    && jy < -0.5f) return true;
    if (gameNeed == kNeed_UIDown  && jy >  0.5f) return true;
    if (gameNeed == kNeed_UIPrev  && jx < -0.5f) return true;
    if (gameNeed == kNeed_UINext  && jx >  0.5f) return true;
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
extern GLboolean bridge_IsEnabled(GLenum cap);

#define BRIDGE_GL_TRIANGLE_FAN 0x0006
#define BRIDGE_GL_LINE_LOOP    0x0003
#define BRIDGE_GL_TRIANGLES    0x0004

#define PI_F 3.14159265358979f

// GL state constants (from gles_compat.h macros which are active in this file)
#define TCGL_TEXTURE_2D    0x0DE1
#define TCGL_LIGHTING      0x0B50
#define TCGL_FOG           0x0B60
#define TCGL_ALPHA_TEST    0x0BC0

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

// Draw a filled rectangle (as two triangles)
static void DrawRectFilled(float x, float y, float w, float h,
                            float r, float g, float b, float a) {
    bridge_Color4f(r, g, b, a);
    bridge_Begin(BRIDGE_GL_TRIANGLES);
    bridge_Vertex2f(x,   y);
    bridge_Vertex2f(x+w, y);
    bridge_Vertex2f(x+w, y+h);
    bridge_Vertex2f(x,   y);
    bridge_Vertex2f(x+w, y+h);
    bridge_Vertex2f(x,   y+h);
    bridge_End();
}

// Draw a rectangle outline (as a line loop via two triangles outline)
static void DrawRectOutline(float x, float y, float w, float h,
                             float r, float g, float b, float a) {
    bridge_Color4f(r, g, b, a);
    bridge_Begin(BRIDGE_GL_LINE_LOOP);
    bridge_Vertex2f(x,   y);
    bridge_Vertex2f(x+w, y);
    bridge_Vertex2f(x+w, y+h);
    bridge_Vertex2f(x,   y+h);
    bridge_End();
}

void TouchControls_Draw(void) {
    if (!gControlsVisible) return;

    int w = 0, h = 0;
    extern SDL_Window* gSDLWindow;
    SDL_GetWindowSize(gSDLWindow, &w, &h);

    // --------------------------------------------------------
    // SAVE current bridge GL state before we modify it.
    // bridge_IsEnabled is used for bridge-tracked states;
    // glIsEnabled delegates to the real GLES for GLES-native states.
    // --------------------------------------------------------
    bridge_EnsureShaderBound();  // ensure bridge shader program is active before uniform uploads

    // Reset viewport to full window so the overlay covers the whole screen,
    // not just the last split-screen pane that was active.
    glViewport(0, 0, w, h);

    // Save state for texture unit 1 first, then unit 0 (order matters for bridge tracking)
    glActiveTexture(GL_TEXTURE1);
    GLboolean savedTex1      = bridge_IsEnabled(TCGL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    GLboolean savedTex0      = bridge_IsEnabled(TCGL_TEXTURE_2D);
    GLboolean savedLighting  = bridge_IsEnabled(TCGL_LIGHTING);
    GLboolean savedFog       = bridge_IsEnabled(TCGL_FOG);
    GLboolean savedAlphaTest = bridge_IsEnabled(TCGL_ALPHA_TEST);
    // For GLES-native states, query and save them before we change them:
    GLboolean savedDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean savedCullFace  = glIsEnabled(GL_CULL_FACE);
    GLboolean savedBlend     = glIsEnabled(GL_BLEND);

    // Set up for 2D overlay drawing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Disable 3D effects - explicitly clear both texture units
    bridge_Disable(TCGL_LIGHTING);
    bridge_Disable(TCGL_FOG);
    bridge_Disable(TCGL_ALPHA_TEST);
    glActiveTexture(GL_TEXTURE1);
    bridge_Disable(TCGL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    bridge_Disable(TCGL_TEXTURE_2D);

    bridge_MatrixMode(0x1701); // GL_PROJECTION
    bridge_LoadIdentity();
    bridge_Ortho(0, w, h, 0, -1, 1);
    bridge_MatrixMode(0x1700); // GL_MODELVIEW
    bridge_LoadIdentity();

    // --------------------------------------------------------
    // DRAW CONTROLS
    // --------------------------------------------------------

    // Draw joystick (if in joystick mode)
    if (gControlMode == CONTROL_MODE_JOYSTICK) {
        // Base circle
        DrawCircleFilled(gJoystickCX, gJoystickCY, gJoystickRadius, 32,
                         0.3f, 0.3f, 0.3f, 0.18f);
        DrawCircleOutline(gJoystickCX, gJoystickCY, gJoystickRadius, 32,
                          0.8f, 0.8f, 0.8f, 0.45f);

        // Thumb indicator
        float thumbX = gJoystickCX + gJoystickX * gJoystickRadius * 0.7f;
        float thumbY = gJoystickCY + gJoystickY * gJoystickRadius * 0.7f;
        DrawCircleFilled(thumbX, thumbY, gJoystickThumbRadius, 16,
                         0.7f, 0.7f, 0.7f, 0.6f);
        DrawCircleOutline(thumbX, thumbY, gJoystickThumbRadius, 16,
                          1.0f, 1.0f, 1.0f, 0.7f);
    } else {
        // Gyro mode: show a gyro symbol (concentric arcs approximated as small circles)
        DrawCircleFilled(gJoystickCX, gJoystickCY, gJoystickRadius, 32,
                         0.1f, 0.2f, 0.5f, 0.18f);
        DrawCircleOutline(gJoystickCX, gJoystickCY, gJoystickRadius, 32,
                          0.4f, 0.6f, 1.0f, 0.55f);
        // Inner indicator dot showing tilt
        float gx = gJoystickCX + gGyroX * gJoystickRadius * 0.7f;
        float gy = gJoystickCY + gGyroY * gJoystickRadius * 0.7f;
        DrawCircleFilled(gx, gy, gJoystickThumbRadius, 16,
                         0.4f, 0.6f, 1.0f, 0.65f);
    }

    // Draw action buttons (diamond layout, lower right)
    for (int i = 0; i < gNumButtons; i++) {
        float pr, pg, pb, pa;
        if (gButtons[i].pressed) {
            pr = 0.9f; pg = 0.5f; pb = 0.1f; pa = 0.70f;
        } else {
            pr = 0.2f; pg = 0.2f; pb = 0.4f; pa = 0.30f;
        }
        DrawCircleFilled(gButtons[i].cx, gButtons[i].cy, gButtons[i].radius, 16,
                         pr, pg, pb, pa);
        DrawCircleOutline(gButtons[i].cx, gButtons[i].cy, gButtons[i].radius, 16,
                          0.9f, 0.9f, 0.9f, 0.6f);
    }

    // Draw pause button (top right, circle)
    DrawCircleFilled(gPauseBtnX, gPauseBtnY, gPauseBtnRadius, 16,
                     0.15f, 0.15f, 0.15f, 0.4f);
    DrawCircleOutline(gPauseBtnX, gPauseBtnY, gPauseBtnRadius, 16,
                      0.8f, 0.8f, 0.8f, 0.6f);
    // Pause icon: two vertical bars
    {
        float bw = gPauseBtnRadius * 0.22f;
        float bh = gPauseBtnRadius * 0.65f;
        float gap = gPauseBtnRadius * 0.15f;
        DrawRectFilled(gPauseBtnX - gap - bw, gPauseBtnY - bh*0.5f, bw, bh,
                       0.9f, 0.9f, 0.9f, 0.8f);
        DrawRectFilled(gPauseBtnX + gap, gPauseBtnY - bh*0.5f, bw, bh,
                       0.9f, 0.9f, 0.9f, 0.8f);
    }

    // Draw RECTANGULAR toggle mode button (top-left) - clearly NOT a joystick
    {
        float tR, tG, tB;
        if (gControlMode == CONTROL_MODE_GYROSCOPE) {
            tR=0.15f; tG=0.25f; tB=0.60f;
        } else {
            tR=0.15f; tG=0.35f; tB=0.15f;
        }
        DrawRectFilled(gToggleBtnX, gToggleBtnY, gToggleBtnW, gToggleBtnH,
                       tR, tG, tB, 0.50f);
        DrawRectOutline(gToggleBtnX, gToggleBtnY, gToggleBtnW, gToggleBtnH,
                        tR*2.5f+0.2f, tG*2.5f+0.2f, tB*2.5f+0.2f, 0.75f);
        // Small icon inside: circle for gyro, cross for joystick
        float icx = gToggleBtnX + gToggleBtnW * 0.5f;
        float icy = gToggleBtnY + gToggleBtnH * 0.5f;
        float icr = gToggleBtnH * 0.28f;
        if (gControlMode == CONTROL_MODE_GYROSCOPE) {
            DrawCircleOutline(icx, icy, icr, 12, 0.6f, 0.8f, 1.0f, 0.9f);
        } else {
            DrawCircleOutline(icx, icy, icr, 12, 0.4f, 0.9f, 0.4f, 0.9f);
            // Center dot
            DrawCircleFilled(icx, icy, icr*0.35f, 8, 0.4f, 0.9f, 0.4f, 0.9f);
        }
    }

    // Draw RECTANGULAR recenter button (top-left, next to toggle)
    {
        DrawRectFilled(gRecenterBtnX, gRecenterBtnY, gRecenterBtnW, gRecenterBtnH,
                       0.35f, 0.12f, 0.12f, 0.50f);
        DrawRectOutline(gRecenterBtnX, gRecenterBtnY, gRecenterBtnW, gRecenterBtnH,
                        0.9f, 0.4f, 0.4f, 0.75f);
        // Center/reset icon: small crosshair
        float icx = gRecenterBtnX + gRecenterBtnW * 0.5f;
        float icy = gRecenterBtnY + gRecenterBtnH * 0.5f;
        float icr = gRecenterBtnH * 0.28f;
        DrawCircleOutline(icx, icy, icr, 12, 0.9f, 0.5f, 0.5f, 0.9f);
        DrawCircleFilled(icx, icy, icr*0.3f, 8, 0.9f, 0.5f, 0.5f, 0.9f);
    }

    // --------------------------------------------------------
    // RESTORE saved GL state so the game's next frame is unaffected
    // --------------------------------------------------------

    // Restore bridge-tracked states for each texture unit separately
    glActiveTexture(GL_TEXTURE1);
    if (savedTex1) bridge_Enable(TCGL_TEXTURE_2D);
    else           bridge_Disable(TCGL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    if (savedTex0) bridge_Enable(TCGL_TEXTURE_2D);
    else           bridge_Disable(TCGL_TEXTURE_2D);

    if (savedLighting)  bridge_Enable(TCGL_LIGHTING);  else bridge_Disable(TCGL_LIGHTING);
    if (savedFog)       bridge_Enable(TCGL_FOG);        else bridge_Disable(TCGL_FOG);
    if (savedAlphaTest) bridge_Enable(TCGL_ALPHA_TEST); else bridge_Disable(TCGL_ALPHA_TEST);

    // Restore real-GLES states
    if (savedDepthTest) glEnable(GL_DEPTH_TEST);  else glDisable(GL_DEPTH_TEST);
    if (savedCullFace)  glEnable(GL_CULL_FACE);   else glDisable(GL_CULL_FACE);
    if (savedBlend)     glEnable(GL_BLEND);        else glDisable(GL_BLEND);
}

#endif // __ANDROID__
