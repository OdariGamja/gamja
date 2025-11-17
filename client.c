#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <netinet/tcp.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <sys/time.h>
#include "game.h"

#define PORT 5000
#define MAX_CLIENT 4
#define MOVE_SPEED 7.f

typedef struct { uint32_t seq; float x,y; Keystts lastInput; long long lastRecvMs; } NetPosEx;

int my_id, sock;
NetPosEx latest[MAX_CLIENT];
uint32_t last_seq[MAX_CLIENT];
pthread_mutex_t pm = PTHREAD_MUTEX_INITIALIZER;

extern CharaInfo *gCharaHead;

// --- 유틸 ---
static long long now_ms() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec/1000;
}

ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n; char *p = buf;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) { if(errno==EINTR) continue; return -1; }
        if (r == 0) return (n-left);
        left -= r; p += r;
    }
    return n;
}

ssize_t writen(int fd, const void *buf, size_t n) {
    size_t left = n; const char *p = buf;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w <= 0) { if(errno==EINTR) continue; return -1; }
        left -= w; p += w;
    }
    return n;
}

// --- 수신 루프 ---
void *RecvLoop(void *a) {
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    NetPosEx buf[MAX_CLIENT];
    static long long last_recv_ms = 0;

    while (1) {
        ssize_t r = readn(sock, buf, sizeof(buf));
        long long now = now_ms();
        if (r <= 0) break;

        pthread_mutex_lock(&pm);
        for (int i = 0; i < MAX_CLIENT; ++i) {
            uint32_t seq = buf[i].seq;
            if (seq == 0) continue;
            if (seq <= last_seq[i]) continue;

            last_seq[i] = seq;
            latest[i].seq = seq;
            latest[i].x = buf[i].x;
            latest[i].y = buf[i].y;
            latest[i].lastInput = buf[i].lastInput;
            latest[i].lastRecvMs = now;
        }
        pthread_mutex_unlock(&pm);
    }

    close(sock);
    return NULL;
}

// --- 입력 전송 ---
void SendInput(Keystts *input) {
    writen(sock, input, sizeof(Keystts));
}

// --- 예측/보간 함수 ---
void UpdatePrediction(float dt) {
    pthread_mutex_lock(&pm);
    CharaInfo *ch = gCharaHead;
    for (int i=0;i<MAX_CLIENT && ch;i++,ch=ch->next){
        float dx = latest[i].x - ch->point.x;
        float dy = latest[i].y - ch->point.y;
        float dist = sqrtf(dx*dx + dy*dy);

        // --- 서버 위치 기반 예측
        long long now = now_ms();
        float dt_sec = (now - latest[i].lastRecvMs)/1000.0f;

        float pred_x = latest[i].x;
        float pred_y = latest[i].y;
        Keystts inp = latest[i].lastInput;
        float vx=0, vy=0;
        if(inp.right) vx += MOVE_SPEED;
        if(inp.left)  vx -= MOVE_SPEED;
        if(inp.down)  vy += MOVE_SPEED;
        if(inp.up)    vy -= MOVE_SPEED;
        if(vx && vy){ vx /= sqrtf(2); vy /= sqrtf(2); }
        pred_x += vx*dt_sec; pred_y += vy*dt_sec;

        // --- 보간 + 예측 혼합
        float t = fminf(1.0f, dt*10.0f);
        ch->point.x += (pred_x - ch->point.x)*t;
        ch->point.y += (pred_y - ch->point.y)*t;

        ch->rect.x = (int)ch->point.x;
        ch->rect.y = (int)ch->point.y;
    }
    pthread_mutex_unlock(&pm);
}

// --- SDL 애니메이션 콜백 ---
Uint32 AnimCB(Uint32 i, void *p){ UpdateAnimation((GameInfo*)p, i/1000.0f); return i; }

int main(int argc,char*argv[]){
    if(argc<3){ printf("usage: <id> <server-ip>\n"); return 0; }
    my_id = atoi(argv[1]);

    InitSystem("chara.data","position.data",my_id);
    InitWindow(&gGames[my_id], "Test", "bg.png", 1280,720);

    // 초기화
    for(int i=0;i<MAX_CLIENT;i++){ latest[i].seq=0; latest[i].x=0; latest[i].y=0; last_seq[i]=0; }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sv={0};
    sv.sin_family=AF_INET; sv.sin_port=htons(PORT);
    inet_pton(AF_INET, argv[2], &sv.sin_addr);

    int flag=1; setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    if(connect(sock,(struct sockaddr*)&sv,sizeof(sv))<0){ perror("connect"); return 1; }

    pthread_t th; pthread_create(&th,NULL,RecvLoop,NULL); pthread_detach(th);

    SDL_AddTimer(16, AnimCB, &gGames[my_id]);

    Uint32 last_sdl = SDL_GetTicks();
    SDL_bool run=SDL_TRUE;
    while(run){
        Uint32 now_sdl = SDL_GetTicks();
        float dt = (now_sdl - last_sdl)/1000.0f;
        last_sdl = now_sdl;

        run = InputEvent(&gGames[my_id]);
        SendInput(&gGames[my_id].input);

        UpdatePrediction(dt);

        DrawGame(&gGames[my_id]);
        SDL_Delay(8);
    }

    CloseWindow(&gGames[my_id]);
    DestroySystem();
    close(sock);
    return 0;
}
