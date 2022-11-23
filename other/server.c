#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define EN_CNT 64
#define DE_CNT 64
int enin, enout, dein, deout;

pid_t enco[EN_CNT];
pid_t deco[DE_CNT];

void handler(int signum) {
        printf("\nKeyboard Interupt received, exiting\n");
        for (int i = 0; i < EN_CNT; i++) {
                kill(enco[i], signum);
                waitpid(enco[i], NULL, 0);
        }
        for (int i = 0; i < DE_CNT; i++) {
                kill(deco[i], signum);
                waitpid(deco[i], NULL, 0);
        }
        msgctl(enin, IPC_RMID, NULL);
        msgctl(enout, IPC_RMID, NULL);
        msgctl(dein, IPC_RMID, NULL);
        msgctl(deout, IPC_RMID, NULL);
        exit(0);
}

int en_cnt, de_cnt;

int main(int argc, char* argv[]) {
        if (argc == 1) {
                en_cnt = EN_CNT;
                de_cnt = DE_CNT;
        } else if (argc == 2) {
                en_cnt = de_cnt = atoi(argv[1]);
        } else if (argc == 3) {
                en_cnt = atoi(argv[1]);
                de_cnt = atoi(argv[2]);
        }
        enin = msgget(ftok("/home/sy/programs/SZ", 1), IPC_CREAT|0666);
        enout = msgget(ftok("/home/sy/programs/SZ", 2), IPC_CREAT|0666);
        dein = msgget(ftok("/home/sy/programs/SZ", 3), IPC_CREAT|0666);
        deout = msgget(ftok("/home/sy/programs/SZ", 4), IPC_CREAT|0666);
        printf("Message Queue creation finished(%d %d %d %d).\n", enin, enout, dein, deout);
        for (int i = 0; i < en_cnt; i++) {
                enco[i] = fork();
                if (enco[i] == 0) {
                        execl("enco_server", "enco_server", NULL);
                        printf("exec failed!\n");
                        exit(-1);
                } 
        }
        for (int i = i; i < de_cnt; i++) {
                deco[i] = fork();
                if (deco[i] == 0) {
                        execl("deco_server", "deco_server", NULL);
                        printf("exec failed!\n");
                        exit(-1);
                }
        }
        printf("SZ Deamon creation finished\n");
        signal(SIGINT, handler);
        printf("Waiting Ctrl-C\n");
        pause();
        return 0;
}