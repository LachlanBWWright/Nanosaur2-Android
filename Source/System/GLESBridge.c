// GLESBridge.c - OpenGL ES 3.0 compatibility bridge for Nanosaur 2
// Emulates the fixed-function OpenGL pipeline using GLES 3.0 shaders

#ifdef __ANDROID__

#include <GLES3/gl3.h>
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#define TAG "Nanosaur2"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ============================================================
// TYPES
// ============================================================

typedef float Mat4[16];

// ============================================================
// MATRIX STACKS
// ============================================================

#define MATRIX_STACK_DEPTH 32

static Mat4  gModelViewStack[MATRIX_STACK_DEPTH];
static int   gModelViewTop = 0;
static Mat4  gProjectionStack[MATRIX_STACK_DEPTH];
static int   gProjectionTop = 0;
static Mat4  gTextureStack[MATRIX_STACK_DEPTH];
static int   gTextureTop = 0;

static int   gMatrixMode = 0x1700; // GL_MODELVIEW

static Mat4 *CurrentStack(void) {
    if (gMatrixMode == 0x1701) return &gProjectionStack[gProjectionTop]; // GL_PROJECTION
    if (gMatrixMode == 0x1702) return &gTextureStack[gTextureTop];       // GL_TEXTURE
    return &gModelViewStack[gModelViewTop];
}

static void Mat4_Identity(Mat4 m) {
    memset(m, 0, sizeof(Mat4));
    m[0]=m[5]=m[10]=m[15]=1.0f;
}

static void Mat4_Multiply(Mat4 out, const Mat4 a, const Mat4 b) {
    Mat4 tmp;
    for (int r=0;r<4;r++)
        for (int c=0;c<4;c++) {
            tmp[c*4+r]=0;
            for (int k=0;k<4;k++)
                tmp[c*4+r]+=a[k*4+r]*b[c*4+k];
        }
    memcpy(out,tmp,sizeof(Mat4));
}

static void Mat4_Copy(Mat4 dst, const Mat4 src) {
    memcpy(dst,src,sizeof(Mat4));
}

static void Mat4_Inverse3x3Transpose(Mat4 out, const Mat4 m) {
    // Extract 3x3 and compute inverse transpose for normal matrix
    // For now, just copy (works correctly for uniform scaling)
    Mat4_Copy(out, m);
}

// ============================================================
// SHADER PROGRAM
// ============================================================

static GLuint gShaderProgram = 0;
static GLuint gVAO = 0, gVBO = 0, gIBO = 0;
static GLuint gImmVAO = 0, gImmVBO = 0;

// Uniform locations
static GLint u_mvpMatrix;
static GLint u_modelViewMatrix;
static GLint u_normalMatrix;
static GLint u_textureMatrix;
static GLint u_lightingEnabled;
static GLint u_texGenEnabled;
static GLint u_texGenMode;
static GLint u_globalColor;
static GLint u_lightEnabled[8];
static GLint u_lightAmbient[8];
static GLint u_lightDiffuse[8];
static GLint u_lightSpecular[8];
static GLint u_lightPosition[8];
static GLint u_matAmbient;
static GLint u_matDiffuse;
static GLint u_matSpecular;
static GLint u_matShininess;
static GLint u_matEmission;
static GLint u_sceneAmbient;
static GLint u_texture0;
static GLint u_texture1;
static GLint u_textureEnabled;
static GLint u_texture1Enabled;
static GLint u_texEnvMode;
static GLint u_alphaTestEnabled;
static GLint u_alphaFunc;
static GLint u_alphaRef;
static GLint u_fogEnabled;
static GLint u_fogMode;
static GLint u_fogStart;
static GLint u_fogEnd;
static GLint u_fogDensity;
static GLint u_fogColor;

// Attribute locations
static GLint a_position;
static GLint a_normal;
static GLint a_texcoord;
static GLint a_color;

static const char* kVertexShader =
"#version 300 es\n"
"precision mediump float;\n"
"uniform mat4 u_mvpMatrix;\n"
"uniform mat4 u_modelViewMatrix;\n"
"uniform mat4 u_normalMatrix;\n"
"uniform mat4 u_textureMatrix;\n"
"uniform bool u_lightingEnabled;\n"
"uniform bool u_texGenEnabled;\n"
"uniform int  u_texGenMode;\n"
"uniform vec4 u_globalColor;\n"
"uniform bool u_lightEnabled[8];\n"
"uniform vec4 u_lightAmbient[8];\n"
"uniform vec4 u_lightDiffuse[8];\n"
"uniform vec4 u_lightSpecular[8];\n"
"uniform vec4 u_lightPosition[8];\n"
"uniform vec4 u_matAmbient;\n"
"uniform vec4 u_matDiffuse;\n"
"uniform vec4 u_matSpecular;\n"
"uniform float u_matShininess;\n"
"uniform vec4 u_matEmission;\n"
"uniform vec4 u_sceneAmbient;\n"
"in vec3 a_position;\n"
"in vec3 a_normal;\n"
"in vec2 a_texcoord;\n"
"in vec4 a_color;\n"
"out vec4 v_color;\n"
"out vec2 v_texcoord;\n"
"void main() {\n"
"  vec4 eyePos = u_modelViewMatrix * vec4(a_position, 1.0);\n"
"  gl_Position = u_mvpMatrix * vec4(a_position, 1.0);\n"
"  if (u_lightingEnabled) {\n"
"    vec3 eyeNormal = normalize(mat3(u_normalMatrix) * a_normal);\n"
"    vec4 color = u_matEmission + u_sceneAmbient * u_matAmbient;\n"
"    for (int i = 0; i < 8; i++) {\n"
"      if (!u_lightEnabled[i]) continue;\n"
"      vec3 lightDir;\n"
"      if (u_lightPosition[i].w == 0.0) {\n"
"        lightDir = normalize(u_lightPosition[i].xyz);\n"
"      } else {\n"
"        lightDir = normalize(u_lightPosition[i].xyz - eyePos.xyz);\n"
"      }\n"
"      float diff = max(dot(eyeNormal, lightDir), 0.0);\n"
"      vec4 ambient = u_lightAmbient[i] * u_matAmbient;\n"
"      vec4 diffuse = u_lightDiffuse[i] * u_matDiffuse * diff;\n"
"      color += ambient + diffuse;\n"
"    }\n"
"    v_color = clamp(color, 0.0, 1.0);\n"
"    v_color.a = u_matDiffuse.a;\n"
"  } else {\n"
"    v_color = a_color * u_globalColor;\n"
"  }\n"
"  if (u_texGenEnabled) {\n"
"    vec3 eyeNorm = normalize(mat3(u_normalMatrix) * a_normal);\n"
"    vec3 eyePosN = normalize(eyePos.xyz);\n"
"    vec3 r = reflect(eyePosN, eyeNorm);\n"
"    float m = 2.0 * sqrt(r.x*r.x + r.y*r.y + (r.z+1.0)*(r.z+1.0));\n"
"    v_texcoord = vec2(r.x/m + 0.5, r.y/m + 0.5);\n"
"  } else {\n"
"    vec4 tc = u_textureMatrix * vec4(a_texcoord, 0.0, 1.0);\n"
"    v_texcoord = tc.xy;\n"
"  }\n"
"}\n";

