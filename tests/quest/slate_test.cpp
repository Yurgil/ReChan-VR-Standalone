#include "pddi/quest/QuestSlateState.h"
#include "pddi/quest/QuestSlateText.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace questxr;
int main(){
    QuestSlateState slate;assert(slate.Scene()=="MAIN MENU"&&slate.Take()==1);
    slate.Retry();assert(slate.Take()==1);
    slate.EnterLevel(0,0," Chinatown","1");assert(slate.Scene()=="CHINATOWN 1"&&slate.Take()==1);
    // Death confirmation counts once, even before the reload finishes.
    slate.Retry();slate.Retry();assert(slate.Take()==2);
    slate.EnterLevel(0,0,"Chinatown","1");assert(slate.Take()==2);
    slate.Retry();assert(slate.Take()==3);
    slate.EnterLevel(0,0,"Chinatown","1");assert(slate.Take()==3);
    // A checkpoint reload is the same stage; another stage starts at TAKE01.
    slate.EnterLevel(0,1,"Chinatown","2");assert(slate.Take()==1&&slate.Scene()=="CHINATOWN 2");
    for(int i=0;i<110;++i){slate.Retry();slate.EnterLevel(0,1,"Chinatown","2");}
    assert(slate.Take()==111);
    slate.MainMenu();assert(slate.Take()==1&&slate.Scene()=="MAIN MENU");
    slate.EnterLevel(0,1,"Chinatown","2");assert(slate.Take()==1);
    slate.Retry();slate.MainMenu(); // Game over or exit to hub resets, including same-stage retry.
    slate.EnterLevel(0,1,"Chinatown","2");assert(slate.Take()==1);
    slate.EnterLevel(9,0," \tWaterfront\r\n","boss");assert(slate.Scene()=="WATERFRONT BOSS");
    slate.EnterLevel(10,0,nullptr,nullptr);assert(slate.Scene()=="LEVEL 11 1");
    for(char c:std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")){
        const char* glyph=SlateGlyph(c);assert(std::strlen(glyph)==35);assert(std::strchr(glyph,'1'));
        for(int i=0;i<35;++i)assert(glyph[i]=='0'||glyph[i]=='1');
    }
    assert(!std::strchr(SlateGlyph(' '),'1'));
    std::puts("Slate: stage names, single death count, checkpoint reload, new stage, menu/game-over reset, 100+ takes and complete glyphs OK");
}
