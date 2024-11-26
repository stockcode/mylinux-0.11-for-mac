#include<klib.h>
volatile void panic(const char* s)
{
    disp_str(s);
    for(;;);
}