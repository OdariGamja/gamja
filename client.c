//client.c 電算室

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "game.h"

#define SERVER_PORT 5000
#define MAX_CLIENT 4

char server_ip[64];

int my_id;
int sock;
struct sockaddr_in server_addr;

extern GameInfo gGames[CHARATYPE_NUM];  // 모든 플레이어 상태
extern CharaInfo* gCharaHead;



Uint32 AnimationCallback(Uint32 interval, void* param) {
    GameInfo* game = (GameInfo*)param;
    UpdateAnimation(game, interval / 1000.0f);  
    return interval;  
}



// ReceiveThread 수정
void* ReceiveThread(void* arg) {
    while (1) {
        CharaInfo buf[MAX_CLIENT];
        int r = read(sock, &buf, sizeof(buf));
        if (r <= 0) break;

        CharaInfo* ch = gCharaHead;
        for (int i = 0; i < CHARATYPE_NUM && ch; i++, ch = ch->next) {
            float dx = buf[i].point.x - ch->point.x;
            float dy = buf[i].point.y - ch->point.y;

            if (i == my_id) {
                ch->point = buf[i].point;
            } else {
                // 보간률 0.25로 변경
                ch->point.x += dx * 0.25f;
                ch->point.y += dy * 0.25f;
            }

            ch->rect.x = (int)ch->point.x;
            ch->rect.y = (int)ch->point.y;

            // 이동 방향 저장 (애니메이션용)
            if (fabs(dx) > 0.5f || fabs(dy) > 0.5f) {
                if (fabs(dx) > fabs(dy)) ch->ani.y = (dx > 0) ? ANI_RunRight : ANI_RunLeft;
                else ch->ani.y = (dy > 0) ? ANI_RunDown : ANI_RunUp;
            } else {
                ch->ani.y = ANI_Stop;
            }
        }
    }
    return NULL;
}

// UpdateAnimation 수정
void UpdateAnimation(GameInfo* gGame, float deltaTime) {
    if (!gGame) return;

    for (CharaInfo* ch = gCharaHead; ch; ch = ch->next) {
        int maxFramesX = ch->entity->aninum.x;
        if (maxFramesX <= 0) maxFramesX = 1;

        if (ch->ani.y != ANI_Stop) {
            // 이동 중일 때만 프레임 증가
            int frameStep = (int)(deltaTime * 10.0f);
            if (frameStep < 1) frameStep = 1;
            ch->ani.x += frameStep;
            if (ch->ani.x >= maxFramesX) ch->ani.x = 0;
        } else {
            ch->ani.x = 0; // 정지 시 프레임 초기화
        }

        ch->imgsrc.x = ch->ani.x * ch->entity->w;
        ch->imgsrc.y = ch->ani.y * ch->entity->h; // 이동 방향에 따라 y값 선택
        ch->imgsrc.w = ch->entity->w;
        ch->imgsrc.h = ch->entity->h;
    }
}


void SendInput(Keystts* input) {
    ssize_t ret = write(sock, input, sizeof(Keystts));
    if (ret < 0) perror("write failed");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <my_id> <server_ip>\n", argv[0]);
        return 1;
    }

    my_id = atoi(argv[1]);
    strcpy(server_ip, argv[2]);

    InitSystem("chara.data", "position.data", my_id);
    InitWindow(&gGames[my_id], "Test", "bg.png", 1980, 1080);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    pthread_t th;
    pthread_create(&th, NULL, ReceiveThread, NULL);

    SDL_AddTimer(16, AnimationCallback, &gGames[my_id]);

    Uint32 lastTime = SDL_GetTicks();
    SDL_bool running = SDL_TRUE;
    while (running) {
        Uint32 now = SDL_GetTicks();
        gGames[my_id].timeDelta = (now - lastTime) / 1000.0f;
        lastTime = now;

        running = InputEvent(&gGames[my_id]);
        
        SendInput(&gGames[my_id].input);

        DrawGame(&gGames[my_id]);

        SDL_Delay(8);
    }

    close(sock);
    CloseWindow(&gGames[my_id]);
    DestroySystem();
    return 0;
}