static const char* kFragmentShader =
"#version 300 es\n"
"precision mediump float;\n"
"uniform sampler2D u_texture0;\n"
"uniform sampler2D u_texture1;\n"
"uniform bool u_textureEnabled;\n"
"uniform bool u_texture1Enabled;\n"
"uniform int  u_texEnvMode;\n"
"uniform bool u_alphaTestEnabled;\n"
"uniform int  u_alphaFunc;\n"
"uniform float u_alphaRef;\n"
"uniform bool u_fogEnabled;\n"
"uniform int  u_fogMode;\n"
"uniform float u_fogStart;\n"
"uniform float u_fogEnd;\n"
"uniform float u_fogDensity;\n"
"uniform vec4 u_fogColor;\n"
"in vec4 v_color;\n"
"in vec2 v_texcoord;\n"
"out vec4 fragColor;\n"
"void main() {\n"
"  vec4 color = v_color;\n"
"  if (u_textureEnabled) {\n"
"    vec4 texColor = texture(u_texture0, v_texcoord);\n"
"    if (u_texEnvMode == 1) {\n"
"      color = texColor;\n"
"    } else if (u_texEnvMode == 2) {\n"
"      color.rgb = mix(color.rgb, texColor.rgb, texColor.a);\n"
"    } else {\n"
"      color *= texColor;\n"
"    }\n"
"  }\n"
"  if (u_texture1Enabled) {\n"
"    vec4 tex1Color = texture(u_texture1, v_texcoord);\n"
"    color *= tex1Color;\n"
"  }\n"
"  if (u_alphaTestEnabled) {\n"
"    float a = color.a;\n"
"    float ref = u_alphaRef;\n"
"    bool pass = false;\n"
"    if      (u_alphaFunc == 0x0200) pass = false;\n"
"    else if (u_alphaFunc == 0x0201) pass = a < ref;\n"
"    else if (u_alphaFunc == 0x0202) pass = (abs(a - ref) < 0.001);\n"
"    else if (u_alphaFunc == 0x0203) pass = a <= ref;\n"
"    else if (u_alphaFunc == 0x0204) pass = a > ref;\n"
"    else if (u_alphaFunc == 0x0205) pass = !(abs(a - ref) < 0.001);\n"
"    else if (u_alphaFunc == 0x0206) pass = a >= ref;\n"
"    else if (u_alphaFunc == 0x0207) pass = true;\n"
"    if (!pass) discard;\n"
"  }\n"
"  if (u_fogEnabled) {\n"
"    float depth = gl_FragCoord.z / gl_FragCoord.w;\n"
"    float fogFactor = clamp((u_fogEnd - depth) / (u_fogEnd - u_fogStart), 0.0, 1.0);\n"
"    color.rgb = mix(u_fogColor.rgb, color.rgb, fogFactor);\n"
"  }\n"
"  fragColor = color;\n"
"}\n";

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        LOGE("Shader compile error: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLint GetUniformLoc(GLuint prog, const char* name) {
    GLint loc = glGetUniformLocation(prog, name);
    if (loc < 0) LOGI("Uniform not found: %s", name);
    return loc;
}

void GLES_Init(void) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) { LOGE("Failed to compile shaders"); return; }

    gShaderProgram = glCreateProgram();
    glAttachShader(gShaderProgram, vs);
    glAttachShader(gShaderProgram, fs);
    glLinkProgram(gShaderProgram);
    GLint ok; glGetProgramiv(gShaderProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(gShaderProgram, sizeof(log), NULL, log);
        LOGE("Program link error: %s", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(gShaderProgram);

    // Get uniform locations
    u_mvpMatrix        = GetUniformLoc(gShaderProgram, "u_mvpMatrix");
    u_modelViewMatrix  = GetUniformLoc(gShaderProgram, "u_modelViewMatrix");
    u_normalMatrix     = GetUniformLoc(gShaderProgram, "u_normalMatrix");
    u_textureMatrix    = GetUniformLoc(gShaderProgram, "u_textureMatrix");
    u_lightingEnabled  = GetUniformLoc(gShaderProgram, "u_lightingEnabled");
    u_texGenEnabled    = GetUniformLoc(gShaderProgram, "u_texGenEnabled");
    u_texGenMode       = GetUniformLoc(gShaderProgram, "u_texGenMode");
    u_globalColor      = GetUniformLoc(gShaderProgram, "u_globalColor");
    u_matAmbient       = GetUniformLoc(gShaderProgram, "u_matAmbient");
    u_matDiffuse       = GetUniformLoc(gShaderProgram, "u_matDiffuse");
    u_matSpecular      = GetUniformLoc(gShaderProgram, "u_matSpecular");
    u_matShininess     = GetUniformLoc(gShaderProgram, "u_matShininess");
    u_matEmission      = GetUniformLoc(gShaderProgram, "u_matEmission");
    u_sceneAmbient     = GetUniformLoc(gShaderProgram, "u_sceneAmbient");
    u_texture0         = GetUniformLoc(gShaderProgram, "u_texture0");
    u_texture1         = GetUniformLoc(gShaderProgram, "u_texture1");
    u_textureEnabled   = GetUniformLoc(gShaderProgram, "u_textureEnabled");
    u_texture1Enabled  = GetUniformLoc(gShaderProgram, "u_texture1Enabled");
    u_texEnvMode       = GetUniformLoc(gShaderProgram, "u_texEnvMode");
    u_alphaTestEnabled = GetUniformLoc(gShaderProgram, "u_alphaTestEnabled");
    u_alphaFunc        = GetUniformLoc(gShaderProgram, "u_alphaFunc");
    u_alphaRef         = GetUniformLoc(gShaderProgram, "u_alphaRef");
    u_fogEnabled       = GetUniformLoc(gShaderProgram, "u_fogEnabled");
    u_fogMode          = GetUniformLoc(gShaderProgram, "u_fogMode");
    u_fogStart         = GetUniformLoc(gShaderProgram, "u_fogStart");
    u_fogEnd           = GetUniformLoc(gShaderProgram, "u_fogEnd");
    u_fogDensity       = GetUniformLoc(gShaderProgram, "u_fogDensity");
    u_fogColor         = GetUniformLoc(gShaderProgram, "u_fogColor");

    char name[32];
    for (int i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "u_lightEnabled[%d]",  i); u_lightEnabled[i]  = GetUniformLoc(gShaderProgram, name);
        snprintf(name, sizeof(name), "u_lightAmbient[%d]",  i); u_lightAmbient[i]  = GetUniformLoc(gShaderProgram, name);
        snprintf(name, sizeof(name), "u_lightDiffuse[%d]",  i); u_lightDiffuse[i]  = GetUniformLoc(gShaderProgram, name);
        snprintf(name, sizeof(name), "u_lightSpecular[%d]", i); u_lightSpecular[i] = GetUniformLoc(gShaderProgram, name);
        snprintf(name, sizeof(name), "u_lightPosition[%d]", i); u_lightPosition[i] = GetUniformLoc(gShaderProgram, name);
    }

    // Attribute locations
    a_position = glGetAttribLocation(gShaderProgram, "a_position");
    a_normal   = glGetAttribLocation(gShaderProgram, "a_normal");
    a_texcoord = glGetAttribLocation(gShaderProgram, "a_texcoord");
    a_color    = glGetAttribLocation(gShaderProgram, "a_color");

    // Set texture unit uniforms
    glUniform1i(u_texture0, 0);
    glUniform1i(u_texture1, 1);

    // Initialize matrix stacks to identity
    Mat4_Identity(gModelViewStack[0]);
    Mat4_Identity(gProjectionStack[0]);
    Mat4_Identity(gTextureStack[0]);

    // Create VBOs and VAOs
    glGenVertexArrays(1, &gImmVAO);
    glGenBuffers(1, &gImmVBO);
    glGenVertexArrays(1, &gVAO);
    glGenBuffers(1, &gVBO);
    glGenBuffers(1, &gIBO);

    LOGI("GLES bridge initialized OK");
}

