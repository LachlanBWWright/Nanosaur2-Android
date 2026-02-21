// gles_compat.h - OpenGL ES 3.0 compatibility layer for Nanosaur 2
// Redirects fixed-function OpenGL calls to the GLES bridge

#pragma once

#ifdef __ANDROID__

#include <GLES3/gl3.h>
#include <stdbool.h>

// GLdouble is not defined in GLES3
typedef double GLdouble;
typedef double GLclampd;

// Missing constants
#ifndef GL_BGRA
#define GL_BGRA  0x80E1
#endif
#ifndef GL_BGR
#define GL_BGR   0x80E0
#endif
#ifndef GL_CLAMP
#define GL_CLAMP GL_CLAMP_TO_EDGE
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE       0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif
#ifndef GL_INTENSITY
#define GL_INTENSITY       0x8049
#endif

// Primitive types not in GLES3
#define GL_QUADS           0x0007
#define GL_QUAD_STRIP      0x0008
#define GL_POLYGON         0x0009

// Fixed function state
#define GL_LIGHTING        0x0B50
#define GL_LIGHT0          0x4000
#define GL_LIGHT1          0x4001
#define GL_LIGHT2          0x4002
#define GL_LIGHT3          0x4003
#define GL_LIGHT4          0x4004
#define GL_LIGHT5          0x4005
#define GL_LIGHT6          0x4006
#define GL_LIGHT7          0x4007
#define GL_POSITION        0x1203
#define GL_AMBIENT         0x1200
#define GL_DIFFUSE         0x1201
#define GL_SPECULAR        0x1202
#define GL_EMISSION        0x1600
#define GL_SHININESS       0x1601
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_COLOR_MATERIAL  0x0B57
#define GL_NORMALIZE       0x0BA1

// Fog
#define GL_FOG             0x0B60
#define GL_FOG_MODE        0x0B65
#define GL_FOG_DENSITY     0x0B62
#define GL_FOG_START       0x0B63
#define GL_FOG_END         0x0B64
#define GL_FOG_COLOR       0x0B66
#define GL_LINEAR          0x2601
#define GL_EXP             0x0800
#define GL_EXP2            0x0801

// Alpha test
#define GL_ALPHA_TEST      0x0BC0
#define GL_NEVER           0x0200
#define GL_LESS            0x0201
#define GL_EQUAL           0x0202
#define GL_LEQUAL          0x0203
#define GL_GREATER         0x0204
#define GL_NOTEQUAL        0x0205
#define GL_GEQUAL          0x0206
#define GL_ALWAYS          0x0207

// Texture env
#define GL_TEXTURE_ENV     0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE        0x2100
#define GL_REPLACE         0x1E01
#define GL_DECAL           0x2101
#define GL_ADD             0x0104

// Texture gen
#define GL_TEXTURE_GEN_S   0x0C60
#define GL_TEXTURE_GEN_T   0x0C61
#define GL_TEXTURE_GEN_R   0x0C62
#define GL_TEXTURE_GEN_Q   0x0C63
#define GL_TEXTURE_GEN_MODE 0x2500
#define GL_SPHERE_MAP      0x2202

// Client state arrays
#define GL_VERTEX_ARRAY       0x8074
#define GL_NORMAL_ARRAY       0x8075
#define GL_COLOR_ARRAY        0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078

// Display lists
#define GL_COMPILE         0x1300
#define GL_COMPILE_AND_EXECUTE 0x1301

// Matrix modes
#define GL_MODELVIEW       0x1700
#define GL_PROJECTION      0x1701
#define GL_TEXTURE         0x1702

// Misc
#define GL_OBJECT_LINEAR   0x2401
#define GL_EYE_LINEAR      0x2400

// Texture coordinate identifiers for glTexGen
#define GL_S               0x2000
#define GL_T               0x2001
#define GL_R               0x2002
#define GL_Q               0x2003
#define GL_COMBINE         0x8570
#define GL_COMBINE_RGB     0x8571
#define GL_COMBINE_ALPHA   0x8572
#define GL_INTERPOLATE     0x8575
#define GL_CONSTANT        0x8576
#define GL_PRIMARY_COLOR   0x8577
#define GL_PREVIOUS        0x8578

// Missing from GLES3: polygon mode, draw buffer, attrib stack
#define GL_FRONT_AND_BACK  0x0408
#define GL_FRONT           0x0404
#define GL_BACK            0x0405
#define GL_LINE            0x1B01
#define GL_FILL            0x1B02
#define GL_POINT           0x1B00
#define GL_BACK_LEFT       0x0402
#define GL_BACK_RIGHT      0x0403
#define GL_ALL_ATTRIB_BITS 0xFFFFFFFF
// GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX
#define GL_MODELVIEW_MATRIX  0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7

