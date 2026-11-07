#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
namespace questxr {
// A take belongs to one playable stage (area + petal), not to pause/render frames.
class QuestSlateState {
public:
    void MainMenu(){levelKey=~uint64_t(0);scene="MAIN MENU";take=1;retryPending=false;}
    void EnterLevel(uint32_t level,uint32_t petal,const char* levelName,const char* partName){
        const uint64_t next=(uint64_t(level)<<32)|petal;
        if(next!=levelKey){take=1;levelKey=next;}
        retryPending=false;
        scene=Clean(levelName);if(scene.empty())scene="LEVEL "+std::to_string(level+1);
        const auto part=Clean(partName);if(!part.empty())scene+=" "+part;
        else scene+=" "+std::to_string(petal+1);
        if(scene.size()>36)scene.resize(36);
    }
    void Retry(){
        if(levelKey==~uint64_t(0)||retryPending)return;
        if(take<std::numeric_limits<unsigned>::max())++take;
        retryPending=true;
    }
    const std::string& Scene()const{return scene;}
    unsigned Take()const{return take;}
private:
    static std::string Clean(const char* text){
        std::string out;if(!text)return out;bool space=false;
        for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;++p){
            unsigned char c=*p;if(c>='a'&&c<='z')c-=32;
            if((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'){
                if(space&&!out.empty())out+=' ';out+=char(c);space=false;
            }else if(c==' '||c=='\t'||c=='\r'||c=='\n')space=true;
        }
        return out;
    }
    uint64_t levelKey=~uint64_t(0);
    std::string scene="MAIN MENU";
    unsigned take=1;
    bool retryPending=false;
};
}
