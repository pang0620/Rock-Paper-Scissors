/* 서울기술 교육센터 IoT - RPS Client (POSIX socket + pthread)
 * author: KSH (base) / edited for RPS by ChatGPT
 *
 * 빌드:  gcc -O2 -Wall -pthread -o rps_client client.c
 * 실행:  ./rps_client 127.0.0.1 5000 P1 pass1
 *
 * 프로토콜(서버 기준):
 *  - 접속 직후: "id:pw\n" 전송
 *  - 서버→클라: WELCOME:<slot> / WAIT / START_ROUND:<round> / RESULT:... / OPPONENT_LEFT / ERROR:...
 *  - 클라→서버: HAND:<hand>:<round>\n   (hand ∈ {rock,paper,scissors})
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define BUF_SIZE   1024
#define NAME_SIZE  32

static void *send_msg(void *arg);
static void *recv_msg(void *arg);
static void  error_exit(const char *msg);

/* 공유 상태 (라운드 동기화) */
static pthread_mutex_t round_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  round_cv  = PTHREAD_COND_INITIALIZER;
static int  round_ready = 0;     // 1이면 입력을 받아 전송
static int  current_round = 0;
static int  running = 1;

static int  sock_global = -1;

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t th_snd, th_rcv;

    setvbuf(stdout, NULL, _IONBF, 0); // 로그 flush 즉시

    if (argc != 5) {
        fprintf(stderr, "Usage : %s <IP> <port> <id> <pw>\n", argv[0]);
        exit(1);
    }
    const char *ip  = argv[1];
    const int   port= atoi(argv[2]);
    const char *id  = argv[3];
    const char *pw  = argv[4];

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) error_exit("socket()");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_exit("connect()");

    sock_global = sock;

    /* 접속 즉시 자격증명 전송: "id:pw\n" */
    char cred[NAME_SIZE*2 + 8];
    snprintf(cred, sizeof(cred), "%s:%s\n", id, pw);
    if (write(sock, cred, strlen(cred)) <= 0) {
        error_exit("write(cred)");
    }

    /* 수신/송신 스레드 */
    pthread_create(&th_rcv, NULL, recv_msg, (void *)&sock);
    pthread_create(&th_snd, NULL, send_msg, (void *)&sock);

    /* 송신 스레드만 join, 수신은 내부에서 종료될 수 있음 */
    pthread_join(th_snd, NULL);

    running = 0; // 종료 신호
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return 0;
}

/* ---- 송신 스레드: 라운드가 시작되면 사용자 입력 받아 HAND 전송 ---- */
static void *send_msg(void *arg)
{
    int *psock = (int *)arg;
    char line[128];

    while (running) {
        /* 라운드 시작을 기다림 */
        pthread_mutex_lock(&round_mtx);
        while (running && !round_ready) {
            pthread_cond_wait(&round_cv, &round_mtx);
        }
        int round_to_send = current_round;
        int can_send = round_ready; // 1이면 전송 타임
        pthread_mutex_unlock(&round_mtx);

        if (!running) break;
        if (!can_send) continue;

        /* 안내 및 입력 */
        printf("Round %d started. Type your hand (rock/paper/scissors) or 'quit': ", round_to_send);
        if (!fgets(line, sizeof(line), stdin)) {
            // stdin 종료
            break;
        }
        // 개행 제거
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (strcmp(line, "quit") == 0) {
            const char *q = "QUIT\n";
            write(*psock, q, strlen(q));
            break;
        }

        /* 소문자 normalize */
        for (char *p = line; *p; ++p) {
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        }

        if (strcmp(line, "rock") && strcmp(line, "paper") && strcmp(line, "scissors")) {
            printf("Invalid hand. Please type exactly one of {rock, paper, scissors}.\n");
            continue; // 다시 입력 대기 (여전히 same round)
        }

        /* HAND:<hand>:<round>\n 전송 */
        char out[128];
        int n = snprintf(out, sizeof(out), "HAND:%s:%d\n", line, round_to_send);
        if (n < 0 || n >= (int)sizeof(out)) continue;

        if (write(*psock, out, strlen(out)) <= 0) {
            perror("write(HAND)");
            break;
        }

        /* 이 라운드에 대한 입력은 완료 */
        pthread_mutex_lock(&round_mtx);
        // 만약 이미 서버가 결과를 보내 라운드가 넘어갔다면 round_ready는 서버 수신 스레드에서 0으로 내려갔을 것.
        // 여기서는 안전하게 내리는 처리만.
        round_ready = 0;
        pthread_mutex_unlock(&round_mtx);
    }

    return NULL;
}