// ============================================================
// STATE
// ============================================================

static bool   gLightingEnabled = false;
static bool   gTexGenEnabled   = false;
static int    gTexGenModeVal   = 0x2197; // GL_SPHERE_MAP
static bool   gTexture0Enabled = false;
static bool   gTexture1Enabled = false;
static bool   gAlphaTestEnabled = false;
static int    gAlphaFuncVal    = 0x0207; // GL_ALWAYS
static float  gAlphaRefVal     = 0.0f;
static bool   gFogEnabled      = false;
static float  gFogStart        = 0.0f;
static float  gFogEnd          = 1.0f;
static float  gFogDensity      = 1.0f;
static float  gFogColor[4]     = {0,0,0,0};
static int    gFogModeVal      = 0; // linear
static int    gTexEnvModeVal   = 0; // modulate
static int    gActiveTexture   = 0;

static float gCurrentColor[4] = {1,1,1,1};
static float gCurrentNormal[3] = {0,0,1};
static float gCurrentTexCoord[2] = {0,0};

static float gMatAmbient[4]   = {0.2f, 0.2f, 0.2f, 1.0f};
static float gMatDiffuse[4]   = {0.8f, 0.8f, 0.8f, 1.0f};
static float gMatSpecular[4]  = {0.0f, 0.0f, 0.0f, 1.0f};
static float gMatEmission[4]  = {0.0f, 0.0f, 0.0f, 1.0f};
static float gMatShininess    = 0.0f;
static float gSceneAmbient[4] = {0.2f, 0.2f, 0.2f, 1.0f};

typedef struct {
    bool  enabled;
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float position[4];  // w=0: directional
} LightState;

static LightState gLights[8];

static void InitLightDefaults(void) {
    static bool done = false;
    if (done) return;
    done = true;
    for (int i = 0; i < 8; i++) {
        gLights[i].enabled    = false;
        gLights[i].ambient[0] = gLights[i].ambient[1] = gLights[i].ambient[2] = 0; gLights[i].ambient[3] = 1;
        gLights[i].diffuse[0] = gLights[i].diffuse[1] = gLights[i].diffuse[2] = (i==0?1:0); gLights[i].diffuse[3] = 1;
        gLights[i].specular[0]= gLights[i].specular[1]= gLights[i].specular[2]= (i==0?1:0); gLights[i].specular[3]= 1;
        gLights[i].position[0]= 0; gLights[i].position[1]= 0; gLights[i].position[2]= 1; gLights[i].position[3]= 0;
    }
}

// ============================================================
// UPLOAD MATRICES & STATE TO SHADER
// ============================================================

static void UploadState(void) {
    InitLightDefaults();
    // Compute MVP
    Mat4 mvp;
    Mat4_Multiply(mvp, gProjectionStack[gProjectionTop], gModelViewStack[gModelViewTop]);
    glUniformMatrix4fv(u_mvpMatrix, 1, GL_FALSE, mvp);
    glUniformMatrix4fv(u_modelViewMatrix, 1, GL_FALSE, gModelViewStack[gModelViewTop]);
    Mat4 nm;
    Mat4_Inverse3x3Transpose(nm, gModelViewStack[gModelViewTop]);
    glUniformMatrix4fv(u_normalMatrix, 1, GL_FALSE, nm);
    glUniformMatrix4fv(u_textureMatrix, 1, GL_FALSE, gTextureStack[gTextureTop]);

    glUniform1i(u_lightingEnabled, gLightingEnabled ? 1 : 0);
    glUniform1i(u_texGenEnabled,   gTexGenEnabled   ? 1 : 0);
    glUniform1i(u_texGenMode,      gTexGenModeVal);
    glUniform4fv(u_globalColor, 1, gCurrentColor);

    glUniform4fv(u_matAmbient,  1, gMatAmbient);
    glUniform4fv(u_matDiffuse,  1, gMatDiffuse);
    glUniform4fv(u_matSpecular, 1, gMatSpecular);
    glUniform4fv(u_matEmission, 1, gMatEmission);
    glUniform1f (u_matShininess, gMatShininess);
    glUniform4fv(u_sceneAmbient, 1, gSceneAmbient);

    for (int i = 0; i < 8; i++) {
        // Transform light position to eye space
        float ep[4];
        const float *lp = gLights[i].position;
        const float *mv = gModelViewStack[gModelViewTop];
        ep[0]=mv[0]*lp[0]+mv[4]*lp[1]+mv[8]*lp[2]+mv[12]*lp[3];
        ep[1]=mv[1]*lp[0]+mv[5]*lp[1]+mv[9]*lp[2]+mv[13]*lp[3];
        ep[2]=mv[2]*lp[0]+mv[6]*lp[1]+mv[10]*lp[2]+mv[14]*lp[3];
        ep[3]=lp[3];

        glUniform1i (u_lightEnabled[i],  gLights[i].enabled ? 1 : 0);
        glUniform4fv(u_lightAmbient[i],  1, gLights[i].ambient);
        glUniform4fv(u_lightDiffuse[i],  1, gLights[i].diffuse);
        glUniform4fv(u_lightSpecular[i], 1, gLights[i].specular);
        glUniform4fv(u_lightPosition[i], 1, ep);
    }

    glUniform1i(u_textureEnabled,  gTexture0Enabled ? 1 : 0);
    glUniform1i(u_texture1Enabled, gTexture1Enabled ? 1 : 0);
    glUniform1i(u_texEnvMode, gTexEnvModeVal);

    glUniform1i(u_alphaTestEnabled, gAlphaTestEnabled ? 1 : 0);
    glUniform1i(u_alphaFunc, gAlphaFuncVal);
    glUniform1f(u_alphaRef,  gAlphaRefVal);

    glUniform1i(u_fogEnabled, gFogEnabled ? 1 : 0);
    glUniform1i(u_fogMode,    gFogModeVal);
    glUniform1f(u_fogStart,   gFogStart);
    glUniform1f(u_fogEnd,     gFogEnd);
    glUniform1f(u_fogDensity, gFogDensity);
    glUniform4fv(u_fogColor,  1, gFogColor);
}

// ============================================================
// ENABLE / DISABLE
// ============================================================

// These get called via the normal glEnable/glDisable which GLES3 supports,
// EXCEPT for the ones we need to intercept.
void bridge_Enable(GLenum cap) {
    switch (cap) {
        case 0x0B50: gLightingEnabled = true; break;  // GL_LIGHTING
        case 0x4000: gLights[0].enabled = true; break; // GL_LIGHT0
        case 0x4001: gLights[1].enabled = true; break;
        case 0x4002: gLights[2].enabled = true; break;
        case 0x4003: gLights[3].enabled = true; break;
        case 0x4004: gLights[4].enabled = true; break;
        case 0x4005: gLights[5].enabled = true; break;
        case 0x4006: gLights[6].enabled = true; break;
        case 0x4007: gLights[7].enabled = true; break;
        case 0x0BC0: gAlphaTestEnabled = true; break; // GL_ALPHA_TEST
        case 0x0B60: gFogEnabled = true; break;       // GL_FOG
        case 0x0DE1: // GL_TEXTURE_2D
            if (gActiveTexture == 0) gTexture0Enabled = true;
            else gTexture1Enabled = true;
            break;
        case 0x0C60: gTexGenEnabled = true; break;    // GL_TEXTURE_GEN_S
        case 0x0C61: gTexGenEnabled = true; break;    // GL_TEXTURE_GEN_T
        // Desktop-only caps not in GLES3 - silently ignore
        case 0x0B57: break; // GL_COLOR_MATERIAL
        case 0x0BA1: break; // GL_NORMALIZE
        case 0x803A: break; // GL_RESCALE_NORMAL
        case 0x0BF2: break; // GL_COLOR_LOGIC_OP
        case 0x0B20: break; // GL_LINE_SMOOTH
        case 0x0B24: break; // GL_LINE_STIPPLE
        case 0x0DE0: break; // GL_TEXTURE_1D
        case 0x851D: break; // GL_VERTEX_ARRAY_RANGE_APPLE
        default: glEnable(cap); break;
    }
}

