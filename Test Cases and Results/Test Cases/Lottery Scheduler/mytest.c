#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>


int main()
  {
        int pid;
        printf("1");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","1","10000","100000","60",NULL);
        }
        printf("2");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","2","10000","100000","50",NULL );
        }
        printf("3");
        pid = fork();
        if (pid == 0) {
            execl("long3","long3","3","10000","100000","35",NULL);
        }
        printf("4");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","4","10000","1000000","70",NULL);
        }
        printf("5");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","5","10000","1000000","84",NULL);
        }
        printf("6");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","6","10000","1000000","50",NULL );
        }
        printf("7");
        pid = fork();
        if (pid == 0) {
            execl("long3","long3","7","10000","1000000","10",NULL);
        }
        printf("8");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","8","10000","1000000","30",NULL);
        }
        printf("9");
        pid = fork();
        if (pid == 0) {
            execl("long1","long1","9","10000","1000000","40",NULL);
        }
        printf("10");
        pid = fork();
        if (pid == 0) {
            execl("long2","long2","10","10000","1000000","45",NULL );
        }

  }
