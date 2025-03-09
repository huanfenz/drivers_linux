#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <sys/ioctl.h>

#define CLOSE_CMD       _IO(0xEF, 1)            // 关闭命令
#define OPEN_CMD        _IO(0xEF, 2)            // 打开命令
#define SETPERIOD_CMD   _IOW(0xEF, 3, int)      // 设置周期

void clear_input_buffer() {
    // 清空输入缓冲区
    while (getchar() != '\n' && getchar() != EOF);
}

int main(int argc, char *argv[]) 
{ 
    int fd = 0;
    int ret = 0;
    unsigned int cmd = 0;
    unsigned int arg = 0;
    unsigned char str[100] = { 0 };
    
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
        printf("Input CMD:");
        ret = scanf("%d", &cmd);
        if (ret != 1) {
            clear_input_buffer();
            printf("Invalid input\n");
            continue;
        }

        if (cmd == 1) { // 关闭
            ioctl(fd, CLOSE_CMD, &arg);
        } else if (cmd == 2) {  // 打开
            ioctl(fd, OPEN_CMD, &arg);
        } else if (cmd == 3) {  // 修改定时器时间
            printf("Input Timer period:");
            ret = scanf("%d", &arg);
            if (ret != 1) {
                clear_input_buffer();
                printf("Invalid input\n");
                continue;
            }
            ioctl(fd, SETPERIOD_CMD, &arg);
        } else {
            printf("invalid cmd %u\n", cmd);
        }
    }

    close(fd);
    return 0;
}
