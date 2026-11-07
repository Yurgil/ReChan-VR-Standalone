#include "pddi/quest/MRPortalRenderer.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <vector>
namespace questxr {
namespace {
using V=std::array<float,3>;
struct Vertex{V p,n,c;float kind;};
GLuint program=0,vao=0,vbo=0;GLint mvp=-1,inverseWindow=-1,eyeLocation=-1;
V Sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V Cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
V Unit(V p){float l=std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);if(l<.00001f)return {0,1,0};for(float& v:p)v/=l;return p;}
void Triangle(std::vector<Vertex>& out,V a,V b,V c,V colour,float kind){const auto n=Unit(Cross(Sub(b,a),Sub(c,a)));for(auto p:{a,b,c})out.push_back({p,n,colour,kind});}
V Point(const Mat4& m,V p){return {m(0,0)*p[0]+m(0,1)*p[1]+m(0,2)*p[2]+m(0,3),m(1,0)*p[0]+m(1,1)*p[1]+m(1,2)*p[2]+m(1,3),m(2,0)*p[0]+m(2,1)*p[1]+m(2,2)*p[2]+m(2,3)};}
void Joint(std::vector<Vertex>& out,V centre,float radius,V colour){
    const V axes[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const int ids[]={4,0,2,4,2,1,4,1,3,4,3,0,5,2,0,5,1,2,5,3,1,5,0,3};
    for(int t=0;t<24;t+=3){V points[3];for(int i=0;i<3;++i)for(int d=0;d<3;++d)points[i][d]=centre[d]+radius*axes[ids[t+i]][d];Triangle(out,points[0],points[1],points[2],colour,0);}
}
void Bone(std::vector<Vertex>& out,V a,V b,float radius,V colour){
    const auto axis=Unit(Sub(b,a)),u=Unit(Cross(axis,std::fabs(axis[1])>.8f?V{1,0,0}:V{0,1,0})),v=Cross(axis,u);
    V pa[6],pb[6];for(int i=0;i<6;++i){const float angle=i*6.2831853f/6;for(int d=0;d<3;++d){const float delta=radius*(std::cos(angle)*u[d]+std::sin(angle)*v[d]);pa[i][d]=a[d]+delta;pb[i][d]=b[d]+delta;}}
    for(int i=0;i<6;++i){int j=(i+1)%6;Triangle(out,pa[i],pb[i],pb[j],colour,0);Triangle(out,pa[i],pb[j],pa[j],colour,0);}
}
GLuint Shader(GLenum type,const char* source){GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(s,sizeof(log),nullptr,log);__android_log_print(ANDROID_LOG_ERROR,"ReChanXR","MR5 shader: %s",log);glDeleteShader(s);return 0;}return s;}
}
bool InitializeMRPortalObjects(){
    const char* vs=R"(#version 300 es
precision highp float;
layout(location=0) in vec3 p;layout(location=1) in vec3 n;layout(location=2) in vec3 c;layout(location=3) in float k;
uniform mat4 uMVP;out vec3 position;out vec3 colour;flat out float kind;
void main(){position=p;colour=c*(.62+.38*abs(dot(normalize(n),normalize(vec3(-.3,.8,.5)))));kind=k;gl_Position=uMVP*vec4(p,1);})";
    const char* fs=R"(#version 300 es
