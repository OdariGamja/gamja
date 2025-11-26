// window.c
#include "game.h"
#include <SDL2/SDL_image.h>
#include <stdlib.h>

extern CharaInfo *gCharaHead;

//========================
// ウインドウ初期化
//========================
int InitWindow(GameInfo *gGame, const char *title, const char *bg_file, int width, int height)
{
    printf("[INIT] InitWindow() called\n");

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
        return PrintError("failed to initialize SDL_image");

    gGame->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     width, height, 0);

    printf("[INIT] window=%p\n", gGame->window);

    gGame->render = SDL_CreateRenderer(gGame->window, -1, SDL_RENDERER_ACCELERATED);
    printf("[INIT] renderer=%p\n", gGame->render);

    // 배경 로드
    SDL_Surface *surf = IMG_Load(bg_file);
    printf("[INIT] background surface=%p (%s)\n", surf, bg_file);

    gGame->bg = SDL_CreateTextureFromSurface(gGame->render, surf);
    SDL_FreeSurface(surf);
    printf("[INIT] background texture=%p\n", gGame->bg);

    // 캐릭터 로드
    for (int i = 0; i < CHARATYPE_NUM; i++)
    {
        printf("\n[INIT] LOADING CHARACTER TYPE %d\n", i);

        SDL_Surface *s = IMG_Load(gCharaType[i].path);
        if (!s) return PrintError("failed to load character image");

        printf("[INIT] character surface=%p path=%s size=%d x %d\n",
               s, gCharaType[i].path, s->w, s->h);

        gCharaType[i].img = SDL_CreateTextureFromSurface(gGame->render, s);

        printf("[INIT] gCharaType[%d].img=%p\n", i, gCharaType[i].img);

        int fullW = s->w;
        int fullH = s->h;
        int frameW = gCharaType[i].w;
        int frameH = gCharaType[i].h;

        gCharaType[i].aninum.x = fullW / frameW;
        gCharaType[i].aninum.y = fullH / frameH;

        printf("[INIT] Frames: %d x %d (frame size %d x %d)\n",
               gCharaType[i].aninum.x, gCharaType[i].aninum.y,
               frameW, frameH);

        SDL_FreeSurface(s);
    }

    gGame->cam.x = 0;
    gGame->cam.y = 0;
    gGame->cam.w = WD_Width;
    gGame->cam.h = WD_Height;

    printf("[INIT] Camera initialized (%d,%d,%d,%d)\n",
           gGame->cam.x, gGame->cam.y, gGame->cam.w, gGame->cam.h);

    return 0;
}




//========================
// ゲーム描画
//========================
void DrawGame(GameInfo *gGame)
{
    printf("\n[DRAW] DrawGame() frame start\n");
    printf("[DRAW] Camera = (%d,%d)\n", gGame->cam.x, gGame->cam.y);

    SDL_RenderClear(gGame->render);

    SDL_Rect camRect = { gGame->cam.x, gGame->cam.y, gGame->cam.w, gGame->cam.h };
    SDL_Rect dstBg = { 0, 0, gGame->cam.w, gGame->cam.h };

    SDL_RenderCopy(gGame->render, gGame->bg, &camRect, &dstBg);

    // 캐릭터 그리기
    CharaInfo *ch = gCharaHead;
    while (ch)
    {
        printf("[DRAW] CHAR id=%d pos=(%.1f,%.1f) ani=(%d,%d)\n",
               ch->type, ch->point.x, ch->point.y, ch->ani.x, ch->ani.y);

        SDL_Rect dst = {
            (int)(ch->point.x - gGame->cam.x),
            (int)(ch->point.y - gGame->cam.y),
            ch->rect.w,
            ch->rect.h
        };

        printf("[DRAW] -> screen dst=(%d,%d,%d,%d)\n",
               dst.x, dst.y, dst.w, dst.h);
        printf("[DRAW] -> src frame=(%d,%d,%d,%d)\n",
               ch->imgsrc.x, ch->imgsrc.y, ch->imgsrc.w, ch->imgsrc.h);

        SDL_RenderCopy(gGame->render, ch->entity->img, &ch->imgsrc, &dst);

        ch = ch->next;
    }

    SDL_RenderPresent(gGame->render);
}





