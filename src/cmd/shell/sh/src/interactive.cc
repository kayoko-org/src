#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <cctype>
#include <cstdlib>

#if defined(__sun) || defined(__sun__) || defined(__hpux) || defined(_AIX)
#include <sys/termios.h>
#endif

class InteractiveReader {
private:
    struct termios orig_termios;
    bool is_raw = false;
    std::vector<std::string> history;
    int history_index = -1;
    std::string history_file;

    void write_out(const std::string& s) {
        write(STDOUT_FILENO, s.c_str(), s.size());
    }

    void enableRawMode() {
        if (!isatty(STDIN_FILENO)) return;
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
        
        struct termios raw = orig_termios;
        
        // Input: No break, no CR to NL, no parity, no strip, no flow control
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        // Output: Preserve OPOST for newline translation
        raw.c_oflag |= OPOST; 
        // Control: 8-bit chars
        raw.c_cflag |= (CS8);
        // Local: No echo, no canonical (line) mode, no extended functions, no signals (Ctrl-C/Z)
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != -1) {
            is_raw = true;
        }
    }

    void disableRawMode() {
        if (is_raw) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            is_raw = false;
        }
    }

    bool is_whitespace(const std::string& s) {
        return s.find_first_not_of(" \t\n\v\f\r") == std::string::npos;
    }

    void load_history() {
        const char* home = getenv("HOME");
        if (home) {
            history_file = std::string(home) + "/.sh_history";
            std::ifstream hf(history_file);
            std::string line;
            while (std::getline(hf, line)) {
                if (!line.empty() && !is_whitespace(line)) {
                    history.push_back(line);
                }
            }
        }
    }

    void save_to_history(const std::string& line) {
        if (line.empty() || is_whitespace(line)) return;
        if (history.empty() || history.back() != line) {
            history.push_back(line);
            std::ofstream hf(history_file, std::ios::app);
            hf << line << "\n";
        }
        history_index = -1;
    }

    void refreshLine(const std::string& prompt, const std::string& buf, size_t pos) {
        // \r: Carriage return to start of line
        // \x1b[K: Clear from cursor to end of line
        write_out("\r\x1b[K" + prompt + buf);
        // Move cursor back to the correct logical position if we aren't at the end
        if (pos < buf.size()) {
            write_out("\x1b[" + std::to_string(buf.size() - pos) + "D");
        }
    }

    void do_ksh_tab_complete(std::string& buf, size_t& pos, const std::string& prompt) {
        // 1. Handle empty buffer or trailing space
        if (buf.empty() || (pos > 0 && isspace(static_cast<unsigned char>(buf[pos-1])))) {
            buf.insert(pos++, 1, '\t');
            return;
        }

        // 2. Identify the word being completed
        size_t start = buf.find_last_of(" \t", pos - 1);
        start = (start == std::string::npos) ? 0 : start + 1;
        std::string search_term = buf.substr(start, pos - start);

        std::vector<std::string> matches;
        bool is_command = (start == 0);

        // 3. PATH Completion (Commands)
        if (is_command && search_term.find('/') == std::string::npos) {
            char* path_env = getenv("PATH");
            if (path_env) {
                std::string path_str(path_env);
                size_t p_start = 0, p_end;
                while ((p_end = path_str.find(':', p_start)) != std::string::npos || p_start < path_str.size()) {
                    std::string dir = path_str.substr(p_start, (p_end == std::string::npos) ? std::string::npos : p_end - p_start);
                    if (dir.empty()) dir = ".";
                    DIR* d = opendir(dir.c_str());
                    if (d) {
                        struct dirent* entry;
                        while ((entry = readdir(d)) != NULL) {
                            if (std::string(entry->d_name).compare(0, search_term.size(), search_term) == 0) {
                                std::string full = dir + "/" + entry->d_name;
                                if (access(full.c_str(), X_OK) == 0) matches.push_back(entry->d_name);
                            }
                        }
                        closedir(d);
                    }
                    if (p_end == std::string::npos) break;
                    p_start = p_end + 1;
                }
            }
        }

        // 4. File Completion (Paths & local files)
        if (matches.empty()) {
            std::string dir_path = ".", file_prefix = search_term;
            size_t last_slash = search_term.find_last_of("/");
            if (last_slash != std::string::npos) {
                dir_path = search_term.substr(0, last_slash);
                if (dir_path.empty()) dir_path = "/";
                file_prefix = search_term.substr(last_slash + 1);
            }
            DIR* d = opendir(dir_path.c_str());
            if (d) {
                struct dirent* entry;
                while ((entry = readdir(d)) != NULL) {
                    std::string name = entry->d_name;
                    if (name == "." || name == "..") continue;
                    if (name.compare(0, file_prefix.size(), file_prefix) == 0) {
                        std::string full = (dir_path == "/" ? "/" : dir_path + "/") + name;
                        struct stat st;
                        if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) name += "/";
                        matches.push_back(name);
                    }
                }
                closedir(d);
            }
        }

        // 5. Clean up the matches list
        std::sort(matches.begin(), matches.end());
        matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

        if (matches.empty()) return;

        // 6. CALCULATE LONGEST COMMON PREFIX (LCP)
        std::string lcp = matches[0];
        for (size_t i = 1; i < matches.size(); ++i) {
            size_t j = 0;
            // Compare characters until a mismatch is found or we hit the end of the shortest string
            while (j < lcp.size() && j < matches[i].size() && lcp[j] == matches[i][j]) {
                j++;
            }
            lcp = lcp.substr(0, j); // Shrink the common prefix
        }

        // 7. ADVANCE THE BUFFER TO THE COMMON PREFIX
        size_t last_slash = search_term.find_last_of("/");
        size_t replace_start = start + (last_slash == std::string::npos ? 0 : last_slash + 1);
        size_t current_len = pos - replace_start;

        if (lcp.size() > current_len) {
            buf.erase(replace_start, current_len);
            buf.insert(replace_start, lcp);
            pos = replace_start + lcp.size();
        }

        // 8. DISPLAY REMAINING OPTIONS IF AMBIGUOUS
        if (matches.size() > 1) {
            write_out("\n");
            for (size_t i = 0; i < matches.size(); ++i) {
                write_out(std::to_string(i + 1) + ") " + matches[i] + "\n");
            }
            // Reprint the prompt and the updated buffer
            write_out(prompt + buf);
            // Fix the cursor position visually if it isn't at the end of the line
            if (pos < buf.size()) {
                write_out("\x1b[" + std::to_string(buf.size() - pos) + "D");
            }
        }
    }


