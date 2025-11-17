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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "game.h"

#define PORT 5001
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
// ================= Non-blocking RecvLoop() =================

// ================= Non-blocking RecvLoop() =================

void *RecvLoop(void *a)
{
    // ソケットをノンブロッキング化
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    while (1) {
        NetPos buf[MAX_CLIENT];

        ssize_t r = recv(sock, buf, sizeof(buf), 0);

        if (r > 0) {
            pthread_mutex_lock(&pm);
            memcpy(latest, buf, sizeof(buf));
            pthread_mutex_unlock(&pm);
        }
        else if (r == 0) {
            printf("Server disconnected\n");
            break;
        }
        else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // データなし → 少し休む
                usleep(1000);
                continue;
            }
            else {
                printf("recv error\n");
                break;
            }
        }
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

int main(int argc,char*argv[]){
    if(argc<3){ printf("usage: <id> <server-ip>\n"); return 0; }
    my_id=atoi(argv[1]);

    InitSystem("chara.data","position.data",my_id);
    InitWindow(&gGames[my_id], "Test", "bg.png", 1280,720);

    for(CharaInfo*ch=gCharaHead;ch;ch=ch->next)
        if (ch->type>=CT_PLAYER0 && ch->type<CT_PLAYER0+MAX_CLIENT) {
            int i=ch->type-CT_PLAYER0;
            latest[i].x=ch->point.x;
            latest[i].y=ch->point.y;
        }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sv={0};
    sv.sin_family=AF_INET;
    sv.sin_port=htons(PORT);
    inet_pton(AF_INET, argv[2], &sv.sin_addr);

    connect(sock,(struct sockaddr*)&sv,sizeof(sv));

    pthread_t th;
    pthread_create(&th,NULL,RecvLoop,NULL);

    SDL_AddTimer(16,AnimCB,&gGames[my_id]);

    Uint32 last = SDL_GetTicks();
    SDL_bool run=SDL_TRUE;
    while(run){
        Uint32 now=SDL_GetTicks();
        gGames[my_id].timeDelta=(now-last)/1000.0f;
        last=now;

        run = InputEvent(&gGames[my_id]);

        // 입력 송신
        SendInput(&gGames[my_id].input);

        // 서버 보간 적용
        pthread_mutex_lock(&pm);
        CharaInfo *ch=gCharaHead;
        for(int i=0;i<MAX_CLIENT && ch;i++,ch=ch->next){
            float t=0.2f;
            ch->point.x += (latest[i].x - ch->point.x)*t;
            ch->point.y += (latest[i].y - ch->point.y)*t;
            ch->rect.x=(int)ch->point.x;
            ch->rect.y=(int)ch->point.y;
        }
        pthread_mutex_unlock(&pm);

        DrawGame(&gGames[my_id]);
        SDL_Delay(8);
    }

    CloseWindow(&gGames[my_id]);
    DestroySystem();
    close(sock);
    return 0;
}
