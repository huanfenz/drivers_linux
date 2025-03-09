#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

#define KEY0_VALUE  0xF0
#define INVALID_KEY 0x00

int main(int argc, char *argv[]) 
{ 
    int fd = 0;
    int ret = 0;
    unsigned char databuf[1];
    int value = 0;
    
    if (argc != 2) {
        printf("need 1 param.\n");
        return -1;
    }
    
    /* 打开led驱动 */
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("open %s failed.\n", argv[1]);
        return -1;
    }

    /* 循环读取 */
    while (1) {
        read(fd, &value, sizeof(value));
        if (value == KEY0_VALUE) {
            printf("key0 press, value = %d\n", value);
        }
    }

    close(fd);
    return 0;
}
