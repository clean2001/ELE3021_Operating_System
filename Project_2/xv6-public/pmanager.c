// [Project 2] user program
#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char* argv[]) {
    // pid, stacksize, limit 등이 10억 이하 정수이므로 buffer는 최소 10칸 이상 있어야함
    char str[512]; // buffer
    char arg0[16];
    char arg1[512], arg2[128];

    char* exec_argv[1];

    int pid, ret, idx;
    int st_size, limit;

    int my_pid = getpid(); // 현재 실행되는 pmanager의 pid

    printf(1, "[PMANAGER] Hello! Pmanager started\n");


    while(1) {
        printf(1, "Enter command: ");
        gets(str, 512);
    
        // printf(1, "pmanager arg is %s", str);

        // split str
        int i = 0;
        for(i=0; str[i] != '\0'; ++i) {
            if(str[i] == ' ' || str[i] == '\n' || str[i] == '\0') {
                arg0[i] = '\0';
                i++; break;
            }

            arg0[i] = str[i];
        }


        if(!strcmp(arg0, "list")) {
            printf(1, "[PMANAGER] proc list\n");

            // pmanager의 list를 위한 시스템 콜을 호출 -> pmlist로하자.
            pmlist();

        } else if(!strcmp(arg0, "kill")) { //pmkill
            idx = 0;
            for(; str[i] != '\0'; ++i) {
                arg1[idx++] = str[i];
                if(str[i] == ' ' || str[i] == '\n' || str[i] == '\0') {
                    arg1[i] = '\0';
                    i++; break;
                }
            }

            pid = atoi(arg1);

            // printf(1, "[PMANAGER DEBUG] %d\n", pid);

            if(pid == my_pid) { // pmanager가 pmanager 자신을 죽임
                printf(1, "[PMANAGER] I killed myself. Bye! %d\n", pid);
            }

            if(pid <= 0) {
                printf(1, "[PMANAGER] fail to kill. invalid pid\n");
                continue;
            }

            ret = kill(pid);

            if(!ret) { // 죽이는거 성공
                printf(1, "[PMANAGER] Successfully killed pid %d\n", pid);
            } else { // 죽이는 거 실패
                printf(1, "[PMANAGER] Fail to kill pid %d\n", pid);
            }

        } else if(!strcmp(arg0, "execute")) { // pmexecute
            //일단 path와 stacksize부터 받자

            // path를 arg1에 저장
            idx = 0;
            for(; str[i] != '\0'; ++i) {
                if(str[i] == ' ' || str[i] == '\n' || str[i] == '\0') {
                    arg1[idx] = '\0';
                    i++; break;
                }
                arg1[idx++] = str[i];
            }

            // stacksize를 arg2에 저장
            idx = 0;
            for(; str[i] != '\0'; ++i) {
                if(str[i] == ' ' || str[i] == '\n' || str[i] == '\0') {
                    arg2[idx] = '\0';
                    i++; break;
                }
                arg2[idx++] = str[i];
            }

            st_size = atoi(arg2);
            strcpy(exec_argv[0], arg1);            
            // exec_argv[0] = arg1; // 0번에 path가 들어있는 인자 하나짜리 배열

            if(st_size <= 0 || st_size > 100) {
                printf(1, "[PMANAGER] execute failed. invalid stack size: %d\n", st_size);
                continue;
            }

            
            if((pid = fork()) ==  0) { // 자식이면
                // ret = exec2(arg1, exec_argv, st_size);
                ret = exec2(arg1, exec_argv, st_size);

                if(ret == -1) { // 실패함
                    printf(1, "[PMANAGER] Fail to execute %s with stacksize %d\n", arg1, st_size);
                }
            }

        } else if(!strcmp(arg0, "memlim")) {
            // pid를 arg1에 저장
            idx = 0;
            for(; str[i] != '\0'; ++i) {
                if(str[i] == ' ' || str[i] == '\n' || str[i] == '\0') {
                    arg1[idx] = '\0';
                    i++; break;
                }
                arg1[idx++] = str[i];
            }

            // limit을 arg2에 저장
            idx = 0;
            for(; str[i] != '\0'; ++i) {
                if(str[i] == ' ' || str[i] == '\n' || str[i] == '\0') {
                    arg2[idx] = '\0';
                    i++; break;
                }
                arg2[idx++] = str[i];
            }

            pid = atoi(arg1);
            limit = atoi(arg2);

            ret = setmemorylimit(pid, limit);

            if(ret == -1) { // 실패함
                printf(1, "[PMANAGER] Fail to setmemorylimit (pid %d with limit %d)\n", pid, limit);
            } else {
                printf(1, "[PMANAGER] Successfully set memlim (pid %d with limit %d)\n", pid, limit);
            }

        } else if(!strcmp(arg0, "exit")) {
            printf(1, "[PMANAGER] Bye!\n");
            exit();

        } else {
            if(sizeof arg0 > 0)
                printf(1, "[PMANAGER] invalid command: %s\n", arg0);
        }

    }
}
