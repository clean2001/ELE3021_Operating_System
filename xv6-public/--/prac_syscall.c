#include "types.h"
#include "defs.h"

int
myfunction(char *str) {
    cprintf("%s\n", str); //콘솔에 출력
    return 0xABCDABCD;
}

// wrapper function -> 인자를 읽어서 실제 function으로 넘겨주기만 하면 된다.
int sys_myfunction(void) {
    char* str;

    // argstr -> argument를 decode해서 str 포인터를 가리키도록 한다?
    if(argstr(0, &str) < 0)
        return -1;
    
    return myfunction(str);
}