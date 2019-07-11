#include <stdio.h>

const char *uncompress(const char *s);

int main()
{
    char buf[16384];

    while(fgets(buf, sizeof(buf), stdin)) {
	puts(uncompress(buf));
    }
    return 0;
}
