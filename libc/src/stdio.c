#include "stdio.h"
#include "stdlib.h"
#include "string.h"

int printf(char* fmt, ...)
{
    char buff[2048];
    int bytes;
    va_list va;
    va_start(va, fmt);
    bytes = vsnprintf(buff, 2048, fmt, va);
    va_end(va);
    
    #ifdef __APPLE__
        __asm__ volatile(
            "andl $0xFFFFFFF0, %%esp \n"
            "pushl %%edx \n"
            "pushl %%ecx \n"
            "pushl $1 \n"
            "movl $4, %%eax \n"
            "pushl $retn_to_here \n"
            "int $0x80 \n"
            "retn_to_here: \n"
            "addl $12, %%esp \n" :: "c"(&buff), "d"(bytes));
    #else
        #error printf() not implemented
    #endif
    
    return bytes;
}

int vsnprintf(char* str, size_t size, const char* fmt, va_list ap)
{
    size_t i;
    str[size - 1] = 0;
    for(i = 0; *fmt && i < size - 1; fmt++) {
        if(*fmt != '%') {
            str[i++] = *fmt;
            continue;
        }
        
        fmt++;
        switch(*fmt) {
            case 0:
                str[i++] = '%';
                goto end_of_loop;
            case 's': {
                char* s = va_arg(ap, char*);
                size_t len = strlen(s);
                while(*s && i < len - 1) {
                    str[i++] = *s++;
                }
                break;
            }
            case 'c': {
                str[i++] = va_arg(ap, char);
                break;
            }
            case 'd': {
                char buff[64];
                int len;
                itoa(va_arg(ap, int), buff, 10);
                len = strlen(buff);
                if(i + len < size - 1) {
                    memcpy(str + i, buff, len);
                } else {
                    goto end_of_loop;
                }
                i += len;
                break;
            }
            case 'x': {
                char buff[64];
                int len;
                itoa(va_arg(ap, int), buff, 16);
                len = strlen(buff);
                if(i + len < size - 1) {
                    memcpy(str + i, buff, len);
                } else {
                    goto end_of_loop;
                }
                i += len;
                break;
            }
            default: {
                str[i++] = '%';
                if(i < size - 1) {
                    str[i++] = *fmt;
                }
                break;
            }
        }
    }
    end_of_loop:
    str[i] = 0;
    return i;
}