precision highp float;
uniform mat4 uInverseWindow;uniform vec3 uEye;
in vec3 position;in vec3 colour;flat in float kind;out vec4 result;
void main(){
 vec3 p=(uInverseWindow*vec4(position,1)).xyz;
 bool behind=p.z<=-2.4;
 if(kind>1.5){
   float dz=p.z-uEye.z;
   if(behind&&abs(dz)>.00001){float t=(-2.4-uEye.z)/dz;vec2 hit=uEye.xy+t*(p.xy-uEye.xy);if(t>=0.&&t<=1.&&abs(hit.x)<=1.82&&hit.y>=-1.51&&hit.y<=1.62)discard;}
 }else if(behind||kind<.5){
   float dz=p.z-uEye.z;if(abs(dz)<.00001)discard;
   float t=(-2.4-uEye.z)/dz;vec2 hit=uEye.xy+t*(p.xy-uEye.xy);
   if(t<0.)discard;
   if(behind){if(t>1.||abs(hit.x)>1.6||abs(hit.y)>1.2)discard;}
   else if(abs(hit.x)>1.82||hit.y< -1.51||hit.y>1.62)discard;
 }
 result=vec4(colour,1);
})";
    GLuint v=Shader(GL_VERTEX_SHADER,vs),f=Shader(GL_FRAGMENT_SHADER,fs);if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){ShutdownMRPortalObjects();return false;}
    mvp=glGetUniformLocation(program,"uMVP");inverseWindow=glGetUniformLocation(program,"uInverseWindow");eyeLocation=glGetUniformLocation(program,"uEye");
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    for(int i=0;i<4;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==3?1:3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(size_t(i*3*sizeof(float))));}
    glBindVertexArray(0);return true;
}
void ShutdownMRPortalObjects(){if(program)glDeleteProgram(program);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);program=vao=vbo=0;}
void DrawMRPortalObjects(const std::array<Mat4,2>& vp,const std::array<unsigned,2>& fbos,int width,int height,const PortalPlacement& window,const std::array<float,6>& eyes,const std::array<MRHandVisual,2>& hands,const MRTestObject& object,const MRRoomGeometry* room){
    if(!program)return;std::vector<Vertex> vertices;
    if(object.Visible()){
        const V corners[]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
        const int faces[][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
        const V colours[]={{.1f,.6f,1},{1,.4f,.1f},{.15f,.85f,.35f},{1,.7f,.1f},{.8f,.2f,.7f},{.15f,.8f,.9f}};
        for(int i=0;i<6;++i){V p[4];for(int j=0;j<4;++j)p[j]=Point(object.Transform(),corners[faces[i][j]]);Triangle(vertices,p[0],p[1],p[2],colours[i],1);Triangle(vertices,p[0],p[2],p[3],colours[i],1);}
    }
    const int bones[][2]={{1,0},{1,2},{2,3},{3,4},{4,5},{1,6},{6,7},{7,8},{8,9},{9,10},{1,11},{11,12},{12,13},{13,14},{14,15},{1,16},{16,17},{17,18},{18,19},{19,20},{1,21},{21,22},{22,23},{23,24},{24,25},{6,11},{11,16},{16,21}};
    for(const auto& hand:hands)if(hand.valid){
        const V colour={.88f,.79f,.65f};
        for(const auto& j:hand.joints)if(j.valid)Joint(vertices,j.position,j.radius,colour);
        for(const auto& bone:bones){const auto& a=hand.joints[bone[0]];const auto& b=hand.joints[bone[1]];if(a.valid&&b.valid)Bone(vertices,a.position,b.position,std::min(a.radius,b.radius)*.75f,colour);}
    }
    const int triangleVertices=vertices.size();
    if(room&&room->ready)for(const auto& surface:room->surfaces)if(surface.located){
        const V colour=surface.floor?V{.2f,.7f,1}:surface.wall?V{1,.75f,.2f}:V{.2f,1,.4f};
        auto line=[&](V a,V b){vertices.push_back({Point(surface.pose,a),{0,1,0},colour,2});vertices.push_back({Point(surface.pose,b),{0,1,0},colour,2});};
        const auto& p=surface.boundary;for(size_t i=0;i<p.size();++i){auto a=p[i],b=p[(i+1)%p.size()];line({a[0],a[1],0},{b[0],b[1],0});}
        if(surface.volume){V corners[8];for(int i=0;i<8;++i)for(int axis=0;axis<3;++axis)corners[i][axis]=surface.low[axis]+((i&(1<<axis))?surface.extent[axis]:0);
            for(int i=0;i<8;++i)for(int axis=0;axis<3;++axis)if(!(i&(1<<axis)))line(corners[i],corners[i|(1<<axis)]);}
    }
    if(vertices.empty())return;
    GLint oldFbo,oldProgram,oldVao,oldBuffer,viewport[4],depthFunc;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&oldBuffer);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
    const bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST);GLboolean mask[4],depthMask;glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_GEQUAL);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);glViewport(0,0,width,height);
    const auto inv=window.ReferenceToLocal();glUniformMatrix4fv(inverseWindow,1,GL_FALSE,inv.Data());
    for(int eye=0;eye<2;++eye){glBindFramebuffer(GL_FRAMEBUFFER,fbos[eye]);glUniformMatrix4fv(mvp,1,GL_FALSE,vp[eye].Data());glUniform3fv(eyeLocation,1,eyes.data()+3*eye);glDepthMask(GL_TRUE);glDrawArrays(GL_TRIANGLES,0,triangleVertices);glDepthMask(GL_FALSE);glDrawArrays(GL_LINES,triangleVertices,vertices.size()-triangleVertices);}
    glBindFramebuffer(GL_FRAMEBUFFER,oldFbo);glUseProgram(oldProgram);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glColorMask(mask[0],mask[1],mask[2],mask[3]);glDepthMask(depthMask);glDepthFunc(depthFunc);if(!depth)glDisable(GL_DEPTH_TEST);if(blend)glEnable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);if(scissor)glEnable(GL_SCISSOR_TEST);
}
}