// Fixed-function enables not in GLES3 (handled as no-ops in bridge)
#define GL_COLOR_LOGIC_OP          0x0BF2
#define GL_LINE_SMOOTH             0x0B20
#define GL_LINE_STIPPLE            0x0B24
#define GL_RESCALE_NORMAL          0x803A
#define GL_TEXTURE_1D              0x0DE0
#define GL_VERTEX_ARRAY_RANGE_APPLE 0x851D

// Missing GL error codes in GLES3
#define GL_STACK_OVERFLOW          0x0503
#define GL_STACK_UNDERFLOW         0x0504

// Bridge function declarations
#ifdef __cplusplus
extern "C" {
#endif

void GLES_Init(void);
void* GLES_ConvertTextureFormat(const void *pixels, int width, int height, int *internalFormat, int *format, int *type);

void bridge_Enable(GLenum cap);
void bridge_Disable(GLenum cap);
void bridge_MatrixMode(GLenum mode);
void bridge_PushMatrix(void);
void bridge_PopMatrix(void);
void bridge_LoadIdentity(void);
void bridge_LoadMatrixf(const float *m);
void bridge_MultMatrixf(const float *m);
void bridge_Translatef(float x, float y, float z);
void bridge_Rotatef(float angle, float x, float y, float z);
void bridge_Scalef(float x, float y, float z);
void bridge_Ortho(double l, double r, double b, double t, double n, double f);
void bridge_Frustum(double l, double r, double b, double t, double n, double f);
void bridge_Begin(GLenum mode);
void bridge_End(void);
void bridge_Vertex3f(float x, float y, float z);
void bridge_Vertex3fv(const float *v);
void bridge_Vertex2f(float x, float y);
void bridge_Normal3f(float x, float y, float z);
void bridge_TexCoord2f(float s, float t);
void bridge_TexCoord2fv(const float *v);
void bridge_Color4f(float r, float g, float b, float a);
void bridge_Color4fv(const float *c);
void bridge_Color4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void bridge_Color3f(float r, float g, float b);
void bridge_Color3fv(const float *c);
void bridge_Lightfv(GLenum light, GLenum pname, const float *params);
void bridge_Lightf(GLenum light, GLenum pname, float param);
void bridge_Lighti(GLenum light, GLenum pname, int param);
void bridge_Materialfv(GLenum face, GLenum pname, const float *params);
void bridge_Materialf(GLenum face, GLenum pname, float param);
void bridge_Fogfv(GLenum pname, const float *params);
void bridge_Fogf(GLenum pname, float param);
void bridge_Fogi(GLenum pname, int param);
void bridge_AlphaFunc(GLenum func, float ref);
void bridge_TexEnvi(GLenum target, GLenum pname, int param);
void bridge_TexEnvf(GLenum target, GLenum pname, float param);
void bridge_TexEnvfv(GLenum target, GLenum pname, const float *params);
void bridge_TexGeni(GLenum coord, GLenum pname, int param);
void bridge_TexGenf(GLenum coord, GLenum pname, float param);
void bridge_EnableClientState(GLenum array);
void bridge_DisableClientState(GLenum array);
void bridge_VertexPointer(int size, GLenum type, int stride, const void *ptr);
void bridge_NormalPointer(GLenum type, int stride, const void *ptr);
void bridge_TexCoordPointer(int size, GLenum type, int stride, const void *ptr);
void bridge_ColorPointer(int size, GLenum type, int stride, const void *ptr);
void bridge_DrawElements(GLenum mode, int count, GLenum indexType, const void *indices);
void bridge_NewList(unsigned int list, GLenum mode);
void bridge_EndList(void);
void bridge_CallList(unsigned int list);
void bridge_DeleteLists(unsigned int list, int range);
void bridge_PolygonMode(GLenum face, GLenum mode);
void bridge_GetDoublev(GLenum pname, double *params);
void bridge_ActiveTexture(GLenum texture);

#ifdef __cplusplus
}
#endif

// ============================================================
// NO-OP STUBS (inline, desktop-only functions not in GLES3)
// ============================================================

