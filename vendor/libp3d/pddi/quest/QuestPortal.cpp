#include "pddi/quest/QuestPortal.h"
#include "pddi/quest/QuestSlateText.h"
#include <algorithm>
#include <string>
#include <cstdio>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <vector>
#include <cstring>

namespace questxr {
namespace {
GLuint frameProgram = 0, frameVAO = 0, frameVBO = 0;
GLint frameMVP = -1,frameEditing=-1;
GLuint alphaProgram=0;
GLint alphaPose=-1,alphaFov=-1,alphaBounds=-1,alphaPlane=-1;
GLsizei vertexCount = 0;
std::string slateScene="MAIN MENU";
unsigned slateTake=1;
bool slateDirty=true;
struct FrameVertex { float x, y, r, g, b, material; };

GLuint Compile(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char message[2048];
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        __android_log_print(ANDROID_LOG_ERROR, "ReChanXR", "Clapperboard shader: %s", message);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void Rect(std::vector<FrameVertex>& vertices, float x, float y, float w, float h,
          float material = 1, float ink = 0.87f) {
    const FrameVertex corners[] = {
        {x,y,ink,ink,ink,material}, {x+w,y,ink,ink,ink,material},
        {x+w,y+h,ink,ink,ink,material}, {x,y+h,ink,ink,ink,material}
    };
    for (const int index : {0,1,2,0,2,3}) vertices.push_back(corners[index]);
}

// Code-native lettering keeps the slate independent of game assets/fonts.
void Text(std::vector<FrameVertex>& vertices, const char* text, float x, float top, float pixel) {
    for (const char* c = text; *c; ++c, x += 6 * pixel) {
        const char* glyph = SlateGlyph(*c);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (glyph[row * 5 + col] == '1')
                    Rect(vertices, x + col*pixel, top-(row+1)*pixel, pixel*.88f, pixel*.88f);
    }
}
void UploadFrameGeometry(){
    if(!frameVBO)return;
    std::vector<FrameVertex> vertices;
    Rect(vertices,-kSlateHalfWidth,kSlateBottom,2*kSlateHalfWidth,kSlateTop-kSlateBottom,0);
    Rect(vertices,-1.5f,-1.405f,3.0f,.006f);
    Text(vertices,"JACKIE CHAN",-1.48f,-1.28f,.013f);
    Text(vertices,"STUNTMASTER",.45f,-1.28f,.013f);
    const std::string scene="SCENE "+slateScene;
    char take[32];std::snprintf(take,sizeof(take),"TAKE %02u",slateTake);
    Text(vertices,scene.c_str(),-1.48f,-1.43f,std::min(.007f,1.8f/(6*std::max(size_t(1),scene.size()))));
    Text(vertices,take,.45f,-1.43f,.007f);
    vertexCount=GLsizei(vertices.size());
    GLint oldBuffer=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);
    glBindBuffer(GL_ARRAY_BUFFER,frameVBO);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(FrameVertex),vertices.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);slateDirty=false;
}

}

bool InitializePortalFrame() {
    const char* vertex = R"(#version 300 es
precision highp float;
layout(location=0) in vec2 position;
layout(location=1) in vec3 ink;
layout(location=2) in float material;
uniform mat4 uFrameMVP;
out vec2 vLocal;
out vec3 vInk;
flat out float vMaterial;
void main() {
    vLocal=position; vInk=ink; vMaterial=material;
    // Ink sits 1 mm above the board: coplanar depth interpolation flickers.
    float inkOffset=material>0.5 ? 0.001 : 0.0;
    gl_Position=uFrameMVP*vec4(position,-2.398+inkOffset,1.0);
}
)";
    const char* fragment = R"(#version 300 es
