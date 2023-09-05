#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[]) {
    __asm__("int $128"); // 유저가 128번 인터럽트를 발생시켜보자는 의미였음.
    return 0; //xv6에서는 한 프로세스를 종료할 때 return 0이 아니라 exit을 해야 종료가 된다고 함.
    // ㅇ여기서 exit하면 안됨? -> 여기서 return 0 한게 세입자가 문단속 안하고 다니는 거임
    // user level에서 exit을 해줘야 되는데 그냥 return 0하고 나갈 수도 있음.
    // 그래서 exit을 trap.c에서 해줘야된다는 뜻인가
}