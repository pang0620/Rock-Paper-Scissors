/* 서울기술교육센터 AIoT - 가위바위보 서버 (POSIX socket + pthread)
 * author: KSH (base) / edited for RPS by ChatGPT
 *
 * 빌드:  gcc -O2 -Wall -pthread -o rps_server server.c
 * 실행:  ./rps_server 5000
 * 인증:  클라이언트는 접속 직후 "id:pw" 한 줄을 보낸다. (개행 없어도 됨)
 * 계정:  실행 디렉터리의 idpasswd.txt (형식: "<id> <pw>" 공백 구분, 한 줄당 1 계정)
 *
 * 게임:  단일 방(슬롯 2개)만 운영. 두 명이 모이면 START_ROUND가 브로드캐스트됨.
 *        클라가 "HAND:<hand>:<round>\n" 보낼 때 둘 다 도착하면 즉시 RESULT 전송 후 다음 라운드.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <stdarg.h>

#define BUF_SIZE   1024
#define MAX_CLNT   256
#define ID_SIZE    32
#define ARR_CNT    5

typedef struct {
    int index;
    int fd;
    char ip[20];
    char id[ID_SIZE];
    char pw[ID_SIZE];
} CLIENT_INFO;

typedef struct {
    int  p1_fd;
    int  p2_fd;
    char p1_id[ID_SIZE];
    char p2_id[ID_SIZE];
    char p1_hand[12];  // "rock"/"paper"/"scissors" 또는 ""
    char p2_hand[12];
    int  round;
    int  score1;
    int  score2;
} GAME_STATE;

/* --- 전역 --- */
static int clnt_cnt = 0;
static pthread_mutex_t mutx      = PTHREAD_MUTEX_INITIALIZER; // client list lock
static pthread_mutex_t game_mtx  = PTHREAD_MUTEX_INITIALIZER; // game state lock
static GAME_STATE game = { .p1_fd = -1, .p2_fd = -1, .round = 1, .score1 = 0, .score2 = 0 };

/* --- 프로토타입 --- */
static void *clnt_connection(void *arg);
static void  log_file(const char *msg);
static void  error_exit(const char *msg);
static void  send_line(int fd, const char *fmt, ...);
static void  broadcast_to_players(const char *fmt, ...);
static const char *judge_winner(const char *h1, const char *h2);
static void  maybe_judge_and_advance(void);

/* ----------------- 유틸 ----------------- */
static void log_file(const char *msg) {
    fputs(msg, stdout);
    fflush(stdout);
}
static void error_exit(const char *msg) {
    perror(msg);
    exit(1);
}
static void send_line(int fd, const char *fmt, ...) {
    if (fd < 0) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf)-2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    int pos = (n < (int)sizeof(buf)-2) ? n : (int)sizeof(buf)-2;
    buf[pos] = '\n';
    buf[pos+1] = '\0';
    (void)write(fd, buf, strlen(buf));
}
static void broadcast_to_players(const char *fmt, ...) {
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line)-2, fmt, ap);
    va_end(ap);
    int pos = (n < (int)sizeof(line)-2) ? n : (int)sizeof(line)-2;
    line[pos] = '\n';
    line[pos+1] = '\0';

    if (game.p1_fd >= 0) (void)write(game.p1_fd, line, strlen(line));
    if (game.p2_fd >= 0 && game.p2_fd != game.p1_fd) (void)write(game.p2_fd, line, strlen(line));
}

/* -------------- 판정 & 라운드 -------------- */
static const char *judge_winner(const char *h1, const char *h2) {
    if (!h1 || !h2) return "draw";
    if (strcmp(h1, h2) == 0) return "draw";
    if (!strcmp(h1,"rock")     && !strcmp(h2,"scissors")) return "p1";
    if (!strcmp(h1,"scissors") && !strcmp(h2,"paper"))    return "p1";
    if (!strcmp(h1,"paper")    && !strcmp(h2,"rock"))     return "p1";
    return "p2"; // 나머지는 p2 승
}

static void maybe_judge_and_advance(void) {
    if (game.p1_fd < 0 || game.p2_fd < 0) return;
    if (game.p1_hand[0] == '\0' || game.p2_hand[0] == '\0') return;

    const char *win = judge_winner(game.p1_hand, game.p2_hand);
    if (!strcmp(win, "p1")) game.score1++;
    else if (!strcmp(win, "p2")) game.score2++;

    broadcast_to_players("RESULT:%d:%s:%s:%s:%d:%d",
        game.round, game.p1_hand, game.p2_hand, win, game.score1, game.score2);

    // 다음 라운드
    game.round++;
    game.p1_hand[0] = '\0';
    game.p2_hand[0] = '\0';

    send_line(game.p1_fd, "START_ROUND:%d:%s", game.round, game.p2_id); // p1에게 p2의 ID 전송
    send_line(game.p2_fd, "START_ROUND:%d:%s", game.round, game.p1_id); // p2에게 p1의 ID 전송
}

