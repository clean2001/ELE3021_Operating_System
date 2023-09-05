#include "types.h"
#include "stat.h"
#include "user.h"
// 이거 순서 꼭 지켜줘야함.

int
main(int argc, char* argv[]) {
    if(argc <= 1) {
        exit();
    }

    // 기존에는 line 13의 "\", /"" 사이의 내용이 "user"라는 문자열이었지만, '2020005269_12300_KimSejeong'으로 변경했습니다.
    if(strcmp(argv[1], "2020005269_12300_KimSejeong") != 0) {
        exit();
    }

    // 기존에는 line 18에서 buf에 저장한 문자열이 "Hello xv6!"라는 문자열이었지만, "2020005269_12300_KimSejeong"으로 변경했습니다.
    char* buf = "2020005269_12300_KimSejeong";
    int ret_val;
    ret_val = myfunction(buf);
    printf(1, "Return value : 0x%x\n", ret_val);

    exit();
};