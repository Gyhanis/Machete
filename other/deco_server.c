#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

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
        in = msgget(ftok("/home/sy/programs/SZ", 3), IPC_CREAT|0666);
        out = msgget(ftok("/home/sy/programs/SZ", 4), IPC_CREAT|0666);
        // printf("Listening %d, writing to %d\n", in, out);
        while (1) {
                struct msg_en msg_in;
                struct msg_de msg_out;
                r = msgrcv(in, &msg_in, sizeof(msg_in), 0, 0);
                // printf("Received decompression request %ld\n", msg_in.id);
                if (r == -1) {
                        printf("[DE] error on receive: %d\n", errno);
                        exit(-1);
                }
                msg_out.size = SZ_decompress_args(SZ_DOUBLE, msg_in.data, msg_in.size, msg_out.data, 0, 0, 0, 0, msg_in.length);
                msg_out.id = msg_in.id;
                r = msgsnd(out, &msg_out, sizeof(msg_out), 0);
                // printf("Request %ld decompressed %ld -> %ld\n", msg_in.id, msg_in.size, msg_out.size * 8);
        }
        return 0;
}