//========================
// アニメーション更新
//========================
/*
void UpdateAnimation(GameInfo *gGame, float dt)
{
    printf("\n[ANI] UpdateAnimation dt=%.3f\n", dt);

   for (CharaInfo *ch = gCharaHead; ch; ch = ch->next)
    {
        printf("[ANI] BEFORE char=%d ani=(%d,%d)\n",
               ch->type, ch->ani.x, ch->ani.y);

        int maxX = ch->entity->aninum.x;

        ch->ani.x += (int)(dt * 10);
        if (ch->ani.x >= maxX)
            ch->ani.x = 0;

        int frameW = ch->entity->w;
        int frameH = ch->entity->h;

        ch->imgsrc.x = ch->ani.x * frameW;
        ch->imgsrc.y = ch->ani.y * frameH;
        ch->imgsrc.w = frameW;
        ch->imgsrc.h = frameH;

        printf("[ANI] AFTER ani=(%d,%d) src=(%d,%d)\n",
               ch->ani.x, ch->ani.y, ch->imgsrc.x, ch->imgsrc.y);

   }
}*/

//========================
// アニメーション更新
//dt:前フレームからの経過時間

//========================
void UpdateAnimation(GameInfo *gGame, float dt)
{
    const float frameTime = 0.1f; // 0.1秒で次のフレームに切り替える

    for (CharaInfo *ch = gCharaHead; ch; ch = ch->next)
    {
        if(ch->input.right == 1){
            ch->ani.y = 1;
        }else if(ch->input.left == 1){
            ch->ani.y = 2;
        }else if(ch->input.down == 1){
            ch->ani.y = 3;
        }else if(ch->input.up == 1){
            ch->ani.y = 4;
        }else{
            ch->ani.y = 0;
        }
printf("ch->key:%d, %d, %d, %d\n",ch->input.right,ch->input.left,ch->input.down,ch->input.up);


        ch->animAcc += dt;

        if (ch->animAcc >= frameTime) { //0.1秒を超えると余った時間を残す
            ch->animAcc -= frameTime;

            ch->ani.x++;
            if (ch->ani.x >= ch->entity->aninum.x)
                ch->ani.x = 0;
        }

        ch->imgsrc.x = ch->ani.x * ch->entity->w;
        ch->imgsrc.y = ch->ani.y * ch->entity->h;
        ch->imgsrc.w = ch->entity->w;
        ch->imgsrc.h = ch->entity->h;

         printf("[ANI] AFTER ani=(%d,%d) src=(%d,%d)\n",
               ch->ani.x, ch->ani.y, ch->imgsrc.x, ch->imgsrc.y);

    }
}


//========================
// カメラアップデート
//========================
void UpdateCamera(GameInfo* gGame, CharaInfo* player)
{
    float targetX = player->point.x + player->rect.w / 2 - gGame->cam.w / 2;
    float targetY = player->point.y + player->rect.h / 2 - gGame->cam.h / 2;

    // マップ境界のクランプ
    if (targetX < 0) targetX = 0;
    if (targetY < 0) targetY = 0;
    if (targetX > MAP_Width - gGame->cam.w) targetX = MAP_Width - gGame->cam.w;
    if (targetY > MAP_Height - gGame->cam.h) targetY = MAP_Height - gGame->cam.h;

    gGame->cam.x = (int)targetX;
    gGame->cam.y = (int)targetY;

    printf("[CAM] FINAL cam=(%d,%d)\n", gGame->cam.x, gGame->cam.y);
}



//========================
// 종료 처리
//========================
void CloseWindow(GameInfo *gGame)
{
    printf("[CLOSE] CloseWindow()\n");

    if (!gGame)
        return;

    if (gGame->bg)
        printf("[CLOSE] Destroying background\n"), SDL_DestroyTexture(gGame->bg);

    for (int i = 0; i < CHARATYPE_NUM; i++)
    {
        if (gCharaType[i].img) {
            printf("[CLOSE] Destroy char texture %d\n", i);
            SDL_DestroyTexture(gCharaType[i].img);
        }
    }

    if (gGame->render)
        printf("[CLOSE] Destroy renderer\n"), SDL_DestroyRenderer(gGame->render);
    if (gGame->window)
        printf("[CLOSE] Destroy window\n"), SDL_DestroyWindow(gGame->window);

    IMG_Quit();
}
