#include <regex>
#include <string>
#include <vector>
#include <iostream>

/**
 * Handles pax-style substitution: s/regex/replacement/flags
 * Returns the modified string.
 */
std::string apply_pax_substitution(const std::string& input, const std::vector<std::string>& s_patterns) {
    std::string result = input;

    for (const auto& full_pat : s_patterns) {
        if (full_pat.empty()) continue;

        // 1. Basic parsing: assume format is s/old/new/g
        // We need to find the delimiter (usually the char after 's')
        char delim = full_pat[1];
        size_t second_delim = full_pat.find(delim, 2);
        size_t third_delim = full_pat.find(delim, second_delim + 1);

        if (second_delim == std::string::npos || third_delim == std::string::npos) {
            std::cerr << "pax: invalid substitution pattern: " << full_pat << std::endl;
            continue;
        }

        std::string pattern_str = full_pat.substr(2, second_delim - 2);
        std::string replace_str = full_pat.substr(second_delim + 1, third_delim - second_delim - 1);
        std::string flags = full_pat.substr(third_delim + 1);

        try {
            std::regex re(pattern_str);
            
            // Handle global flag 'g'
            std::regex_constants::match_flag_type f = std::regex_constants::format_default;
            if (flags.find('g') == std::string::npos) {
                f |= std::regex_constants::format_first_only;
            }

            result = std::regex_replace(result, re, replace_str, f);
        } catch (const std::regex_error& e) {
            std::cerr << "pax: regex error: " << e.what() << std::endl;
        }
    }

    return result;
}