void bridge_Disable(GLenum cap) {
    switch (cap) {
        case 0x0B50: gLightingEnabled = false; break;
        case 0x4000: gLights[0].enabled = false; break;
        case 0x4001: gLights[1].enabled = false; break;
        case 0x4002: gLights[2].enabled = false; break;
        case 0x4003: gLights[3].enabled = false; break;
        case 0x4004: gLights[4].enabled = false; break;
        case 0x4005: gLights[5].enabled = false; break;
        case 0x4006: gLights[6].enabled = false; break;
        case 0x4007: gLights[7].enabled = false; break;
        case 0x0BC0: gAlphaTestEnabled = false; break;
        case 0x0B60: gFogEnabled = false; break;
        case 0x0DE1: // GL_TEXTURE_2D
            if (gActiveTexture == 0) gTexture0Enabled = false;
            else gTexture1Enabled = false;
            break;
        case 0x0C60: gTexGenEnabled = false; break;
        case 0x0C61: gTexGenEnabled = false; break;
        // Desktop-only caps not in GLES3 - silently ignore
        case 0x0B57: break; // GL_COLOR_MATERIAL
        case 0x0BA1: break; // GL_NORMALIZE
        case 0x803A: break; // GL_RESCALE_NORMAL
        case 0x0BF2: break; // GL_COLOR_LOGIC_OP
        case 0x0B20: break; // GL_LINE_SMOOTH
        case 0x0B24: break; // GL_LINE_STIPPLE
        case 0x0DE0: break; // GL_TEXTURE_1D
        case 0x851D: break; // GL_VERTEX_ARRAY_RANGE_APPLE
        default: glDisable(cap); break;
    }
}

// ============================================================
// MATRIX FUNCTIONS
// ============================================================

void bridge_MatrixMode(GLenum mode) {
    gMatrixMode = (int)mode;
}

void bridge_PushMatrix(void) {
    if (gMatrixMode == 0x1701) { // GL_PROJECTION
        if (gProjectionTop < MATRIX_STACK_DEPTH-1) {
            Mat4_Copy(gProjectionStack[gProjectionTop+1], gProjectionStack[gProjectionTop]);
            gProjectionTop++;
        }
    } else if (gMatrixMode == 0x1702) { // GL_TEXTURE
        if (gTextureTop < MATRIX_STACK_DEPTH-1) {
            Mat4_Copy(gTextureStack[gTextureTop+1], gTextureStack[gTextureTop]);
            gTextureTop++;
        }
    } else {
        if (gModelViewTop < MATRIX_STACK_DEPTH-1) {
            Mat4_Copy(gModelViewStack[gModelViewTop+1], gModelViewStack[gModelViewTop]);
            gModelViewTop++;
        }
    }
}

void bridge_PopMatrix(void) {
    if (gMatrixMode == 0x1701) {
        if (gProjectionTop > 0) gProjectionTop--;
    } else if (gMatrixMode == 0x1702) {
        if (gTextureTop > 0) gTextureTop--;
    } else {
        if (gModelViewTop > 0) gModelViewTop--;
    }
}

void bridge_LoadIdentity(void) {
    Mat4_Identity(*CurrentStack());
}

void bridge_LoadMatrixf(const float *m) {
    memcpy(*CurrentStack(), m, sizeof(Mat4));
}

void bridge_MultMatrixf(const float *m) {
    Mat4 cur;
    Mat4_Copy(cur, *CurrentStack());
    Mat4_Multiply(*CurrentStack(), cur, (const float*)m);
}

void bridge_Translatef(float x, float y, float z) {
    Mat4 t;
    Mat4_Identity(t);
    t[12]=x; t[13]=y; t[14]=z;
    Mat4 cur;
    Mat4_Copy(cur, *CurrentStack());
    Mat4_Multiply(*CurrentStack(), cur, t);
}

void bridge_Rotatef(float angle, float x, float y, float z) {
    float rad = angle * (3.14159265358979f / 180.0f);
    float c = cosf(rad), s = sinf(rad);
    float len = sqrtf(x*x+y*y+z*z);
    if (len > 0) { x/=len; y/=len; z/=len; }
    Mat4 r;
    r[ 0]=c+x*x*(1-c);   r[ 4]=x*y*(1-c)-z*s; r[ 8]=x*z*(1-c)+y*s; r[12]=0;
    r[ 1]=y*x*(1-c)+z*s; r[ 5]=c+y*y*(1-c);   r[ 9]=y*z*(1-c)-x*s; r[13]=0;
    r[ 2]=x*z*(1-c)-y*s; r[ 6]=y*z*(1-c)+x*s; r[10]=c+z*z*(1-c);   r[14]=0;
    r[ 3]=0;             r[ 7]=0;             r[11]=0;             r[15]=1;
    Mat4 cur;
    Mat4_Copy(cur, *CurrentStack());
    Mat4_Multiply(*CurrentStack(), cur, r);
}

void bridge_Scalef(float x, float y, float z) {
    Mat4 s;
    Mat4_Identity(s);
    s[0]=x; s[5]=y; s[10]=z;
    Mat4 cur;
    Mat4_Copy(cur, *CurrentStack());
    Mat4_Multiply(*CurrentStack(), cur, s);
}

void bridge_Ortho(double l, double r, double b, double t, double n, double f) {
    Mat4 m;
    Mat4_Identity(m);
    m[0]  = 2.0f/(float)(r-l);
    m[5]  = 2.0f/(float)(t-b);
    m[10] = -2.0f/(float)(f-n);
    m[12] = -(float)(r+l)/(float)(r-l);
    m[13] = -(float)(t+b)/(float)(t-b);
    m[14] = -(float)(f+n)/(float)(f-n);
    Mat4 cur;
    Mat4_Copy(cur, *CurrentStack());
    Mat4_Multiply(*CurrentStack(), cur, m);
}

void bridge_Frustum(double l, double r, double b, double t, double n, double f) {
    Mat4 m;
    memset(m, 0, sizeof(m));
    m[0]  = (float)(2.0*n/(r-l));
    m[5]  = (float)(2.0*n/(t-b));
    m[8]  = (float)((r+l)/(r-l));
    m[9]  = (float)((t+b)/(t-b));
    m[10] = (float)(-(f+n)/(f-n));
    m[11] = -1.0f;
    m[14] = (float)(-2.0*f*n/(f-n));
    Mat4 cur;
    Mat4_Copy(cur, *CurrentStack());
    Mat4_Multiply(*CurrentStack(), cur, m);
}

// ============================================================
// LIGHTING
// ============================================================

