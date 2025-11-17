// improved_client_debug.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <sys/time.h>
#include "game.h"

#define PORT 5000
#define MAX_CLIENT 4

typedef struct { uint32_t seq; float x,y; } NetPos;

int my_id, sock;
NetPos latest[MAX_CLIENT];
uint32_t last_seq[MAX_CLIENT];
pthread_mutex_t pm = PTHREAD_MUTEX_INITIALIZER;

extern CharaInfo *gCharaHead;

// utility: ms timestamp (wall clock)
static long long now_ms() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec/1000;
}

// readn / writen (blocking, 안전)
ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n; char *p = buf;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return (n - left); // EOF: return bytes read
        left -= r; p += r;
    }
    return n;
}
ssize_t writen(int fd, const void *buf, size_t n) {
    size_t left = n; const char *p = buf;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= w; p += w;
    }
    return n;
}

// RecvLoop: 블로킹 readn으로 전체 패킷 읽음.
// 디버그: recv jitter, tcp fragment, seq gap 체크
void *RecvLoop(void *a) {
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    NetPos buf[MAX_CLIENT];
    static long long last_recv_ms = 0;

    while (1) {
        ssize_t r = readn(sock, buf, sizeof(buf));
        long long now = now_ms();

        if (r <= 0) {
            fprintf(stderr, "[RecvLoop] readn failed r=%zd (closing)\n", r);
            break;
        }

        // --- TCP fragment check (이건 보통 readn에서 블록되므로 잘 발생하지 않음)
        if ((size_t)r != sizeof(buf)) {
            printf("[TCP FRAGMENT] expected=%zu got=%zd\n", sizeof(buf), r);
        }

        // --- recv jitter 체크
        if (last_recv_ms != 0) {
            long long diff = now - last_recv_ms;
            if (diff > 50) { // 임계값: 50ms
                printf("[RECV JITTER] Packet gap = %lld ms (now=%lld)\n", diff, now);
            }
        }
        last_recv_ms = now;

        // --- 시퀀스 / 손실 체크 & 최신 데이터 갱신
        pthread_mutex_lock(&pm);
        for (int i = 0; i < MAX_CLIENT; ++i) {
            uint32_t seq = buf[i].seq;
            if (seq == 0) continue; // 서버가 비워놓은 슬롯
            // 시퀀스 갭 체크
            if (last_seq[i] != 0 && seq > last_seq[i] + 1) {
                uint32_t lost = seq - last_seq[i] - 1;
                printf("[SEQ GAP] player=%d last=%u now=%u lost=%u\n", i, last_seq[i], seq, lost);
            }
            if (seq <= last_seq[i]) {
                // 오래된/중복 패킷 무시
                continue;
            }
            last_seq[i] = seq;
            latest[i].seq = seq;
            latest[i].x = buf[i].x;
            latest[i].y = buf[i].y;
        }
        pthread_mutex_unlock(&pm);
    }

    close(sock);
    return NULL;
}

void SendInput(Keystts *input)
{
    long long t0 = SDL_GetTicks();
    ssize_t w = writen(sock, input, sizeof(Keystts));
    long long t1 = SDL_GetTicks();

    if (t1 - t0 > 30)
        printf("[WARN] SendInput slow write: %lld ms\n", (t1 - t0));

    if (w != sizeof(Keystts))
        printf("write failed: %zd\n", w);
}

// 애니메이션 타이머
Uint32 AnimCB(Uint32 i, void *p){ UpdateAnimation((GameInfo*)p, i/1000.0f); return i; }

int main(int argc,char*argv[]){
    if(argc<3){ printf("usage: <id> <server-ip>\n"); return 0; }
    my_id=atoi(argv[1]);

    InitSystem("chara.data","position.data",my_id);
    InitWindow(&gGames[my_id], "Test", "bg.png", 1280,720);

    // 초기 latest, last_seq 세팅
    for (int i=0;i<MAX_CLIENT;i++){ latest[i].seq = 0; latest[i].x = 0; latest[i].y = 0; last_seq[i]=0; }

    for(CharaInfo*ch=gCharaHead;ch;ch=ch->next)
        if (ch->type>=CT_PLAYER0 && ch->type<CT_PLAYER0+MAX_CLIENT) {
            int i=ch->type-CT_PLAYER0;
            latest[i].x=ch->point.x;
            latest[i].y=ch->point.y;
            last_seq[i]=0;
        }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in sv={0};
    sv.sin_family=AF_INET;
    sv.sin_port=htons(PORT);
    inet_pton(AF_INET, argv[2], &sv.sin_addr);

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    if (connect(sock,(struct sockaddr*)&sv,sizeof(sv)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    pthread_t th;
    if (pthread_create(&th,NULL,RecvLoop,NULL) != 0) {
        perror("pthread_create");
        close(sock);
        return 1;
    }
    pthread_detach(th);

    SDL_AddTimer(16,AnimCB,&gGames[my_id]);

    Uint32 last_sdl = SDL_GetTicks();
    long long last_frame_ms = now_ms();
    SDL_bool run=SDL_TRUE;
    while(run){
        Uint32 now_sdl = SDL_GetTicks();
        float dt = (now_sdl - last_sdl)/1000.0f;
        gGames[my_id].timeDelta = dt;
        last_sdl = now_sdl;

        // --- 프레임 드랍 체크 (메인루프 지연)
        long long now = now_ms();
        if (last_frame_ms != 0) {
            long long frame_diff = now - last_frame_ms;
            if (frame_diff > 25) { // 25ms 이상이면 40FPS 이하
                printf("[FRAME DROP] main loop took %lld ms\n", frame_diff);
            }
        }
        last_frame_ms = now;

        run = InputEvent(&gGames[my_id]);

        // 입력 송신
        SendInput(&gGames[my_id].input);

        // 서버 보간 적용 + difference 체크
        pthread_mutex_lock(&pm);
        CharaInfo *ch=gCharaHead;
        for(int i=0;i<MAX_CLIENT && ch;i++,ch=ch->next){
            // latest[i] 적용 (시퀀스 검사 이미 했음)
            float dx = latest[i].x - ch->point.x;
            float dy = latest[i].y - ch->point.y;
            float dist = sqrtf(dx*dx + dy*dy);

            // 보간 전 디버그: 큰 변화(텔레포트) 감지
            if (dist > 50.0f) {
                // 가능한 원인 로그: 수신 지연 / 서버 위치 급변 / 보간 파라미터
                printf("[TELEPORT DETECT] player=%d dist=%.2f latest=(%.1f,%.1f) cur=(%.1f,%.1f) seq=%u\n",
                       i, dist, latest[i].x, latest[i].y, ch->point.x, ch->point.y, latest[i].seq);
            }

            if(dist>200.0f){
                // 아주 큰 차이라면 즉시 보정
                ch->point.x = latest[i].x;
                ch->point.y = latest[i].y;
            } else {
                // timeDelta 기반 부드러운 보간 (프레임 독립)
                float t = fminf(1.0f, dt * 10.0f); // dt*10 -> 약 0.1~0.5 범위 권장
                // 보간 전/후 변경량 로그(선택적, 많은 로그일 수 있음)
                // 예: if (dist > 20) printf("[INTERP] player=%d t=%.3f dist=%.2f move=%.2f\n", i, t, dist, dist*t);
                ch->point.x += dx * t;
                ch->point.y += dy * t;
            }
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
