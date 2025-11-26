// system_server.c
#include "server_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

CharaInfo* SevergCharaHead = NULL;
//初期化
int InitServerChara(const char* position_data_file, CharaInfo players[], int max_players)
{
    int ret = 0;
    SevergCharaHead = NULL;

    FILE* fp = fopen(position_data_file, "r");
    if (!fp) return PrintError("failed to open position data file.");

    char linebuf[MAX_LINEBUF];

    while (fgets(linebuf, MAX_LINEBUF, fp)) {
        if (linebuf[0] == '#') continue;

        unsigned int type;
        float px, py;
        int rx, ry, rw, rh;

        if (7 != sscanf(linebuf, "%u%f%f%d%d%d%d", &type, &px, &py, &rx, &ry, &rw, &rh)) {
            PrintError("failed to parse line in position data");
            continue;
        }

        if (type >= CT_PLAYER0 && type < CT_PLAYER0 + max_players) {
            // プレイヤーの初期化
            int i = type - CT_PLAYER0;
            players[i].type = type;
            players[i].point.x = px;
            players[i].point.y = py;
            players[i].rect.x = (int)px;
            players[i].rect.y = (int)py;
            players[i].rect.w = rw;
            players[i].rect.h = rh;
            players[i].stts = CS_Normal;
            memset(&players[i].input, 0, sizeof(players[i].input));

            // linked list に追加
            players[i].next = SevergCharaHead;
            SevergCharaHead = &players[i];

        } else {
            // その他のNPCなど
            CharaInfo* ch = malloc(sizeof(CharaInfo));
            if (!ch) { ret = PrintError("failed to allocate NPC"); break; }

            ch->type = type;
            ch->point.x = px;
            ch->point.y = py;
            ch->rect.x = (int)px;
            ch->rect.y = (int)py;
            ch->rect.w = rw;
            ch->rect.h = rh;
            ch->stts = CS_Normal;
            ch->vel.x = ch->vel.y = 0;
            ch->ani.x = 0;
            ch->entity = NULL; // サーバーでは画像不要

            // linked list に追加
            ch->next = SevergCharaHead;
            SevergCharaHead = ch;
        }
    }

    fclose(fp);
    return ret;
}
//プレイヤー同士の当たり判定処理
SDL_bool CollisionPlayer(CharaInfo* a, CharaInfo* b)
{
    if (a->stts == CS_Disable || b->stts == CS_Disable) return SDL_FALSE;

    SDL_Rect ra = a->rect;
    SDL_Rect rb = b->rect;
    SDL_Rect ir;
    if (!SDL_IntersectRect(&ra, &rb, &ir)) return SDL_FALSE;

    int overlapW = ir.w;
    int overlapH = ir.h;

    float ax = a->point.x + a->rect.w / 2.0f;
    float ay = a->point.y + a->rect.h / 2.0f;
    float bx = b->point.x + b->rect.w / 2.0f;
    float by = b->point.y + b->rect.h / 2.0f;

    float dx = ax - bx;
    float dy = ay - by;

    if (overlapW < overlapH) {
        float push = overlapW / 2.0f;
        a->point.x += (dx > 0 ? push : -push);
        b->point.x -= (dx > 0 ? push : -push);
    } else {
        float push = overlapH / 2.0f;
        a->point.y += (dy > 0 ? push : -push);
        b->point.y -= (dy > 0 ? push : -push);
    }

    // 移動制限
    if (a->point.x < 0) a->point.x = 0;
    if (a->point.y < 0) a->point.y = 0;
    if (a->point.x + a->rect.w > MAP_Width) a->point.x = MAP_Width - a->rect.w;
    if (a->point.y + a->rect.h > MAP_Height) a->point.y = MAP_Height - a->rect.h;

    if (b->point.x < 0) b->point.x = 0;
    if (b->point.y < 0) b->point.y = 0;
    if (b->point.x + b->rect.w > MAP_Width) b->point.x = MAP_Width - b->rect.w;
    if (b->point.y + b->rect.h > MAP_Height) b->point.y = MAP_Height - b->rect.h;

    a->rect.x = (int)a->point.x;
    a->rect.y = (int)a->point.y;
    b->rect.x = (int)b->point.x;
    b->rect.y = (int)b->point.y;

    return SDL_TRUE;
}

//当たり判定
SDL_bool Collision(CharaInfo* ci, CharaInfo* cj)
{
    if (ci->stts == CS_Disable || cj->stts == CS_Disable) 
        return SDL_FALSE;

    SDL_Rect ir;
    if (!SDL_IntersectRect(&(ci->rect), &(cj->rect), &ir)) 
        return SDL_FALSE;

    if ((ci->type >= CT_PLAYER0 && ci->type <= CT_PLAYER3) &&
        (cj->type >= CT_PLAYER0 && cj->type <= CT_PLAYER3))
        return CollisionPlayer(ci, cj);

    printf("[Collision] ci:type=%d cj:type=%d ci=(%.1f,%.1f) cj=(%.1f,%.1f)\n",
           ci->type, cj->type, ci->point.x, ci->point.y, cj->point.x, cj->point.y); //デバッグ
    return SDL_FALSE;
}

int PrintError(const char* msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    return -1;
}
