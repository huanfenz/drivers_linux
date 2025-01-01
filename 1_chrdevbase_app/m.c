/* 
./chrdevbase_app 1  // 表示从驱动里读数据
./chrdevbase_app 2  // 表示向驱动里写数据
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/chrdevbase"
static const char userdata[] = "This is user data!";

int main(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    char readbuf[100] = { 0 };
    unsigned char oper = 0;

    if (argc != 2) {    // inlucde self
        printf("need a param, 1=read, 2=write.\n");
        return -1;
    }
    
    oper = atoi(argv[1]);   // if param not number, atoi return 0
    if (oper != 1 && oper != 2) {
        printf("param out of range.\n");
        return -1;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        printf("open %s failed.\n", DEVICE_PATH);
        return -1;
    }

    if (oper == 1) {
        ret = read(fd, readbuf, sizeof(readbuf) - 1);
        if (ret < 0) {
            printf("read failed.\n");
            close(fd);
            return -1;
        }
        printf("read data: %s\n", readbuf);
    } else {
        ret = write(fd, userdata, sizeof(userdata));
        if (ret < 0) {
            printf("write failed.\n");
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}
