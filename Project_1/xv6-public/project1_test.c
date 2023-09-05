// #include "types.h"
// #include "stat.h"
// #include "user.h"

// #define ITER 1000000
// #define CHECKPOINT 100000
// #define CALLPOINT 10000


// // 개입 없이 mlfq 만으로 child1, child2, child3, parent process를 처리합니다
// void
// normal_mlfq_test(void) {
//     printf(1, "[TEST INTO]: normal_mlfq_test started\n");

//     int pid1 = fork();

//     // int cache = 100;
//     if(pid1>0) {
//         // cache = pid1;
//         wait();
//     }

//     if(pid1 == 0) {
//         for(int j=0; j<ITER; ++j) {
//             if(j%CHECKPOINT == 0) printf(1, "[TEST INFO] child process 1 is working\n");
//         }
//     }


//     int pid2 = fork();

//     if(pid2 == 0) {
//         for(int i=0; i<ITER; ++i) {
//             if(i%CHECKPOINT == 0) printf(1, "[TEST INFO] child process 2 is working!!!\n");
//         }
//     }

//     if(pid2>0) wait();



//     for(int i=0; i<ITER; ++i) {
//         if(i%CHECKPOINT == 0) printf(1, "[TEST INFO] Parent Process is working\n");
//     }

// }


// // child 1, 2가 i == 10000 마다 yield()를 호출하게 합니다.
// void
// yield_mlfq_test(void) {
//     printf(1, "[TEST INTO]: yield_mlfq_test started\n");

//     int pid1 = fork();

//     // int cache = 100;
//     if(pid1>0) {
//         // cache = pid1;
//         wait();
//     }

//     if(pid1 == 0) {
//         for(int j=0; j<ITER; ++j) {
//             if(j % CHECKPOINT == 0) printf(1, "[TEST INFO] child process 1 is working\n");
//             if(j%CHECKPOINT == 0) {
//                 printf(1, "[TEST INFO] child process 1 calls yield!\n");
//                 yield();
//             }
//         }
//     }


//     int pid2 = fork();

//     if(pid2 == 0) {
//         for(int j=0; j<ITER; ++j) {
//             if(j % CHECKPOINT == 0) printf(1, "[TEST INFO] child process 2 is working\n");
//             if(j%CHECKPOINT == 0) {
//                 printf(1, "[TEST INFO] child process 2 calls yield!\n");
//                 yield();
//             }
//         }
//     }

//     if(pid2>0) wait();

//     int pid3 = fork();
//     if(pid3 == 0) {
//         for(int j=0; j<ITER; ++j) {
//             if(j % CHECKPOINT == 0) printf(1, "[TEST INFO] child process 3 is working\n");
//         }
//     }



//     for(int i=0; i<ITER; ++i) {
//         if(i%CHECKPOINT == 0) printf(1, "[TEST INFO] Parent Process is working\n");
//     }
// }


// // setPriority를 호출해서
// void
// setPriority_test(void) {
//     printf(1, "[TEST INTO]: setPriority Test started\n");

//     int pid1 = fork();
    
//     int pid_of_child1 = -1;
//     if(pid1 > 0) { // 부모라면
//         pid_of_child1 = pid1;
//         wait();
//     }

//     if(pid1 == 0) { // 자식이라면

//         for(int i=0; i<ITER; ++i) {
//         if(pid_of_child1 != -1 && i % CALLPOINT == 0) {
//             setPriority(pid_of_child1, 0); // priority를 변경

//             printf(1, "[TEST INFO] child process 1 called setPriority\n");
//             }
//         }
//     }

//     int pid2 = fork();

//     if(pid2 == 0) { // 자식이라면
//         for(int i=0; i<ITER; ++i) {
//             if(i % CHECKPOINT == 0) {
//                 printf(1, "[TEST INFO] child process 2 is working\n");
//             }
//         }
//     }
    
// }

// // sche lock unlock을 테스트합니다. child 3이 10번 lock을 겁니다.
// void
// lock_test(int password) {
//     printf(1, "[TEST INTO]: lock_test started\n");

//     int pid1 = fork();

//     // int cache = 100;
//     if(pid1>0) {
//         // cache = pid1;
//         wait();
//     }

//     if(pid1 == 0) {
//         for(int j=0; j<ITER; ++j) {
//             if(j % CHECKPOINT == 0) printf(1, "[TEST INFO] child process 1 is working\n");
//             if(j%CHECKPOINT == 0) {
//                 printf(1, "[TEST INFO] child process 1 calls yield!\n");
//             }
//         }
//     }


//     int pid2 = fork();

//     if(pid2 == 0) {
//         for(int j=0; j<ITER; ++j) {
//             if(j % CHECKPOINT == 0) printf(1, "[TEST INFO] child process 2 is working\n");
//             if(j%CHECKPOINT == 0) {
//                 printf(1, "[TEST INFO] child process 2 calls yield!\n");
//             }
//         }
//     }

//     if(pid2>0) wait();

//     int pid3 = fork();
//     if(pid3 == 0) {
//         for(int j=0; j<ITER; ++j) {
//             if(j % CHECKPOINT == 0) {
//                 printf(1, "[TEST INFO] child process 3 call LOCK!!!!\n");
//                 schedulerLock(password);
//         }
//     }



//         for(int i=0; i<ITER; ++i) {
//             if(i%CHECKPOINT == 0) printf(1, "[TEST INFO] Parent Process is working\n");
//         }
//     }
// }

// int main(int argc, char* argv[]) {


//     int password = atoi(argv[1]);

//     printf(1, "[TEST INFO] Input(arg) password is %d\n", password);


//     // normal_mlfq_test();
//     // sleep(5);

//     // yield_mlfq_test();
//     // sleep(5);

//     // setPriority_test();
//     // sleep(5);

//     lock_test(password);


//     exit();
// }

