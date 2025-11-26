//game.h

#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>

#define MAX_PATH 256
#define MAX_LINEBUF 256
#define MOVE_SPEED 15.f
#define WD_Width 1980
#define WD_Height 1080
#define MAP_Width 10312
#define MAP_Height 1391

/* ??­ã?£ã???????¿ã?¼ã?¿ã?¤ã?? */
typedef enum
{
    CT_PLAYER0 = 0,
    CT_PLAYER1 = 1,
    CT_PLAYER2 = 2,
    CT_PLAYER3 = 3
} CharaType;
#define CHARATYPE_NUM 4

/* ??­ã?£ã???????¿ã?¼ã?¿ã?¤ã????¥æ????? */
typedef struct
{
    int w, h;
    char *path;
    SDL_Point aninum;
    SDL_Texture *img;
} CharaTypeInfo;

/* ??­ã?£ã???????¿ã?¼ã????¶æ?? */
typedef enum
{
    CS_Normal = 0,
    CS_Disable = 1
} CharaStts;

/*??­ã?£ã???????¿ã?¢ã????¡ã?¼ã?·ã?§ã?³ç?¶æ??*/
typedef enum
{
    ANI_Stop = 0,
    ANI_RunRight = 1,
    ANI_RunLeft = 2,
    ANI_RunDown = 3,
    ANI_RunUp = 4
} CharaMoveStts;

/* ??­ã?¼å?¥å???????¶æ?? */
typedef struct
{
    SDL_bool right;
    SDL_bool left;  
    SDL_bool down;
    SDL_bool up;
    SDL_bool space;
} Keystts;

/* ??­ã?£ã???????¿ã?¼ã???????? */
typedef struct CharaInfo_t
{
    CharaStts stts;
    CharaTypeInfo *entity;
    SDL_Rect rect;
    SDL_Rect imgsrc;    
    SDL_FPoint vel;
    SDL_FPoint point;
    SDL_Point ani;
    CharaType type;
    struct CharaInfo_t *next;
    int exp;
    int level;
    Keystts input; // ???ê¸? ??????
    float animAcc;//¥¹¥×¥é¥¤¥ÈÍÑ¥¿¥¤¥Þ¡¼
} CharaInfo;


/* ??²ã?¼ã???????¶æ?? */
typedef enum
{
    GS_Ready = 0,
    GS_Playing = 1,
} GameStts;

typedef struct {
    int x, y;   // ì¹´ë????? ì¢??????? ì¢????
    int w, h;   // ???ë©? ???ê¸? (????????? ???ê¸?)
} Camera;

/* ??²ã?¼ã??????????? */
typedef struct
{
    CharaInfo *player;
    GameStts stts;
    SDL_Window *window;
    SDL_Renderer *render;
    SDL_Texture *bg;
    SDL_Point dp;
    Keystts input;
    Camera cam;
    float timeDelta;
} GameInfo;




/* ???? ?????? ë³???? ??????*/
extern GameInfo gGames[CHARATYPE_NUM];
extern CharaTypeInfo gCharaType[CHARATYPE_NUM];



/* ???? ?????? ?????? */
int InitSystem(const char *chara_file, const char *pos_file, int my_id);
void DestroySystem(void);
void InitCharaInfo(CharaInfo *ch);
SDL_bool InputEvent(GameInfo *game);
void MoveChara(CharaInfo *ch, GameInfo *game);
SDL_bool InputEvent(GameInfo *game);


int InitWindow(GameInfo *game, const char *title, const char *bg_file, int width, int height);
void CloseWindow(GameInfo *game);
void UpdateAnimation(GameInfo *game, float deltaTime);
void DrawGame(GameInfo *game);
int PrintError(const char *msg);
CharaInfo* GetPlayerChara(int my_id);
void UpdateCamera(GameInfo *gGame, CharaInfo *player);

#endif