void bridge_Lightfv(GLenum light, GLenum pname, const float *params) {
    int idx = (int)(light - 0x4000);
    if (idx < 0 || idx >= 8) return;
    switch (pname) {
        case 0x1200: memcpy(gLights[idx].ambient,  params, 4*sizeof(float)); break; // GL_AMBIENT
        case 0x1201: memcpy(gLights[idx].diffuse,  params, 4*sizeof(float)); break; // GL_DIFFUSE
        case 0x1202: memcpy(gLights[idx].specular, params, 4*sizeof(float)); break; // GL_SPECULAR
        case 0x1203: memcpy(gLights[idx].position, params, 4*sizeof(float)); break; // GL_POSITION
    }
}

void bridge_Lightf(GLenum light, GLenum pname, float param) {
    (void)light; (void)pname; (void)param;
}

void bridge_Lighti(GLenum light, GLenum pname, int param) {
    (void)light; (void)pname; (void)param;
}

void bridge_Materialfv(GLenum face, GLenum pname, const float *params) {
    (void)face;
    switch (pname) {
        case 0x1200: memcpy(gMatAmbient,  params, 4*sizeof(float)); break; // GL_AMBIENT
        case 0x1201: memcpy(gMatDiffuse,  params, 4*sizeof(float)); break; // GL_DIFFUSE
        case 0x1202: memcpy(gMatSpecular, params, 4*sizeof(float)); break; // GL_SPECULAR
        case 0x1600: memcpy(gMatEmission, params, 4*sizeof(float)); break; // GL_EMISSION
        case 0x1601: gMatShininess = params[0]; break;
        case 0x1602: // GL_AMBIENT_AND_DIFFUSE
            memcpy(gMatAmbient, params, 4*sizeof(float));
            memcpy(gMatDiffuse, params, 4*sizeof(float));
            break;
    }
}

void bridge_Materialf(GLenum face, GLenum pname, float param) {
    float v[4] = {param, param, param, param};
    bridge_Materialfv(face, pname, v);
}

// ============================================================
// FOG
// ============================================================

void bridge_Fogfv(GLenum pname, const float *params) {
    switch (pname) {
        case 0x0B66: memcpy(gFogColor, params, 4*sizeof(float)); break; // GL_FOG_COLOR
    }
}

void bridge_Fogf(GLenum pname, float param) {
    switch (pname) {
        case 0x0B63: gFogStart   = param; break; // GL_FOG_START
        case 0x0B64: gFogEnd     = param; break; // GL_FOG_END
        case 0x0B62: gFogDensity = param; break; // GL_FOG_DENSITY
    }
}

void bridge_Fogi(GLenum pname, int param) {
    switch (pname) {
        case 0x0B65: // GL_FOG_MODE
            if      (param == 0x2601) gFogModeVal = 0; // GL_LINEAR
            else if (param == 0x0800) gFogModeVal = 1; // GL_EXP
            else if (param == 0x0801) gFogModeVal = 2; // GL_EXP2
            break;
        case 0x0B66: // GL_FOG_COLOR (int - ignore for now)
            break;
    }
}

// ============================================================
// ALPHA TEST
// ============================================================

void bridge_AlphaFunc(GLenum func, float ref) {
    gAlphaFuncVal = (int)func;
    gAlphaRefVal  = ref;
}

// ============================================================
// TEXTURE ENV
// ============================================================

void bridge_TexEnvi(GLenum target, GLenum pname, int param) {
    (void)target;
    if (pname == 0x2200) { // GL_TEXTURE_ENV_MODE
        if      (param == 0x2100) gTexEnvModeVal = 0; // GL_MODULATE
        else if (param == 0x1E01) gTexEnvModeVal = 1; // GL_REPLACE
        else if (param == 0x2101) gTexEnvModeVal = 2; // GL_DECAL
        else                      gTexEnvModeVal = 0;
    }
}

void bridge_TexEnvf(GLenum target, GLenum pname, float param) {
    (void)target; (void)pname; (void)param;
}

void bridge_TexEnvfv(GLenum target, GLenum pname, const float *params) {
    (void)target; (void)pname; (void)params;
}

// ============================================================
// TEX GEN
// ============================================================

void bridge_TexGeni(GLenum coord, GLenum pname, int param) {
    (void)coord;
    if (pname == 0x2500) { // GL_TEXTURE_GEN_MODE
        gTexGenModeVal = param;
    }
}

void bridge_TexGenf(GLenum coord, GLenum pname, float param) {
    (void)coord; (void)pname; (void)param;
}

// ============================================================
// IMMEDIATE MODE
// ============================================================

#define IMM_MAX_VERTS 65536

typedef struct {
    float pos[3];
    float normal[3];
    float texcoord[2];
    float color[4];
} ImmVertex;

static ImmVertex gImmBuffer[IMM_MAX_VERTS];
static int       gImmCount = 0;
static GLenum    gImmMode  = 0;

// Stride of ImmVertex in bytes
#define IMM_STRIDE (sizeof(ImmVertex))

void bridge_Begin(GLenum mode) {
    gImmMode  = mode;
    gImmCount = 0;
}

void bridge_Vertex3f(float x, float y, float z) {
    if (gImmCount >= IMM_MAX_VERTS) return;
    ImmVertex *v = &gImmBuffer[gImmCount++];
    v->pos[0]=x; v->pos[1]=y; v->pos[2]=z;
    memcpy(v->normal,   gCurrentNormal,   3*sizeof(float));
    memcpy(v->texcoord, gCurrentTexCoord, 2*sizeof(float));
    memcpy(v->color,    gCurrentColor,    4*sizeof(float));
}

void bridge_Vertex3fv(const float *v) {
    bridge_Vertex3f(v[0], v[1], v[2]);
}

void bridge_Vertex2f(float x, float y) {
    bridge_Vertex3f(x, y, 0.0f);
}

void bridge_Normal3f(float x, float y, float z) {
    gCurrentNormal[0]=x; gCurrentNormal[1]=y; gCurrentNormal[2]=z;
}

void bridge_TexCoord2f(float s, float t) {
    gCurrentTexCoord[0]=s; gCurrentTexCoord[1]=t;
}

void bridge_TexCoord2fv(const float *v) {
    gCurrentTexCoord[0]=v[0]; gCurrentTexCoord[1]=v[1];
}

void bridge_Color4f(float r, float g, float b, float a) {
    gCurrentColor[0]=r; gCurrentColor[1]=g; gCurrentColor[2]=b; gCurrentColor[3]=a;
}

void bridge_Color4fv(const float *c) {
    memcpy(gCurrentColor, c, 4*sizeof(float));
}

void bridge_Color4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    gCurrentColor[0]=r/255.0f; gCurrentColor[1]=g/255.0f; gCurrentColor[2]=b/255.0f; gCurrentColor[3]=a/255.0f;
}

void bridge_Color3f(float r, float g, float b) {
    gCurrentColor[0]=r; gCurrentColor[1]=g; gCurrentColor[2]=b; gCurrentColor[3]=1.0f;
}

void bridge_Color3fv(const float *c) {
    bridge_Color3f(c[0], c[1], c[2]);
}

