#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>


int main()
  {
        int pid;
        printf("1");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","1","10000","100000",NULL);
        }
        printf("2");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","2","20000","200000",NULL );
        }
        printf("3");
        pid = fork();
        if (pid == 0) {
            execl("long3","long3","3","30000","300000",NULL);
        }
        printf("4");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","4","10000","100000",NULL);
        }
        printf("5");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","5","10000","100000",NULL);
        }
        printf("6");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","6","20000","200000",NULL );
        }
        printf("7");
        pid = fork();
        if (pid == 0) {
            execl("long3","long3","7","30000","300000",NULL);
        }
        printf("8");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","8","10000","10000",NULL);
        }
        printf("9");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","9","10000","100000",NULL);
        }
        printf("10");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","10","20000","200000",NULL );
        }

  }