// glColorMaterial - no color material tracking needed (handled by state)
static inline void stub_ColorMaterial(GLenum face, GLenum mode) { (void)face; (void)mode; }
// glDrawBuffer - no stereo on Android
static inline void stub_DrawBuffer(GLenum buf) { (void)buf; }
// Attribute stack
static inline void stub_PushAttrib(GLbitfield mask) { (void)mask; }
static inline void stub_PopAttrib(void) {}
// Vertex array range (Apple extension)
#define PFNGLVERTEXARRAYRANGEAPPLEPROC void*
#define PFNGLFLUSHVERTEXARRAYRANGEAPPLEPROC void*
static inline void stub_FlushVertexArrayRangeAPPLE(GLsizei length, void *ptr) { (void)length; (void)ptr; }
// Raster pos
static inline void stub_RasterPos2f(float x, float y) { (void)x; (void)y; }
// glColorMask macro already available in GLES3 directly

// ============================================================
// REDIRECTS
// ============================================================

// Replace glEnable/glDisable to intercept fixed-function caps
#define glEnable            bridge_Enable
#define glDisable           bridge_Disable

#define glBegin             bridge_Begin
#define glEnd               bridge_End
#define glVertex3f          bridge_Vertex3f
#define glVertex3fv         bridge_Vertex3fv
#define glVertex2f          bridge_Vertex2f
#define glNormal3f          bridge_Normal3f
#define glTexCoord2f        bridge_TexCoord2f
#define glTexCoord2fv       bridge_TexCoord2fv
#define glColor4f           bridge_Color4f
#define glColor4fv          bridge_Color4fv
#define glColor4ub          bridge_Color4ub
#define glColor3f           bridge_Color3f
#define glColor3fv          bridge_Color3fv
#define glMatrixMode        bridge_MatrixMode
#define glPushMatrix        bridge_PushMatrix
#define glPopMatrix         bridge_PopMatrix
#define glLoadIdentity      bridge_LoadIdentity
#define glLoadMatrixf       bridge_LoadMatrixf
#define glMultMatrixf       bridge_MultMatrixf
#define glTranslatef        bridge_Translatef
#define glRotatef           bridge_Rotatef
#define glScalef            bridge_Scalef
#define glOrtho(l,r,b,t,n,f) bridge_Ortho((double)(l),(double)(r),(double)(b),(double)(t),(double)(n),(double)(f))
#define glFrustum(l,r,b,t,n,f) bridge_Frustum((double)(l),(double)(r),(double)(b),(double)(t),(double)(n),(double)(f))
#define glLightfv           bridge_Lightfv
#define glLightf            bridge_Lightf
#define glLighti            bridge_Lighti
#define glMaterialfv        bridge_Materialfv
#define glMaterialf         bridge_Materialf
#define glFogfv             bridge_Fogfv
#define glFogf              bridge_Fogf
#define glFogi              bridge_Fogi
#define glAlphaFunc         bridge_AlphaFunc
#define glTexEnvi           bridge_TexEnvi
#define glTexEnvf           bridge_TexEnvf
#define glTexEnvfv          bridge_TexEnvfv
#define glTexGeni           bridge_TexGeni
#define glTexGenf           bridge_TexGenf
#define glEnableClientState  bridge_EnableClientState
#define glDisableClientState bridge_DisableClientState
#define glVertexPointer      bridge_VertexPointer
#define glNormalPointer      bridge_NormalPointer
#define glTexCoordPointer    bridge_TexCoordPointer
#define glColorPointer       bridge_ColorPointer
#define glDrawElements(mode,count,type,indices) bridge_DrawElements(mode,count,type,indices)
#define glNewList           bridge_NewList
#define glEndList           bridge_EndList
#define glCallList          bridge_CallList
#define glDeleteLists       bridge_DeleteLists
#define glPolygonMode       bridge_PolygonMode
#define glGetDoublev        bridge_GetDoublev
#define glActiveTexture     bridge_ActiveTexture

// glClientActiveTexture - for multitexture vertex arrays, map to ActiveTexture on Android
#define glClientActiveTexture(t) bridge_ActiveTexture(t)

// Additional no-op stubs for desktop-only features
#define glColorMaterial      stub_ColorMaterial
#define glDrawBuffer         stub_DrawBuffer
#define glPushAttrib         stub_PushAttrib
#define glPopAttrib          stub_PopAttrib
#define glFlushVertexArrayRangeAPPLE stub_FlushVertexArrayRangeAPPLE
#define glRasterPos2f        stub_RasterPos2f

// Ensure GL_TEXTURE_2D is defined (may have been overridden above)
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif

#endif // __ANDROID__
