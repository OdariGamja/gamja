// =============================================================
// simplified_client.c  (기능 동일, 구조 단순화)
// =============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "game.h"

#define PORT 5000
#define MAX_CLIENT 4

typedef struct { float x,y; } NetPos;

int my_id, sock;
NetPos latest[MAX_CLIENT];
pthread_mutex_t pm = PTHREAD_MUTEX_INITIALIZER;

extern CharaInfo *gCharaHead;  // これがgame.cで定義されていることを確認


// read/write
ssize_t readn(int fd, void *buf, size_t n) {
    size_t left=n; char*p=buf;
    while(left>0){ 
        ssize_t r=read(fd,p,left);
     if(r<=0)return(r==0?n-left:-1); 
     left-=r;
     p+=r;
     } 
     return n;
}
ssize_t writen(int fd,const void*buf,size_t n){
    size_t l=n;
    const char*p=buf;
    while(l>0){
        ssize_t w=write(fd,p,l);
        if(w<=0)return-1;
        l-=w;
        p+=w;
        }
        return n;
        }

// 서버 수신 스레드
void *RecvLoop(void *a){
    while(1){
        NetPos buf[MAX_CLIENT];
        if (readn(sock, buf, sizeof(buf)) <= 0) break;

        pthread_mutex_lock(&pm);
        memcpy(latest, buf, sizeof(buf));
        pthread_mutex_unlock(&pm);
    }
    return NULL;
}
void SendInput(Keystts *input)
{
    if (writen(sock, input, sizeof(Keystts)) != sizeof(Keystts))
    {   
        perror("write failed");
    }
}
// 애니메이션 타이머
Uint32 AnimCB(Uint32 i, void *p){ UpdateAnimation((GameInfo*)p, i/1000.0f); return i; }

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("usage: <server-ip>\n");
        return 0;
    }

    // 1) 서버 연결
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sv = {0};
    sv.sin_family = AF_INET;
    sv.sin_port   = htons(PORT);
    inet_pton(AF_INET, argv[1], &sv.sin_addr);

    connect(sock, (struct sockaddr*)&sv, sizeof(sv));

    // 2) 서버가 보내는 id 받기
    if (readn(sock, &my_id, sizeof(int)) != sizeof(int)) {
        printf("Failed to get my_id from server\n");
        return 0;
    }
    printf("My ID from server = %d\n", my_id);

    // 3) 이제 여기서 InitSystem 실행
    InitSystem("chara.data", "position.data", my_id);

    // 4) 내 ID에 해당하는 GameInfo 로 윈도우 생성
    InitWindow(&gGames[my_id], "Test", "bg.png", 1980, 1080);
    
    
    // 5) RecvLoop 시작
    pthread_t th;
    pthread_create(&th, NULL, RecvLoop, NULL);

    // 6) 애니메이션 타이머 추가
    SDL_AddTimer(16, AnimCB, &gGames[my_id]);

    // 7) 메인 루프 시작
    Uint32 last = SDL_GetTicks();
    SDL_bool run = SDL_TRUE;

    while (run) {
        Uint32 now = SDL_GetTicks();
        gGames[my_id].timeDelta = (now - last) / 1000.0f;
        last = now;

        run = InputEvent(&gGames[my_id]);
        SendInput(&gGames[my_id].input);

        // 보간 처리
        pthread_mutex_lock(&pm);
        CharaInfo *ch = gCharaHead;
        while(ch){
            int id = ch->type - CT_PLAYER0;
            if(id >= 0 && id < MAX_CLIENT){
                float t = 0.2f;
                ch->point.x += (latest[id].x - ch->point.x) * t;
                ch->point.y += (latest[id].y - ch->point.y) * t;
                ch->rect.x = (int)ch->point.x;
                ch->rect.y = (int)ch->point.y;
            }
            ch = ch->next;
        }
        pthread_mutex_unlock(&pm);

        
        // カメラアップデート 
        CharaInfo *myChar = GetPlayerChara(my_id);
        GameInfo *myGame = &gGames[my_id];   // my_id 基準
        UpdateCamera(myGame, myChar);
        printf("[DEBUG] myChar type=%d pos=%.2f,%.2f\n",
            myChar->type, myChar->point.x, myChar->point.y);
        printf("[DEBUG] myGame cam=(%.2d,%.2d)\n",
            myGame->cam.x, myGame->cam.y);
        

        DrawGame(&gGames[my_id]);
        SDL_Delay(8);
    }

    CloseWindow(&gGames[my_id]);
    DestroySystem();
    close(sock);
    return 0;
}

