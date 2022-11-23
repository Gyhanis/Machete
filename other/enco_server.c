#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <stdint.h>
#include "../SZ/sz_api.h"

struct msg_de {
        long id;
        long size;
        double error_bound;
        double data[1021];
};

struct msg_en {
        long id;
        long size;
        long length;
        unsigned char data[8168];
};

int in, out;

void handler(int signum) {
        SZ_Finalize();
        exit(0);
}

int main() {
        long r;
        signal(SIGINT, handler);
        SZ_Init(NULL);
        in = msgget(ftok("/home/sy/programs/SZ", 1), 0);
        out = msgget(ftok("/home/sy/programs/SZ", 2), 0);
        // printf("Listening %d, writing to %d\n", in, out);
        while (1) {
                struct msg_de msg_in;
                struct msg_en msg_out;
                r = msgrcv(in, &msg_in, sizeof(msg_in), 0, 0);
                // printf("Received compression request %ld\n", msg_in.id);
                if (r == -1) {
                        printf("[EN] error on receive: %d\n", errno);
                        exit(-1);
                } 
                unsigned char *en = SZ_compress_args(SZ_DOUBLE, msg_in.data, &msg_out.size, ABS, msg_in.error_bound, 0, 0, 0, 0, 0, 0, msg_in.size);
                memcpy(&msg_out.data, en, msg_out.size);
                free(en);
                msg_out.id = msg_in.id;
                r = msgsnd(out, &msg_out, sizeof(msg_out), 0);
                // printf("Request %ld compressed %ld -> %ld\n", msg_in.id, msg_in.size * 8, msg_out.size);
        }
        return 0;
}