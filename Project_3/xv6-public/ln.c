#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){ // project 3에서 고침. 이제는 옵션까지 인자를 4개 받아야한다. 0->ln | 1->d
    printf(2, "Usage: ln old new\n");
    exit();
  }
  if(!strcmp(argv[1], "-h")) {
    if(link(argv[2], argv[3]) < 0)
      printf(2, "hard link %s %s: failed\n", argv[2], argv[3]);
  } else if(!strcmp(argv[1], "-s")) {
    if(symlink(argv[2], argv[3]) < 0) {
      printf(1, "symbolic link %s %s: falied\n", argv[2], argv[3]);
    }
  } else { // 옵션이 잘못됨
    printf(1, "ln faild. invalid option.\n");
  }

  exit();
}