/* -------------- 메인 -------------- */
int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    int sock_option = 1;
    pthread_t t_id[MAX_CLNT] = {0};

    int i = 0, str_len = 0;
    char idpasswd[(ID_SIZE*2)+8];
    char *pToken;
    char *pArray[ARR_CNT] = {0};

    /* 계정 로드 */
    FILE *idFd = fopen("idpasswd.txt","r");
    if (idFd == NULL) {
        error_exit("fopen(\"idpasswd.txt\",\"r\")");
    }
    CLIENT_INFO *client_info = (CLIENT_INFO *)calloc(MAX_CLNT, sizeof(CLIENT_INFO));
    if (!client_info) error_exit("calloc(client_info)");

    char id[ID_SIZE], pw[ID_SIZE];
    while ((str_len = fscanf(idFd, "%31s %31s", id, pw)) == 2) {
        client_info[i].fd = -1;
        strncpy(client_info[i].id, id, ID_SIZE-1);
        strncpy(client_info[i].pw, pw, ID_SIZE-1);
        i++;
        if (i >= MAX_CLNT) break;
    }
    fclose(idFd);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    fputs("RPS IoT Server Start!!\n", stdout);

    if (pthread_mutex_init(&mutx, NULL) != 0) error_exit("mutex init (mutx)");
    if (pthread_mutex_init(&game_mtx, NULL) != 0) error_exit("mutex init (game_mtx)");

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock < 0) error_exit("socket()");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_option, sizeof(sock_option));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_exit("bind()");
    if (listen(serv_sock, 16) == -1)
        error_exit("listen()");

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        if (clnt_sock < 0) {
            perror("accept()");
            continue;
        }

        if (clnt_cnt >= MAX_CLNT) {
            send_line(clnt_sock, "ERROR:Server full");
            shutdown(clnt_sock, SHUT_RDWR);
            close(clnt_sock);
            continue;
        }

        /* 1) 접속 즉시 자격증명 수신 ("id:pw") */
        str_len = read(clnt_sock, idpasswd, sizeof(idpasswd)-1);
        if (str_len <= 0) {
            shutdown(clnt_sock, SHUT_RDWR);
            close(clnt_sock);
            continue;
        }
        idpasswd[str_len] = '\0';
        // 줄바꿈 제거
        char *nl = strchr(idpasswd, '\n');
        if (nl) *nl = '\0';
        nl = strchr(idpasswd, '\r');
        if (nl) *nl = '\0';

        /* 2) 파싱 (구분자 ":") */
        int tokc = 0;
        pToken = strtok(idpasswd, ":");
        while (pToken && tokc < ARR_CNT) {
            pArray[tokc++] = pToken;
            pToken = strtok(NULL, ":");
        }
        if (tokc < 2) {
            send_line(clnt_sock, "ERROR:BAD_AUTH_FORMAT");
            shutdown(clnt_sock, SHUT_RDWR);
            close(clnt_sock);
            continue;
        }

        /* 3) 인증 */
        int found = -1;
        for (i = 0; i < MAX_CLNT; i++) {
            if (client_info[i].id[0] == '\0') break; // 계정 목록 끝
            if (!strcmp(client_info[i].id, pArray[0])) {
                found = i;
                break;
            }
        }

        if (found < 0 || strcmp(client_info[found].pw, pArray[1]) != 0) {
            send_line(clnt_sock, "ERROR:AUTH_FAIL");
            shutdown(clnt_sock, SHUT_RDWR);
            close(clnt_sock);
            continue;
        }

        if (client_info[found].fd != -1) {
            // 이미 로그인 중
            send_line(clnt_sock, "ERROR:ALREADY_LOGGED");
            shutdown(clnt_sock, SHUT_RDWR);
            close(clnt_sock);
            continue;
        }

        // 등록
        strncpy(client_info[found].ip, inet_ntoa(clnt_adr.sin_addr), sizeof(client_info[found].ip)-1);
        pthread_mutex_lock(&mutx);
        client_info[found].index = found;
        client_info[found].fd = clnt_sock;
        clnt_cnt++;
        pthread_mutex_unlock(&mutx);

        char msg[256];
        snprintf(msg, sizeof(msg), "[%s] New connected! (ip:%s,fd:%d,sockcnt:%d)\n",
                 client_info[found].id, client_info[found].ip, clnt_sock, clnt_cnt);
        log_file(msg);

        /* 4) 슬롯 배정 + 시작 알림 */
        pthread_mutex_lock(&game_mtx);
        if (game.p1_fd < 0) {
            game.p1_fd = clnt_sock;
            strncpy(game.p1_id, client_info[found].id, ID_SIZE-1);
            send_line(clnt_sock, "WELCOME:1");
            if (game.p2_fd < 0) send_line(clnt_sock, "WAIT");
        } else if (game.p2_fd < 0) {
            game.p2_fd = clnt_sock;
            strncpy(game.p2_id, client_info[found].id, ID_SIZE-1);
            send_line(clnt_sock, "WELCOME:2");
            // 두 명 모였으니 라운드 시작
            send_line(game.p1_fd, "START_ROUND:%d:%s", game.round, game.p2_id); // p1에게 p2의 ID 전송
            send_line(game.p2_fd, "START_ROUND:%d:%s", game.round, game.p1_id); // p2에게 p1의 ID 전송
        } else {
            // 방이 찼음 -> 거절
            pthread_mutex_unlock(&game_mtx);
            send_line(clnt_sock, "ERROR:Room full");
            shutdown(clnt_sock, SHUT_RDWR);
            close(clnt_sock);
            // 클라이언트 목록 되돌리기
            pthread_mutex_lock(&mutx);
            client_info[found].fd = -1;
            clnt_cnt--;
            pthread_mutex_unlock(&mutx);
            continue;
        }
        pthread_mutex_unlock(&game_mtx);

        /* 5) 스레드 생성 */
        pthread_create(&t_id[found], NULL, clnt_connection, (void *)(client_info + found));
        pthread_detach(t_id[found]);
    }

    return 0;
}

