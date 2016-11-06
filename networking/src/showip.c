#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char * argv[]) {
    printf("argc: %d\n", argc);
    for (int i=0; i<argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }
    return 0;
}