public:
    InteractiveReader() { load_history(); }
    ~InteractiveReader() { disableRawMode(); }

    std::string readline(const std::string& prompt) {
        enableRawMode();
        std::string buf;
        size_t pos = 0;
        write_out(prompt);

        while (true) {
            char c;
            ssize_t nread = read(STDIN_FILENO, &c, 1);
            if (nread <= 0) break;

            if (c == 9) { // TAB
                do_ksh_tab_complete(buf, pos, prompt);
            } else if (c == '\r' || c == '\n') { // ENTER
                write_out("\n");
                save_to_history(buf);
                break; // Exit loop, return current buffer
            } else if (c == 127 || c == 8) { // BACKSPACE
                if (pos > 0) {
                    buf.erase(--pos, 1);
                }
            } else if (c == 4) { // CTRL-D
                if (buf.empty()) {
                    disableRawMode();
                    exit(0);
                }
            } else if (c == 3) { // CTRL-C
                buf.clear();
                pos = 0;
                write_out("^C\n" + prompt);
                continue;
            } else if (c == '\x1b') { // ESCAPE SEQUENCES
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
                if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;

                if (seq[0] == '[') {
                    if (seq[1] == 'A') { // UP
                        if (!history.empty()) {
                            if (history_index == -1) history_index = history.size() - 1;
                            else if (history_index > 0) history_index--;
                            buf = history[history_index];
                            pos = buf.size();
                        }
                    } else if (seq[1] == 'B') { // DOWN
                        if (history_index != -1 && history_index < (int)history.size() - 1) {
                            history_index++;
                            buf = history[history_index];
                            pos = buf.size();
                        } else {
                            history_index = -1;
                            buf = "";
                            pos = 0;
                        }
                    } else if (seq[1] == 'C' && pos < buf.size()) pos++; // RIGHT
                    else if (seq[1] == 'D' && pos > 0) pos--; // LEFT
                }
            } else if (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) <= 126) {
                // Printable ASCII only
                buf.insert(pos++, 1, c);
            }

            refreshLine(prompt, buf, pos);
        }

        disableRawMode();
        return buf;
    }
};

std::string ksh_readline(const std::string& prompt) {
    static InteractiveReader reader;
    return reader.readline(prompt);
}