precision highp float;
in vec2 vLocal;
in vec3 vInk;
flat in float vMaterial;
uniform bool uEditing;
out vec4 colour;
void main() {
    if(vMaterial>0.5) { colour=vec4(vInk,1.0); return; }
    vec2 p=vLocal;
    if(abs(p.x)<1.6 && abs(p.y)<1.2) discard;
    vec3 paint=vec3(0.022,0.027,0.033);
    // Thin painted outer and aperture edges emphasize that the slate is flat.
    if(abs(p.x)>1.80 || p.y>1.60 || p.y< -1.49) paint=vec3(0.55,0.57,0.58);
    if(abs(p.x)<1.635 && abs(p.y)<1.235) paint=vec3(0.77,0.78,0.76);
    // Two aligned diagonal-striped clapper strips, separated by a dark hinge gap.
    if((p.y>1.34 && p.y<1.57) || (p.y>1.245 && p.y<1.305)) {
        float band=mod(floor((p.x+p.y*0.8+2.5)/0.27),2.0);
        paint=mix(vec3(0.018),vec3(0.9,0.89,0.84),band);
    }
    if(length(p-vec2(-1.715,1.285))<0.025) paint=vec3(0.65);
    if(length(p-vec2(-1.715,1.285))<0.011) paint=vec3(0.08);
    if(uEditing&&(abs(p.x)>1.77||p.y>1.57||p.y< -1.46)) paint=vec3(0.08,0.8,1.0);
    colour=vec4(paint,1.0);
}
)";
    const GLuint vs = Compile(GL_VERTEX_SHADER, vertex);
    const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    frameProgram = glCreateProgram();
    glAttachShader(frameProgram, vs); glAttachShader(frameProgram, fs); glLinkProgram(frameProgram);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(frameProgram, GL_LINK_STATUS, &ok);
    if (!ok) { ShutdownPortalFrame(); return false; }
    frameMVP = glGetUniformLocation(frameProgram, "uFrameMVP");
    frameEditing=glGetUniformLocation(frameProgram,"uEditing");
    const char* alphaVertex=R"(#version 300 es
precision highp float;
out vec2 uv;
void main(){
    vec2 p=gl_VertexID==0?vec2(-1,-1):(gl_VertexID==1?vec2(3,-1):vec2(-1,3));
    uv=(p+1.0)*0.5;gl_Position=vec4(p,0,1);
})";
    const char* alphaFragment=R"(#version 300 es
precision highp float;
in vec2 uv;
uniform mat4 uPortalFromEye;
uniform vec4 uFov;
uniform vec4 uBounds;
uniform float uPlaneZ;
out vec4 colour;
void main(){
    vec3 eye=uPortalFromEye[3].xyz;
    vec3 ray=mat3(uPortalFromEye)*vec3(mix(uFov.x,uFov.y,uv.x),mix(uFov.z,uFov.w,uv.y),-1.0);
    float alpha=0.0;
    if(abs(ray.z)>0.000001){
        float t=(uPlaneZ-eye.z)/ray.z;
        vec2 hit=eye.xy+t*ray.xy;
        if(t>0.0 && all(greaterThanEqual(hit,uBounds.xy)) && all(lessThanEqual(hit,uBounds.zw)))alpha=1.0;
    }
    colour=vec4(0,0,0,alpha);
})";
    const GLuint av=Compile(GL_VERTEX_SHADER,alphaVertex),af=Compile(GL_FRAGMENT_SHADER,alphaFragment);
    if(!av||!af){if(av)glDeleteShader(av);if(af)glDeleteShader(af);ShutdownPortalFrame();return false;}
    alphaProgram=glCreateProgram();glAttachShader(alphaProgram,av);glAttachShader(alphaProgram,af);glLinkProgram(alphaProgram);
    glDeleteShader(av);glDeleteShader(af);glGetProgramiv(alphaProgram,GL_LINK_STATUS,&ok);
    if(!ok){ShutdownPortalFrame();return false;}
    alphaPose=glGetUniformLocation(alphaProgram,"uPortalFromEye");alphaFov=glGetUniformLocation(alphaProgram,"uFov");
    alphaBounds=glGetUniformLocation(alphaProgram,"uBounds");alphaPlane=glGetUniformLocation(alphaProgram,"uPlaneZ");
    glGenVertexArrays(1,&frameVAO);glGenBuffers(1,&frameVBO);
    glBindVertexArray(frameVAO);glBindBuffer(GL_ARRAY_BUFFER,frameVBO);
    UploadFrameGeometry();
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FrameVertex), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FrameVertex), reinterpret_cast<void*>(2*sizeof(float)));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(FrameVertex), reinterpret_cast<void*>(5*sizeof(float)));
    for (int i=0; i<3; ++i) glEnableVertexAttribArray(i);
    glBindVertexArray(0);
    return true;
}