/* ---- 수신 스레드: 서버 라인 파싱, 라운드/결과/오류 표시 ---- */
static void *recv_msg(void *arg)
{
    int *psock = (int *)arg;
    char rbuf[BUF_SIZE*2];
    int  rlen = 0;

    while (running) {
        int n = read(*psock, rbuf + rlen, (int)sizeof(rbuf) - 1 - rlen);
        if (n <= 0) {
            if (n < 0) perror("read()");
            fprintf(stderr, "\n[Disconnected from server]\n");
            running = 0;
            break;
        }
        rlen += n;
        rbuf[rlen] = '\0';

        /* 라인 단위('\n')로 처리 */
        char *start = rbuf;
        char *lf;
        while ((lf = strchr(start, '\n')) != NULL) {
            *lf = '\0';
            // CR 제거
            char *cr = strchr(start, '\r');
            if (cr) *cr = '\0';

            if (*start) {
                // 한 줄 처리
                // printf("<< %s\n", start); // 디버그

                if (!strncmp(start, "WELCOME:", 8)) {
                    int slot = atoi(start + 8);
                    printf("[SERVER] WELCOME: You are Player %d\n", slot);
                }
                else if (!strcmp(start, "WAIT")) {
                    printf("[SERVER] WAIT: Waiting for opponent...\n");
                }
                else if (!strncmp(start, "START_ROUND:", 12)) {
                    int r = atoi(start + 12);
                    pthread_mutex_lock(&round_mtx);
                    current_round = r;
                    round_ready = 1;
                    pthread_mutex_unlock(&round_mtx);
                    pthread_cond_signal(&round_cv);
                    printf("[SERVER] START_ROUND: %d\n", r);
                }
                else if (!strncmp(start, "RESULT:", 7)) {
                    // RESULT:<round>:<p1>:<p2>:<winner>:<s1>:<s2>
                    char tmp[256];
                    strncpy(tmp, start + 7, sizeof(tmp)-1);
                    tmp[sizeof(tmp)-1] = '\0';

                    char *tokv[8] = {0};
                    int tokc = 0;
                    char *save = NULL;
                    char *t = strtok_r(tmp, ":", &save);
                    while (t && tokc < 8) { tokv[tokc++] = t; t = strtok_r(NULL, ":", &save); }
                    if (tokc >= 6) {
                        int r = atoi(tokv[0]);
                        const char *p1 = tokv[1];
                        const char *p2 = tokv[2];
                        const char *win= tokv[3];
                        int s1 = atoi(tokv[4]);
                        int s2 = atoi(tokv[5]);
                        printf("[RESULT] Round %d | P1=%s  P2=%s  Winner=%s  Score %d:%d\n",
                               r, p1, p2, win, s1, s2);
                    } else {
                        printf("[RESULT] %s\n", start + 7);
                    }
                    /* 결과 수신 시, 다음 START_ROUND을 기다린다(서버가 곧 보냄) */
                }
                else if (!strcmp(start, "OPPONENT_LEFT")) {
                    printf("[SERVER] OPPONENT_LEFT: Opponent disconnected.\n");
                }
                else if (!strncmp(start, "ERROR:", 6)) {
                    printf("[SERVER] %s\n", start);
                }
                else {
                    printf("[SERVER] %s\n", start);
                }
            }
            start = lf + 1;
        }

        /* 남은 부분 앞으로 당김 */
        int remain = (int)(rbuf + rlen - start);
        if (remain > 0) memmove(rbuf, start, remain);
        rlen = remain;
    }

    return NULL;
}

static void error_exit(const char *msg)
{
    perror(msg);
    exit(1);
}