// Convert quad indices to triangle indices (in-place on a temporary array)
static void FlushImmediate_Quads(void) {
    // 4 verts per quad -> 6 indices per quad (2 triangles)
    int quadCount = gImmCount / 4;
    int triCount  = quadCount * 6;

    unsigned short *indices = (unsigned short*)malloc(triCount * sizeof(unsigned short));
    if (!indices) return;

    for (int i = 0; i < quadCount; i++) {
        int base = i * 4;
        indices[i*6+0] = (unsigned short)(base+0);
        indices[i*6+1] = (unsigned short)(base+1);
        indices[i*6+2] = (unsigned short)(base+2);
        indices[i*6+3] = (unsigned short)(base+0);
        indices[i*6+4] = (unsigned short)(base+2);
        indices[i*6+5] = (unsigned short)(base+3);
    }

    glBindVertexArray(gImmVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gImmVBO);
    glBufferData(GL_ARRAY_BUFFER, gImmCount * IMM_STRIDE, gImmBuffer, GL_STREAM_DRAW);

    glEnableVertexAttribArray(a_position);
    glEnableVertexAttribArray(a_normal);
    glEnableVertexAttribArray(a_texcoord);
    glEnableVertexAttribArray(a_color);

    glVertexAttribPointer(a_position, 3, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, pos));
    glVertexAttribPointer(a_normal,   3, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, normal));
    glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, texcoord));
    glVertexAttribPointer(a_color,    4, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, color));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triCount * sizeof(unsigned short), indices, GL_STREAM_DRAW);

    UploadState();
    glDrawElements(GL_TRIANGLES, triCount, GL_UNSIGNED_SHORT, 0);

    free(indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void FlushImmediate_QuadStrip(void) {
    int numVerts = gImmCount;
    if (numVerts < 4) return;
    int quadCount = (numVerts - 2) / 2;
    int triCount  = quadCount * 6;

    unsigned short *indices = (unsigned short*)malloc(triCount * sizeof(unsigned short));
    if (!indices) return;

    for (int i = 0; i < quadCount; i++) {
        int base = i * 2;
        indices[i*6+0] = (unsigned short)(base+0);
        indices[i*6+1] = (unsigned short)(base+1);
        indices[i*6+2] = (unsigned short)(base+2);
        indices[i*6+3] = (unsigned short)(base+1);
        indices[i*6+4] = (unsigned short)(base+3);
        indices[i*6+5] = (unsigned short)(base+2);
    }

    glBindVertexArray(gImmVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gImmVBO);
    glBufferData(GL_ARRAY_BUFFER, gImmCount * IMM_STRIDE, gImmBuffer, GL_STREAM_DRAW);

    glEnableVertexAttribArray(a_position);
    glEnableVertexAttribArray(a_normal);
    glEnableVertexAttribArray(a_texcoord);
    glEnableVertexAttribArray(a_color);

    glVertexAttribPointer(a_position, 3, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, pos));
    glVertexAttribPointer(a_normal,   3, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, normal));
    glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, texcoord));
    glVertexAttribPointer(a_color,    4, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, color));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triCount * sizeof(unsigned short), indices, GL_STREAM_DRAW);

    UploadState();
    glDrawElements(GL_TRIANGLES, triCount, GL_UNSIGNED_SHORT, 0);

    free(indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void FlushImmediate_Direct(GLenum glMode) {
    if (gImmCount == 0) return;

    glBindVertexArray(gImmVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gImmVBO);
    glBufferData(GL_ARRAY_BUFFER, gImmCount * IMM_STRIDE, gImmBuffer, GL_STREAM_DRAW);

    glEnableVertexAttribArray(a_position);
    glEnableVertexAttribArray(a_normal);
    glEnableVertexAttribArray(a_texcoord);
    glEnableVertexAttribArray(a_color);

    glVertexAttribPointer(a_position, 3, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, pos));
    glVertexAttribPointer(a_normal,   3, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, normal));
    glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, texcoord));
    glVertexAttribPointer(a_color,    4, GL_FLOAT, GL_FALSE, IMM_STRIDE, (void*)offsetof(ImmVertex, color));

    UploadState();
    glDrawArrays(glMode, 0, gImmCount);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void bridge_End(void) {
    if (gImmCount == 0) return;
    glUseProgram(gShaderProgram);

    switch (gImmMode) {
        case 0x0007: FlushImmediate_Quads();                   break; // GL_QUADS
        case 0x0008: FlushImmediate_QuadStrip();               break; // GL_QUAD_STRIP
        case 0x0009: FlushImmediate_Direct(GL_TRIANGLE_FAN);   break; // GL_POLYGON -> triangle fan
        case 0x0004: FlushImmediate_Direct(GL_TRIANGLES);      break; // GL_TRIANGLES
        case 0x0005: FlushImmediate_Direct(GL_TRIANGLE_STRIP); break; // GL_TRIANGLE_STRIP
        case 0x0006: FlushImmediate_Direct(GL_TRIANGLE_FAN);   break; // GL_TRIANGLE_FAN
        case 0x0001: FlushImmediate_Direct(GL_LINES);          break; // GL_LINES
        case 0x0002: FlushImmediate_Direct(GL_LINE_STRIP);     break; // GL_LINE_STRIP
        case 0x0003: FlushImmediate_Direct(GL_LINE_LOOP);      break; // GL_LINE_LOOP
        case 0x0000: FlushImmediate_Direct(GL_POINTS);         break; // GL_POINTS
        default:     FlushImmediate_Direct(GL_TRIANGLES);      break;
    }
    gImmCount = 0;
}

// ============================================================
// VERTEX ARRAYS
// ============================================================

typedef struct {
    int     size;
    GLenum  type;
    int     stride;
    const void *ptr;
    bool    enabled;
} VertexArrayState;

static VertexArrayState gVAState[4] = {0}; // 0=position, 1=normal, 2=texcoord, 3=color

void bridge_EnableClientState(GLenum array) {
    switch (array) {
        case 0x8074: gVAState[0].enabled = true; break; // GL_VERTEX_ARRAY
        case 0x8075: gVAState[1].enabled = true; break; // GL_NORMAL_ARRAY
        case 0x8078: gVAState[2].enabled = true; break; // GL_TEXTURE_COORD_ARRAY
        case 0x8076: gVAState[3].enabled = true; break; // GL_COLOR_ARRAY
    }
}

void bridge_DisableClientState(GLenum array) {
    switch (array) {
        case 0x8074: gVAState[0].enabled = false; break;
        case 0x8075: gVAState[1].enabled = false; break;
        case 0x8078: gVAState[2].enabled = false; break;
        case 0x8076: gVAState[3].enabled = false; break;
    }
}

void bridge_VertexPointer(int size, GLenum type, int stride, const void *ptr) {
    gVAState[0].size=size; gVAState[0].type=type; gVAState[0].stride=stride; gVAState[0].ptr=ptr;
}

void bridge_NormalPointer(GLenum type, int stride, const void *ptr) {
    gVAState[1].size=3; gVAState[1].type=type; gVAState[1].stride=stride; gVAState[1].ptr=ptr;
}

void bridge_TexCoordPointer(int size, GLenum type, int stride, const void *ptr) {
    gVAState[2].size=size; gVAState[2].type=type; gVAState[2].stride=stride; gVAState[2].ptr=ptr;
}

void bridge_ColorPointer(int size, GLenum type, int stride, const void *ptr) {
    gVAState[3].size=size; gVAState[3].type=type; gVAState[3].stride=stride; gVAState[3].ptr=ptr;
}

static int TypeSize(GLenum type) {
    switch (type) {
        case GL_FLOAT:          return 4;
        case GL_UNSIGNED_BYTE:  return 1;
        case GL_SHORT:          return 2;
        case GL_UNSIGNED_SHORT: return 2;
        case GL_INT:            return 4;
        default: return 4;
    }
}

