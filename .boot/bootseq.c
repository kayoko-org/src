#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <zstd.h>

void show_xai_help() {
    printf("  1  Boot Advanced eXecutive Interactive\n");
    printf("  2  Start socat server\n");
    printf("  3  Show help menu\n");
    printf("  4  Terminate boot sequence\n");
    printf("\n  Type the number for the menu item and press Enter.\n");
}

// Helper to simulate baud rate and line-feed latency
void slow_print(const char* str, int global_line_count, int total_expected_lines) {
    while (*str) {
        putchar(*str);
        fflush(stdout);

        if (*str == '\n') {
            // --- Line Feed Latency ---
            // Calculate base delay: ramps up speed then slows down at the end
            int base_delay = (global_line_count < (total_expected_lines * 0.75)) ? 
                             global_line_count * 5000 : (total_expected_lines - global_line_count) * 7000;
            
            int jitter = rand() % 400000;

            // The "Big Stall" simulation
            if (global_line_count > 80 && global_line_count < 85) {
                jitter += 2000000; 
            }

            if (base_delay < 10000) base_delay = 10000;
            usleep(base_delay + jitter);
        } else {
            // --- Character Baud Latency ---
            // 2-5ms per character for that classic "modem" feel
            usleep(5000 + (rand() % 4000));
        }
        str++;
    }
}

void run_boot_sequence() {
    char filename[256];
    void* src_buffer = NULL;
    void* dst_buffer = NULL;
    size_t src_size, dst_size;
    FILE* fp;
    
    int global_line_count = 0;
    int total_expected_lines = 110;

    for (int i = 1; i <= 5; i++) {
        // --- Added Newline Logic for Stages 1 and 2 ---
        if (i == 2 || i == 3) {
            printf("\n\n");
            fflush(stdout);
        }

        snprintf(filename, sizeof(filename), "./boot/stage%d.zst", i);
        fp = fopen(filename, "rb");
        if (!fp) {
            printf("\n! ! Missing device: %d ! !\n", i);
            continue;
        }

        // ... [File reading and ZSTD setup remains the same] ...
        fseek(fp, 0, SEEK_END);
        src_size = ftell(fp);
        rewind(fp);
        src_buffer = malloc(src_size);
        fread(src_buffer, 1, src_size, fp);
        fclose(fp);

        unsigned long long const total_dst_size = ZSTD_getFrameContentSize(src_buffer, src_size);
        dst_size = (size_t)total_dst_size;
        dst_buffer = malloc(dst_size + 1);

        size_t const d_size = ZSTD_decompress(dst_buffer, dst_size, src_buffer, src_size);
        if (!ZSTD_isError(d_size)) {
            ((char*)dst_buffer)[d_size] = '\0';
            
            char* ptr = (char*)dst_buffer;
            while (*ptr) {
                putchar(*ptr);
                fflush(stdout);

                if (*ptr == '\n') {
                    global_line_count++;
                    // Standard line latency logic
                    int base_delay = (global_line_count < (total_expected_lines * 0.75)) ? 
                                     global_line_count * 5000 : (total_expected_lines - global_line_count) * 7000;
                    usleep(base_delay + (rand() % 400000));
                } else {
                    // --- Dynamic Baud Rate ---
                    // Stage 4 gets a much faster "High-Speed" baud rate
                    int char_delay = (i == 4) ? (500 + (rand() % 500)) : (4000 + (rand() % 6000));
                    usleep(char_delay);
                }
                ptr++;
            }
        }

        free(src_buffer);
        free(dst_buffer);

        // --- Stage Transition Latency ---
        if (i == 3) {
            // Massive delay between Stage 3 and 4
            fflush(stdout);
            usleep(8000000); // 8 second stall
        } else {
            usleep(1500000); // Standard 1.5s seek
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN);

    char input[64];
    char socat_cmd[512];
    srand(time(NULL));

    if (argc > 1 && strcmp(argv[1], "--auto-boot") == 0) {
        run_boot_sequence();
    }

    while (1) {
        printf("===> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "3") == 0) {
            show_xai_help();
        } else if (strcmp(input, "1") == 0) {
            run_boot_sequence();
        } else if (strcmp(input, "2") == 0) {
            snprintf(socat_cmd, sizeof(socat_cmd), 
                     "socat TCP-LISTEN:8080,reuseaddr,fork EXEC:\"%s --auto-boot\",pty,echo=1,stderr,setsid,sigint,sane", 
                     argv[0]);
            system(socat_cmd);
        } else if (strcmp(input, "4") == 0) {
            break;
        } else if (strlen(input) > 0) {
            printf("! ! '%s' is not a valid option ! !\n", input);
            return 1;
        }
    }
    return 0;
}
