#include "pddi/quest/MRPointerRenderer.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <vector>
namespace questxr {
namespace {
GLuint program=0,vao=0,vbo=0;GLint mvp=-1;
struct Vertex{float x,y,z,r,g,b;};
GLuint Compile(GLenum type,const char* source){GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(s,sizeof(log),nullptr,log);__android_log_print(ANDROID_LOG_ERROR,"ReChanXR","MR pointer shader: %s",log);glDeleteShader(s);return 0;}return s;}
}
bool InitializeMRPointers(){
    const char* vs="#version 300 es\nprecision highp float;layout(location=0) in vec3 p;layout(location=1) in vec3 c;uniform mat4 uMVP;out vec3 colour;void main(){colour=c;gl_Position=uMVP*vec4(p,1);}";
    const char* fs="#version 300 es\nprecision highp float;in vec3 colour;out vec4 result;void main(){result=vec4(colour,1);}";
    GLuint v=Compile(GL_VERTEX_SHADER,vs),f=Compile(GL_FRAGMENT_SHADER,fs);if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){ShutdownMRPointers();return false;}
    mvp=glGetUniformLocation(program,"uMVP");glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,4096,nullptr,GL_STREAM_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(3*sizeof(float)));glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glBindVertexArray(0);return true;
}
void ShutdownMRPointers(){if(program)glDeleteProgram(program);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);program=vao=vbo=0;}
void DrawMRPointers(const std::array<Mat4,2>& vp,const std::array<unsigned,2>& fbos,int width,int height,const std::array<MRPointerVisual,2>& pointers){
    std::vector<Vertex> vertices;int lines=0;
    auto vertex=[&](const std::array<float,3>& p,const MRPointerVisual& v){vertices.push_back({p[0],p[1],p[2],v.active?.15f:(v.resize?1.f:.1f),v.active?1.f:.8f,v.active?.35f:1.f});};
    for(const auto& p:pointers)if(p.visible&&!p.direct){vertex(p.start,p);vertex(p.end,p);lines+=2;}
    const std::array<float,3> directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const int triangles[]={4,0,2,4,2,1,4,1,3,4,3,0,5,2,0,5,1,2,5,3,1,5,0,3};
    for(const auto& p:pointers)if(p.visible)for(int index:triangles){auto point=p.end;for(int i=0;i<3;++i)point[i]+=.009f*directions[index][i];vertex(point,p);}
    if(vertices.empty()||!program)return;
    GLint oldFbo,oldProgram,oldVao,oldBuffer,viewport[4];glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);glGetIntegerv(GL_VIEWPORT,viewport);
    bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST);GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);glViewport(0,0,width,height);
    for(int eye=0;eye<2;++eye){glBindFramebuffer(GL_FRAMEBUFFER,fbos[eye]);glUniformMatrix4fv(mvp,1,GL_FALSE,vp[eye].Data());if(lines)glDrawArrays(GL_LINES,0,lines);glDrawArrays(GL_TRIANGLES,lines,vertices.size()-lines);}
    glBindFramebuffer(GL_FRAMEBUFFER,oldFbo);glUseProgram(oldProgram);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glColorMask(mask[0],mask[1],mask[2],mask[3]);if(depth)glEnable(GL_DEPTH_TEST);if(blend)glEnable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);if(scissor)glEnable(GL_SCISSOR_TEST);
}
}
