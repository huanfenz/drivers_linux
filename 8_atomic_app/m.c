#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

#define LEDOFF   0 
#define LEDON    1

int main(int argc, char *argv[]) 
{ 
    int fd = 0;
    int ret = 0;
    unsigned char databuf[1];
    __uint8_t cnt = 0;
    
    if (argc != 3) {
        printf("need 2 param.\n");
        return -1;
    }
    
    /* 打开led驱动 */
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        printf("open %s failed.\n", argv[1]);
        return -1;
    }

    databuf[0] = atoi(argv[2]); /* 要执行的操作：打开或关闭 */
    if ((databuf[0] != 0) && (databuf[0] !=1)) {
        printf("param out of range.\n");
        close(fd);
        return -1;
    }

    /* 向/dev/led文件写入数据 */
    ret = write(fd, databuf, 1);
    if (ret < 0) {
        printf("LED Control Failed!\n");
        close(fd);
        return -1;
    }

    /* 模拟应用占用驱动25S */
    printf("App running start!\n");
    for (cnt = 0; cnt < 5; cnt++) {
        sleep(5);
    }
    printf("App running finished!\n");

    close(fd);
    return 0;
}