static int EffectiveStride(const VertexArrayState *s) {
    if (s->stride != 0) return s->stride;
    return s->size * TypeSize(s->type);
}

void bridge_DrawElements(GLenum mode, int count, GLenum indexType, const void *indices) {
    if (!gVAState[0].enabled || !gVAState[0].ptr) return;

    // Find the max vertex index to know how much vertex data to upload
    int maxIndex = 0;
    if (indexType == GL_UNSIGNED_SHORT) {
        const unsigned short *idx = (const unsigned short*)indices;
        for (int i = 0; i < count; i++)
            if (idx[i] > maxIndex) maxIndex = idx[i];
    } else if (indexType == GL_UNSIGNED_INT) {
        const unsigned int *idx = (const unsigned int*)indices;
        for (int i = 0; i < count; i++)
            if ((int)idx[i] > maxIndex) maxIndex = (int)idx[i];
    } else if (indexType == GL_UNSIGNED_BYTE) {
        const unsigned char *idx = (const unsigned char*)indices;
        for (int i = 0; i < count; i++)
            if (idx[i] > maxIndex) maxIndex = idx[i];
    }
    int numVerts = maxIndex + 1;

    // Build an interleaved buffer: pos(3f) + normal(3f) + texcoord(2f) + color(4f) = 12 floats = 48 bytes
    int vertStride = 12 * sizeof(float);
    float *vbuf = (float*)malloc(numVerts * vertStride);
    if (!vbuf) return;

    for (int i = 0; i < numVerts; i++) {
        float *dst = vbuf + i * 12;
        // position
        if (gVAState[0].enabled && gVAState[0].ptr) {
            int stride = EffectiveStride(&gVAState[0]);
            const char *src = (const char*)gVAState[0].ptr + i * stride;
            if (gVAState[0].type == GL_FLOAT) {
                const float *sf = (const float*)src;
                dst[0]=sf[0]; dst[1]=sf[1]; dst[2]=(gVAState[0].size>=3?sf[2]:0.0f);
            } else { dst[0]=dst[1]=dst[2]=0; }
        } else { dst[0]=dst[1]=dst[2]=0; }
        // normal
        if (gVAState[1].enabled && gVAState[1].ptr) {
            int stride = EffectiveStride(&gVAState[1]);
            const char *src = (const char*)gVAState[1].ptr + i * stride;
            if (gVAState[1].type == GL_FLOAT) {
                const float *sf = (const float*)src;
                dst[3]=sf[0]; dst[4]=sf[1]; dst[5]=sf[2];
            } else { dst[3]=0; dst[4]=0; dst[5]=1; }
        } else { dst[3]=gCurrentNormal[0]; dst[4]=gCurrentNormal[1]; dst[5]=gCurrentNormal[2]; }
        // texcoord
        if (gVAState[2].enabled && gVAState[2].ptr) {
            int stride = EffectiveStride(&gVAState[2]);
            const char *src = (const char*)gVAState[2].ptr + i * stride;
            if (gVAState[2].type == GL_FLOAT) {
                const float *sf = (const float*)src;
                dst[6]=sf[0]; dst[7]=sf[1];
            } else { dst[6]=dst[7]=0; }
        } else { dst[6]=gCurrentTexCoord[0]; dst[7]=gCurrentTexCoord[1]; }
        // color
        if (gVAState[3].enabled && gVAState[3].ptr) {
            int stride = EffectiveStride(&gVAState[3]);
            const char *src = (const char*)gVAState[3].ptr + i * stride;
            if (gVAState[3].type == GL_FLOAT) {
                const float *sf = (const float*)src;
                dst[8]=sf[0]; dst[9]=sf[1]; dst[10]=sf[2]; dst[11]=(gVAState[3].size>=4?sf[3]:1.0f);
            } else if (gVAState[3].type == GL_UNSIGNED_BYTE) {
                const unsigned char *sb = (const unsigned char*)src;
                dst[8]=sb[0]/255.0f; dst[9]=sb[1]/255.0f; dst[10]=sb[2]/255.0f; dst[11]=(gVAState[3].size>=4?sb[3]/255.0f:1.0f);
            } else { dst[8]=dst[9]=dst[10]=dst[11]=1; }
        } else { dst[8]=gCurrentColor[0]; dst[9]=gCurrentColor[1]; dst[10]=gCurrentColor[2]; dst[11]=gCurrentColor[3]; }
    }

    glUseProgram(gShaderProgram);
    glBindVertexArray(gVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gVBO);
    glBufferData(GL_ARRAY_BUFFER, numVerts * vertStride, vbuf, GL_STREAM_DRAW);
    free(vbuf);

    glEnableVertexAttribArray(a_position);
    glEnableVertexAttribArray(a_normal);
    glEnableVertexAttribArray(a_texcoord);
    glEnableVertexAttribArray(a_color);

    glVertexAttribPointer(a_position, 3, GL_FLOAT, GL_FALSE, vertStride, (void*)(0*sizeof(float)));
    glVertexAttribPointer(a_normal,   3, GL_FLOAT, GL_FALSE, vertStride, (void*)(3*sizeof(float)));
    glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, GL_FALSE, vertStride, (void*)(6*sizeof(float)));
    glVertexAttribPointer(a_color,    4, GL_FLOAT, GL_FALSE, vertStride, (void*)(8*sizeof(float)));

    // Upload index data to IBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIBO);
    size_t indexSize = 0;
    switch (indexType) {
        case GL_UNSIGNED_SHORT: indexSize = count * sizeof(unsigned short); break;
        case GL_UNSIGNED_INT:   indexSize = count * sizeof(unsigned int);   break;
        case GL_UNSIGNED_BYTE:  indexSize = count * sizeof(unsigned char);  break;
        default: indexSize = count * sizeof(unsigned short); break;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexSize, indices, GL_STREAM_DRAW);

    UploadState();
    glDrawElements(mode, count, indexType, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ============================================================
// NO-OP STUBS
// ============================================================

void bridge_NewList(unsigned int list, GLenum mode)    { (void)list; (void)mode; }
void bridge_EndList(void)                              {}
void bridge_CallList(unsigned int list)                { (void)list; }
void bridge_DeleteLists(unsigned int list, int range)  { (void)list; (void)range; }
void bridge_PolygonMode(GLenum face, GLenum mode)      { (void)face; (void)mode; }

// ============================================================
// MISC
// ============================================================

void bridge_GetDoublev(GLenum pname, double *params) {
    // Return something sensible for common queries
    float fparams[16];
    switch (pname) {
        case 0x0BA6: // GL_MODELVIEW_MATRIX
            memcpy(fparams, gModelViewStack[gModelViewTop], 16*sizeof(float));
            for(int i=0;i<16;i++) params[i]=(double)fparams[i];
            break;
        case 0x0BA7: // GL_PROJECTION_MATRIX
            memcpy(fparams, gProjectionStack[gProjectionTop], 16*sizeof(float));
            for(int i=0;i<16;i++) params[i]=(double)fparams[i];
            break;
        default:
            params[0] = 0.0;
            break;
    }
}

// ============================================================
// ACTIVE TEXTURE WRAPPER
// ============================================================

void bridge_ActiveTexture(GLenum texture) {
    gActiveTexture = (int)(texture - 0x84C0); // GL_TEXTURE0 = 0x84C0
    glActiveTexture(texture);
}

// ============================================================
// TEXTURE FORMAT CONVERSION HELPER
// ============================================================

// Returns a malloc'd RGBA buffer if conversion needed, or NULL if src can be used as-is.
// Call free() on the return value if non-NULL.
// Also adjusts *internalFormat, *format, *type to the converted values.
void* GLES_ConvertTextureFormat(
    const void *pixels, int width, int height,
    int *internalFormat, int *format, int *type)
{
    // GLES3 doesn't support GL_LUMINANCE, GL_LUMINANCE_ALPHA as internal formats
    if (*internalFormat == 0x1909) { // GL_LUMINANCE
        *internalFormat = GL_R8;
        *format = GL_RED;
        *type = GL_UNSIGNED_BYTE;
        return NULL; // no need to convert data, just change the format
    }
    if (*internalFormat == 0x190A) { // GL_LUMINANCE_ALPHA
        *internalFormat = GL_RG8;
        *format = GL_RG;
        *type = GL_UNSIGNED_BYTE;
        return NULL;
    }
    // GL_BGRA + GL_UNSIGNED_INT_8_8_8_8_REV -> GL_RGBA + GL_UNSIGNED_BYTE
    if (*format == 0x80E1 && *type == 0x8367) { // GL_BGRA + GL_UNSIGNED_INT_8_8_8_8_REV
        int n = width * height;
        unsigned char *dst = (unsigned char*)malloc(n * 4);
        const unsigned char *src = (const unsigned char*)pixels;
        for (int i = 0; i < n; i++) {
            // 8_8_8_8_REV: stored as ARGB in memory on little-endian
            dst[i*4+0] = src[i*4+2]; // R
            dst[i*4+1] = src[i*4+1]; // G
            dst[i*4+2] = src[i*4+0]; // B
            dst[i*4+3] = src[i*4+3]; // A
        }
        *internalFormat = GL_RGBA;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        return dst;
    }
    // GL_BGRA + GL_UNSIGNED_BYTE -> GL_RGBA + GL_UNSIGNED_BYTE
    if (*format == 0x80E1 && *type == GL_UNSIGNED_BYTE) {
        int n = width * height;
        unsigned char *dst = (unsigned char*)malloc(n * 4);
        const unsigned char *src = (const unsigned char*)pixels;
        for (int i = 0; i < n; i++) {
            dst[i*4+0] = src[i*4+2]; // R
            dst[i*4+1] = src[i*4+1]; // G
            dst[i*4+2] = src[i*4+0]; // B
            dst[i*4+3] = src[i*4+3]; // A
        }
        *internalFormat = GL_RGBA;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        return dst;
    }
    // GL_RGBA + GL_UNSIGNED_INT_8_8_8_8_REV -> GL_RGBA + GL_UNSIGNED_BYTE
    // Big-endian Mac stored RGBA+8_8_8_8_REV as [A,B,G,R] bytes per pixel.
    // Rearrange to [R,G,B,A] for GLES3.
    if (*format == GL_RGBA && *type == 0x8367) { // 0x8367 = GL_UNSIGNED_INT_8_8_8_8_REV
        int n = width * height;
        unsigned char *dst = (unsigned char*)malloc(n * 4);
        const unsigned char *src = (const unsigned char*)pixels;
        for (int i = 0; i < n; i++) {
            dst[i*4+0] = src[i*4+3]; // R ← byte[3]
            dst[i*4+1] = src[i*4+2]; // G ← byte[2]
            dst[i*4+2] = src[i*4+1]; // B ← byte[1]
            dst[i*4+3] = src[i*4+0]; // A ← byte[0]
        }
        *internalFormat = GL_RGBA;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        return dst;
    }
    // GL_RGBA + GL_UNSIGNED_SHORT_1_5_5_5_REV -> GL_RGBA + GL_UNSIGNED_BYTE
    // GLES3 does not support the REV variant; expand to RGBA bytes.
    if (*format == GL_RGBA && *type == 0x8366) { // 0x8366 = GL_UNSIGNED_SHORT_1_5_5_5_REV
        int n = width * height;
        unsigned char *dst = (unsigned char*)malloc(n * 4);
        const unsigned short *src = (const unsigned short*)pixels;
        for (int i = 0; i < n; i++) {
            unsigned short px = src[i];
            // Expand 5-bit channels to 8-bit using bit replication (v<<3)|(v>>2)
            unsigned int r = (px >> 11) & 0x1F; dst[i*4+0] = (unsigned char)((r << 3) | (r >> 2));
            unsigned int g = (px >>  6) & 0x1F; dst[i*4+1] = (unsigned char)((g << 3) | (g >> 2));
            unsigned int b = (px >>  1) & 0x1F; dst[i*4+2] = (unsigned char)((b << 3) | (b >> 2));
            dst[i*4+3] = (unsigned char)((px & 0x01) ? 255 : 0);           // A
        }
        *internalFormat = GL_RGBA;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        return dst;
    }
    return NULL;
}

// ============================================================
// GETFLOATV / GETINTEGERV BRIDGE
// ============================================================

void bridge_GetFloatv(GLenum pname, float *params) {
    switch (pname) {
        case 0x0BA6: // GL_MODELVIEW_MATRIX
            memcpy(params, gModelViewStack[gModelViewTop], 16*sizeof(float));
            break;
        case 0x0BA7: // GL_PROJECTION_MATRIX
            memcpy(params, gProjectionStack[gProjectionTop], 16*sizeof(float));
            break;
        case 0x0BA8: // GL_TEXTURE_MATRIX
            memcpy(params, gTextureStack[gTextureTop], 16*sizeof(float));
            break;
        default:
            glGetFloatv(pname, params);
            break;
    }
}

void bridge_GetIntegerv(GLenum pname, int *params) {
    switch (pname) {
        case 0x0BA0: // GL_MATRIX_MODE
            params[0] = gMatrixMode;
            break;
        default:
            glGetIntegerv(pname, params);
            break;
    }
}

// ============================================================
// ISENABLE BRIDGE - for bridge-tracked states
// ============================================================

GLboolean bridge_IsEnabled(GLenum cap) {
    switch (cap) {
        case 0x0B50: return gLightingEnabled ? GL_TRUE : GL_FALSE;   // GL_LIGHTING
        case 0x0BC0: return gAlphaTestEnabled ? GL_TRUE : GL_FALSE;  // GL_ALPHA_TEST
        case 0x0B60: return gFogEnabled ? GL_TRUE : GL_FALSE;        // GL_FOG
        case 0x0DE1: return gTexture0Enabled ? GL_TRUE : GL_FALSE;   // GL_TEXTURE_2D
        case 0x0C60:
        case 0x0C61: return gTexGenEnabled ? GL_TRUE : GL_FALSE;     // GL_TEXTURE_GEN_S/T
        // Desktop-only caps not in GLES3: return false without causing GL error
        case 0x0BA1: return GL_FALSE;  // GL_NORMALIZE
        case 0x803A: return GL_FALSE;  // GL_RESCALE_NORMAL
        case 0x0B57: return GL_FALSE;  // GL_COLOR_MATERIAL
        case 0x0B20: return GL_FALSE;  // GL_LINE_SMOOTH
        case 0x0B24: return GL_FALSE;  // GL_LINE_STIPPLE
        case 0x0BF2: return GL_FALSE;  // GL_COLOR_LOGIC_OP
        case 0x0DE0: return GL_FALSE;  // GL_TEXTURE_1D
        // GLES3-supported caps - delegate to GL
        default: return glIsEnabled(cap);
    }
}

#endif // __ANDROID__
