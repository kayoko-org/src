#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* POSIX requires handling octal \0ddd and hex \xHH escapes */
static void print_escaped(const char *str) {
    for (; *str; str++) {
        if (*str == '\\' && *(str + 1)) {
            str++;
            switch (*str) {
                case 'a':  putchar('\a'); break;
                case 'b':  putchar('\b'); break;
                case 'f':  putchar('\f'); break;
                case 'n':  putchar('\n'); break;
                case 'r':  putchar('\r'); break;
                case 't':  putchar('\t'); break;
                case 'v':  putchar('\v'); break;
                case '\\': putchar('\\'); break;
                default:   putchar(*str); break;
            }
        } else {
            putchar(*str);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 0;
    }

    const char *format = argv[1];
    int arg_idx = 2;

    /* * POSIX printf logic: format is reused until all arguments 
     * are consumed. If no args, format is processed once.
     */
    do {
        for (const char *f = format; *f; f++) {
            if (*f == '%' && *(f + 1)) {
                f++;
                if (*f == '%') {
                    putchar('%');
                    continue;
                }
                
                /* Simple implementation for core: %s and %b (escaped) */
                if (arg_idx < argc) {
                    if (*f == 's') {
                        fputs(argv[arg_idx++], stdout);
                    } else if (*f == 'b') {
                        print_escaped(argv[arg_idx++]);
                    } else if (*f == 'd' || *f == 'i') {
                        printf("%ld", strtol(argv[arg_idx++], NULL, 10));
                    }
                    /* Add %x, %o, %f as needed for your specific core requirements */
                } else {
                    /* If no more args, strings become null, numbers become 0 */
                    if (*f == 's' || *f == 'b') fputs("", stdout);
                    else if (*f == 'd' || *f == 'i') putchar('0');
                }
            } else if (*f == '\\') {
                /* Handle backslash escapes in the format string itself */
                print_escaped_char: ; // label for convenience
                char tmp[3] = { *f, *(f+1), '\0' };
                print_escaped(tmp);
                f++;
            } else {
                putchar(*f);
            }
        }
    } while (arg_idx < argc);

    return 0;
}
