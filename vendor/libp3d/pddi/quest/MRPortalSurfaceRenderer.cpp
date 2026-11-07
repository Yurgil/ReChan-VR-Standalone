#include "pddi/quest/MRPortalSurfaceRenderer.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <vector>
namespace questxr {
namespace {
struct Vertex{float x,y,dx,dy,age,strength;};
GLuint program=0,vao=0,vbo=0;GLint mvp=-1;
GLuint Shader(GLenum type,const char* source){
    GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char message[1024];glGetShaderInfoLog(shader,sizeof(message),nullptr,message);__android_log_print(ANDROID_LOG_ERROR,"ReChanXR","MR surface shader: %s",message);glDeleteShader(shader);return 0;}return shader;
}
}
bool InitializeMRPortalSurface(){
    const char* vs=R"(#version 300 es
precision highp float;
layout(location=0) in vec2 p;layout(location=1) in vec2 d;layout(location=2) in vec2 pulse;
uniform mat4 uMVP;out vec2 local;out vec2 delta;flat out vec2 state;
void main(){local=p;delta=d;state=pulse;gl_Position=uMVP*vec4(p,-2.3985,1);})";
    const char* fs=R"(#version 300 es
precision highp float;
in vec2 local;in vec2 delta;flat in vec2 state;out vec4 result;
void main(){
 if(abs(local.x)>1.6||abs(local.y)>1.2)discard;
 float age=state.x,r=length(delta),radius=.018+.22*age;
 float outer=exp(-pow((r-radius)/.005,2.));
 float inner=.45*exp(-pow((r-radius*.66)/.004,2.));
 float edge=smoothstep(0.,.035,min(1.6-abs(local.x),1.2-abs(local.y)));
 float alpha=clamp((outer+inner)*exp(-age*2.8)*state.y*.55*edge,0.,.6);
 if(alpha<.003)discard;result=vec4(.48,.87,1.,alpha);
})";
    GLuint v=Shader(GL_VERTEX_SHADER,vs),f=Shader(GL_FRAGMENT_SHADER,fs);if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){ShutdownMRPortalSurface();return false;}
    mvp=glGetUniformLocation(program,"uMVP");glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    for(int i=0;i<3;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(size_t(i*2*sizeof(float))));}
    glBindVertexArray(0);return true;
}
void ShutdownMRPortalSurface(){if(program)glDeleteProgram(program);if(vao)glDeleteVertexArrays(1,&vao);if(vbo)glDeleteBuffers(1,&vbo);program=vao=vbo=0;}
void DrawMRPortalSurface(const std::array<Mat4,2>& vp,const std::array<unsigned,2>& fbos,int width,int height,const PortalPlacement& window,const MRPortalSurface& surface){
    if(!program)return;std::vector<Vertex> vertices;
    for(const auto& ring:surface.Ripples())if(ring.strength>0){
        const float radius=.018f+.22f*ring.age+.016f;
        const float xy[][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
        for(int i:{0,1,2,0,2,3}){const float dx=xy[i][0]*radius,dy=xy[i][1]*radius;vertices.push_back({ring.x+dx/window.scale,ring.y+dy/window.scale,dx,dy,ring.age,ring.strength});}
    }
    if(vertices.empty())return;
    GLint fbo,oldProgram,oldVao,oldBuffer,viewport[4],depthFunc,srcRGB,dstRGB,srcAlpha,dstAlpha,equationRGB,equationAlpha;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB);glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRGB);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
    const bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST);GLboolean colourMask[4],depthMask;glGetBooleanv(GL_COLOR_WRITEMASK,colourMask);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_GEQUAL);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);glViewport(0,0,width,height);
    for(int eye=0;eye<2;++eye){glBindFramebuffer(GL_FRAMEBUFFER,fbos[eye]);glUniformMatrix4fv(mvp,1,GL_FALSE,vp[eye].Data());glDrawArrays(GL_TRIANGLES,0,vertices.size());}
    glBindFramebuffer(GL_FRAMEBUFFER,fbo);glUseProgram(oldProgram);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glColorMask(colourMask[0],colourMask[1],colourMask[2],colourMask[3]);glDepthMask(depthMask);glDepthFunc(depthFunc);glBlendEquationSeparate(equationRGB,equationAlpha);glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha);if(!depth)glDisable(GL_DEPTH_TEST);if(!blend)glDisable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);if(scissor)glEnable(GL_SCISSOR_TEST);
}
}