/* -------------- 클라 스레드 -------------- */
static void *clnt_connection(void *arg) {
    CLIENT_INFO *client_info = (CLIENT_INFO *)arg;
    char msg[BUF_SIZE];
    int str_len;

    while (1) {
        memset(msg, 0, sizeof(msg));
        str_len = read(client_info->fd, msg, sizeof(msg)-1);
        if (str_len <= 0) break;

        msg[str_len] = '\0';
        // \r\n 제거
        char *nl = strchr(msg, '\n');
        if (nl) *nl = '\0';
        nl = strchr(msg, '\r');
        if (nl) *nl = '\0';

        // 토큰화: HAND:<hand>:<round>
        int tokc = 0;
        char *tokv[4] = {0};
        char *saveptr = NULL;
        char *t = strtok_r(msg, ":", &saveptr);
        while (t && tokc < 4) { tokv[tokc++] = t; t = strtok_r(NULL, ":", &saveptr); }

        if (tokc >= 1 && !strcmp(tokv[0], "HAND")) {
            if (tokc < 3) { send_line(client_info->fd, "ERROR:BAD_HAND_FORMAT"); continue; }
            const char *hand = tokv[1];
            int round_in = atoi(tokv[2]);

            if (strcmp(hand,"rock") && strcmp(hand,"paper") && strcmp(hand,"scissors")) {
                send_line(client_info->fd, "ERROR:BAD_HAND_VALUE");
                continue;
            }

            pthread_mutex_lock(&game_mtx);
            if (round_in != game.round) {
                pthread_mutex_unlock(&game_mtx);
                send_line(client_info->fd, "ERROR:WRONG_ROUND");
                continue;
            }

            if (client_info->fd == game.p1_fd) {
                strncpy(game.p1_hand, hand, sizeof(game.p1_hand)-1);
                game.p1_hand[sizeof(game.p1_hand)-1] = '\0';
            } else if (client_info->fd == game.p2_fd) {
                strncpy(game.p2_hand, hand, sizeof(game.p2_hand)-1);
                game.p2_hand[sizeof(game.p2_hand)-1] = '\0';
            } else {
                pthread_mutex_unlock(&game_mtx);
                send_line(client_info->fd, "ERROR:NOT_IN_ROOM");
                continue;
            }

            maybe_judge_and_advance();
            pthread_mutex_unlock(&game_mtx);
            continue;
        }
        else if (tokc >= 1 && !strcmp(tokv[0], "HEARTBEAT")) {
            // 필요 시 시간 갱신 로직 추가 가능
            continue;
        }
        else if (tokc >= 1 && !strcmp(tokv[0], "QUIT")) {
            break;
        }
        else {
            send_line(client_info->fd, "ERROR:UNKNOWN_CMD");
        }
    }

    /* 연결 종료 처리 */
    int closed_fd = client_info->fd;
    close(client_info->fd);

    pthread_mutex_lock(&game_mtx);
    int was_p1 = (closed_fd == game.p1_fd);
    int was_p2 = (closed_fd == game.p2_fd);

    if (was_p1) {
        game.p1_fd = -1;
        game.p1_id[0] = '\0';
        game.p1_hand[0] = '\0';
        if (game.p2_fd >= 0) send_line(game.p2_fd, "OPPONENT_LEFT");
    }
    if (was_p2) {
        game.p2_fd = -1;
        game.p2_id[0] = '\0';
        game.p2_hand[0] = '\0';
        if (game.p1_fd >= 0) send_line(game.p1_fd, "OPPONENT_LEFT");
    }
    pthread_mutex_unlock(&game_mtx);

    char buf[256];
    snprintf(buf, sizeof(buf), "Disconnect ID:%s (ip:%s,fd:%d,sockcnt:%d)\n",
             client_info->id, client_info->ip, closed_fd, clnt_cnt-1);
    log_file(buf);

    pthread_mutex_lock(&mutx);
    clnt_cnt--;
    client_info->fd = -1;
    pthread_mutex_unlock(&mutx);

    return NULL;
}