void SetPortalCaption(const char* scene,unsigned take){
    const std::string label=scene?scene:"MAIN MENU";
    if(slateScene==label&&slateTake==take)return;
    slateScene=label.substr(0,36);slateTake=std::max(1u,take);slateDirty=true;
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","Slate: SCENE %s / TAKE %02u",slateScene.c_str(),slateTake);
}

void ShutdownPortalFrame() {
    if (frameVBO) glDeleteBuffers(1, &frameVBO);
    if (frameVAO) glDeleteVertexArrays(1, &frameVAO);
    if (frameProgram) glDeleteProgram(frameProgram);
    if(alphaProgram)glDeleteProgram(alphaProgram);
    alphaProgram=0;
    frameVBO=frameVAO=frameProgram=0;slateDirty=true;
}

void DrawPortalFrame(const std::array<Mat4,2>& eyeVP,
                     const std::array<unsigned,2>& eyeFramebuffers, int width, int height,bool editing) {
    if (!frameProgram) return;
    if(slateDirty)UploadFrameGeometry();
    GLint previousFBO=0, program=0, vao=0, viewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_VIEWPORT, viewport);
    const bool depth=glIsEnabled(GL_DEPTH_TEST), blend=glIsEnabled(GL_BLEND);
    const bool scissor=glIsEnabled(GL_SCISSOR_TEST), cull=glIsEnabled(GL_CULL_FACE);
    GLboolean colourMask[4]; glGetBooleanv(GL_COLOR_WRITEMASK, colourMask);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glUseProgram(frameProgram); glBindVertexArray(frameVAO); glViewport(0,0,width,height);
    glUniform1i(frameEditing,editing?1:0);
    for(int eye=0; eye<2; ++eye) {
        glBindFramebuffer(GL_FRAMEBUFFER, eyeFramebuffers[eye]);
        glUniformMatrix4fv(frameMVP,1,GL_FALSE,eyeVP[eye].Data());
        glDrawArrays(GL_TRIANGLES,0,vertexCount);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,previousFBO); glUseProgram(program); glBindVertexArray(vao);
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    if(depth) glEnable(GL_DEPTH_TEST);
    if(blend) glEnable(GL_BLEND);
    if(scissor) glEnable(GL_SCISSOR_TEST);
    if(cull) glEnable(GL_CULL_FACE);
    glColorMask(colourMask[0],colourMask[1],colourMask[2],colourMask[3]);
}
}

namespace questxr {
// Alpha-only final pass: the room is visible around the slate, while the entire
// game aperture (including unfilled background pixels) remains opaque.
void DrawPortalAlpha(const std::array<Mat4,2>& portalFromEye,
                     const std::array<std::array<float,4>,2>& tangents,
                     const std::array<unsigned,2>& eyeFramebuffers,int width,int height){
    if(!alphaProgram)return;
    GLint fbo=0,program=0,vao=0,viewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_VIEWPORT,viewport);
    const bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),scissor=glIsEnabled(GL_SCISSOR_TEST),cull=glIsEnabled(GL_CULL_FACE);
    GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
    glUseProgram(alphaProgram);glBindVertexArray(frameVAO);glViewport(0,0,width,height);
    glUniform4f(alphaBounds,-kSlateHalfWidth,kSlateBottom,kSlateHalfWidth,kSlateTop);
    glUniform1f(alphaPlane,-kPortalDepth+0.002f);
    for(int eye=0;eye<2;++eye){
        glBindFramebuffer(GL_FRAMEBUFFER,eyeFramebuffers[eye]);
        glUniformMatrix4fv(alphaPose,1,GL_FALSE,portalFromEye[eye].Data());
        glUniform4fv(alphaFov,1,tangents[eye].data());glDrawArrays(GL_TRIANGLES,0,3);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);glUseProgram(program);glBindVertexArray(vao);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glColorMask(mask[0],mask[1],mask[2],mask[3]);
    if(depth)glEnable(GL_DEPTH_TEST);if(blend)glEnable(GL_BLEND);if(scissor)glEnable(GL_SCISSOR_TEST);if(cull)glEnable(GL_CULL_FACE);
}
}
