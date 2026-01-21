#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <regex>

namespace bot {
namespace string_utils {

// Trim whitespace
std::string trim(const std::string& str);
std::string ltrim(const std::string& str);
std::string rtrim(const std::string& str);

// Case conversion
std::string to_lower(const std::string& str);
std::string to_upper(const std::string& str);

// String splitting
std::vector<std::string> split(const std::string& str, char delimiter);
std::vector<std::string> split(const std::string& str, const std::string& delimiter);

// String joining
std::string join(const std::vector<std::string>& parts, const std::string& delimiter);

// URL encoding
std::string url_encode(const std::string& value);

// Discord text cleaning
std::string clean_text_for_detection(const std::string& text);
std::string escape_markdown(const std::string& text);
std::string strip_mentions(const std::string& text);

// Text truncation
std::string truncate(const std::string& str, size_t max_length, const std::string& suffix = "...");

// Variable replacement for templates
std::string replace_variables(const std::string& template_str, const std::map<std::string, std::string>& variables);

// Check if string contains substring (case insensitive)
bool contains_word(const std::string& text, const std::string& word);

} // namespace string_utils
} // namespace bot
