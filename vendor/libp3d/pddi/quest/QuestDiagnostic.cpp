#include "pddi/quest/QuestXR.h"
#include "pddi/quest/QuestMath.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <array>

namespace questxr {
namespace {
GLuint Compile(GLenum stage,const char* source){
    const GLuint shader=glCreateShader(stage);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048];glGetShaderInfoLog(shader,sizeof(log),nullptr,log);__android_log_print(ANDROID_LOG_ERROR,"ReChanXR","Diagnostic shader: %s",log);glDeleteShader(shader);return 0;}
    return shader;
}
}
int RunDiagnostic(){
    SetPortalEnabled(false);
    const char* vertex=R"(#version 300 es
#extension GL_OVR_multiview2 : require
precision highp float;
layout(num_views=2) in;
layout(location=0) in vec3 position;
layout(location=1) in vec3 colour;
uniform mat4 uMVP[2];
out vec3 vColour;
void main(){vColour=colour;gl_Position=uMVP[gl_ViewID_OVR]*vec4(position,1.0);}
)";
    const char* fragment=R"(#version 300 es
precision highp float;
in vec3 vColour;
out vec4 fragment;
void main(){fragment=vec4(vColour,1.0);}
)";
    GLuint vs=Compile(GL_VERTEX_SHADER,vertex),fs=Compile(GL_FRAGMENT_SHADER,fragment);
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 1;}
    const GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
    glDeleteShader(vs);glDeleteShader(fs);GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(program);return 1;}
    // A 30-cm cube, with a different colour at each corner for orientation checks.
    const float vertices[]={
        -.15f,-.15f,-.15f,1,0,0,  .15f,-.15f,-.15f,0,1,0,
        .15f,.15f,-.15f,0,0,1,  -.15f,.15f,-.15f,1,1,0,
        -.15f,-.15f,.15f,1,0,1,  .15f,-.15f,.15f,0,1,1,
        .15f,.15f,.15f,1,1,1,  -.15f,.15f,.15f,1,.4f,0};
    const unsigned short indices[]={0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,3,2,6,3,6,7,0,3,7,0,7,4,1,5,6,1,6,2};
    GLuint vao=0,vbo=0,ebo=0;glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ebo);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),reinterpret_cast<void*>(3*sizeof(float)));glEnableVertexAttribArray(1);
    const GLint mvp=glGetUniformLocation(program,"uMVP[0]");
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","Diagnostic: three 30cm cubes at depths 1.5m, 2.5m, 3.5m; left stick click recenters");
    const std::array<Mat4,3> positions={Translation(-.5f,0,-1.5f),Translation(0,0,-2.5f),Translation(.5f,0,-3.5f)};
    while(!ShouldClose()){
        PollEvents();if(ShouldClose())break;
        BeginFrame();if(ShouldClose())break;
        glBindFramebuffer(GL_FRAMEBUFFER,Framebuffer());glViewport(0,0,Width(),Height());glDisable(GL_SCISSOR_TEST);
        glClearColor(.025f,.035f,.06f,1);glClearDepthf(0);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_GEQUAL);
        glUseProgram(program);glBindVertexArray(vao);
        for(const auto& model:positions){
            // UploadMeshMatrices converts game units to metres. These diagnostic cubes already use metres.
            UploadMeshMatrices(mvp,Scale(1000),model);glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_SHORT,nullptr);
        }
        EndFrame();
    }
    glDeleteBuffers(1,&ebo);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);
    return Error()[0]?1:0;
}
}
