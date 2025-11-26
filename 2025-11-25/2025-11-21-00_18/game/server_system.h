#ifndef SERVER_SYSTEM_H
#define SERVER_SYSTEM_H

#include "game.h"

extern CharaInfo* SevergCharaHead;

// 位置データファイルから NPC とプレイヤーを初期化
int InitServerChara(const char* position_data_file, CharaInfo players[], int max_players);

// 衝突判定
SDL_bool CollisionPlayer(CharaInfo* a, CharaInfo* b);
SDL_bool Collision(CharaInfo* ci, CharaInfo* cj);
int PrintError(const char *msg);

#endif
