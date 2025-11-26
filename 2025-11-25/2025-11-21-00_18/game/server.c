//server.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include "game.h"
#include "server_system.h"

#define PORT 5000        // サーバーポート
#define MAX_CLIENT 4     // 最大クライアント数
#define TICK_MS 33       // ゲームループの更新間隔（ミリ秒）

typedef struct { float x, y; } NetPos;  // クライアントに送信する座標パケット

int csock[MAX_CLIENT];           // 各クライアントのソケット
int alive[MAX_CLIENT];           // クライアント接続状態
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;  // csock/alive の排他用

CharaInfo players[MAX_CLIENT];   // プレイヤー情報

//========================
// 指定バイト数の読み込み
//========================
ssize_t readn(int fd, void *buf, size_t n)
{
    size_t left = n; 
    char *p = buf;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r <= 0) return (r == 0 ? n-left : -1);  // EOF またはエラー
        left -= r; 
        p += r;
    }
    return n;
}

//========================
// 指定バイト数の書き込み
//========================
ssize_t writen(int fd, const void *buf, size_t n)
{
    size_t left = n; 
    const char *p = buf;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w <= 0) return -1;  // エラー
        left -= w; 
        p += w;
    }
    return n;
}

//========================
// サーバー側のプレイヤー移動
//========================
void MoveServer(CharaInfo *ch)
{
    if (!ch || ch->stts == CS_Disable) return;

    float vx=0, vy=0;
    if (ch->input.right) vx += MOVE_SPEED;
    if (ch->input.left)  vx -= MOVE_SPEED;
    if (ch->input.down)  vy += MOVE_SPEED;
    if (ch->input.up)    vy -= MOVE_SPEED;

    // 斜め移動補正
    if (vx && vy) { vx /= sqrtf(2); vy /= sqrtf(2); }

    ch->point.x += vx;
    ch->point.y += vy;

    // マップ外に出ないように制限
    if (ch->point.x < 0) ch->point.x = 0;
    if (ch->point.y < 0) ch->point.y = 0;
    if (ch->point.x + ch->rect.w > MAP_Width) ch->point.x = MAP_Width - ch->rect.w;
    if (ch->point.y + ch->rect.h > MAP_Height) ch->point.y = MAP_Height - ch->rect.h;

    // rect の位置を更新
    ch->rect.x = (int)ch->point.x;
    ch->rect.y = (int)ch->point.y;
}

//========================
// 現在時刻（ミリ秒）取得
//========================
long long now_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec*1000 + tv.tv_usec/1000;
}

//========================
// クライアント受信スレッド
//========================
void *RecvThread(void *arg)
{
    int id = *(int*)arg; 
    free(arg);  
    int s = csock[id];

    while (1) {
        Keystts inp;
        if (readn(s, &inp, sizeof(inp)) <= 0) break;  // 切断
        players[id].input = inp;  // キー入力更新
        
    }

    // 切断処理
    pthread_mutex_lock(&mtx);
    close(csock[id]);
    csock[id] = -1;
    alive[id] = 0;
    pthread_mutex_unlock(&mtx);

    printf("Client %d disconnected\n", id);
    return NULL;
}

//========================
// ゲームループスレッド
//========================
void *GameLoop(void *arg)
{
    long long last = now_ms();

    while (1) {
        long long cur = now_ms();
        if (cur - last < TICK_MS) { 
            usleep((TICK_MS-(cur-last))*1000); 
            continue; 
        }
        last = cur;

        // プレイヤー移動更新
        for (CharaInfo* ch = SevergCharaHead; ch; ch = ch->next){
            if (ch->type >= CT_PLAYER0 && ch->type < CT_PLAYER0 + MAX_CLIENT && alive[ch->type - CT_PLAYER0]){
                MoveServer(ch);
                }
            }
            


        // 衝突判定
        for (CharaInfo* p = SevergCharaHead; p; p = p->next){
            for (CharaInfo* q = p->next; q; q = q->next){
                Collision(p, q);
            }
        }
            


        // クライアントに送る座標パケット作成
        NetPos pack[MAX_CLIENT];
        for (int i=0; i<MAX_CLIENT; i++) {
            pack[i].x = players[i].point.x;
            pack[i].y = players[i].point.y;
        }


        // 送信
        pthread_mutex_lock(&mtx);
        for (int i=0; i<MAX_CLIENT; i++)
            if (alive[i] && csock[i] != -1)
                if (writen(csock[i], pack, sizeof(pack)) < 0) { 
                    close(csock[i]); 
                    csock[i]=-1; 
                    alive[i]=0; 
                }
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

//========================
// メイン
//========================
int main()
{    
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // ソケットオプション（アドレス再利用）
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, MAX_CLIENT);
    printf("Server ready.\n");

    // プレイヤー初期化
    for (int i=0;i<MAX_CLIENT;i++){
        csock[i]=-1; 
        alive[i]=0;
        memset(&players[i].input,0,sizeof(players[i].input));
    }

    // NPC/プレイヤー情報を linked list に初期化
    InitServerChara("position.data", players, MAX_CLIENT);

    // ゲームループスレッド作成
    pthread_t gl;
    pthread_create(&gl,NULL,GameLoop,NULL);
    pthread_detach(gl);

    // クライアント接続待ち
    while(1){
        int cs = accept(s,NULL,NULL);
        pthread_mutex_lock(&mtx);

        // 空きID検索
        int id=-1;
        for (int i=0;i<MAX_CLIENT;i++) 
            if(!alive[i]){id=i;break;}

        if(id<0){close(cs); pthread_mutex_unlock(&mtx); continue;}  // 空きなし

        csock[id]=cs; 
        alive[id]=1;

        // 클라이언트에게 플레이어 id 전달
        writen(cs, &id, sizeof(int));

        pthread_mutex_unlock(&mtx);

        printf("Client %d connected\n", id);

        // 受信スレッド作成
        int *pid = malloc(sizeof(int)); 
        *pid=id;
        pthread_t th;
        pthread_create(&th,NULL,RecvThread,pid);
        pthread_detach(th);
    }
    return 0;
}
