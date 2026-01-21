#include "utils/string_utils.hpp"
#include <iomanip>
#include <sstream>
#include <cctype>

namespace bot {
namespace string_utils {

std::string trim(const std::string& str) {
    return ltrim(rtrim(str));
}

std::string ltrim(const std::string& str) {
    auto it = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    return std::string(it, str.end());
}

std::string rtrim(const std::string& str) {
    auto it = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    return std::string(str.begin(), it.base());
}

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::istringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    result.push_back(str.substr(start));
    return result;
}

std::string join(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += delimiter;
        result += parts[i];
    }
    return result;
}

std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

std::string clean_text_for_detection(const std::string& text) {
    // Remove Unicode emojis
    std::regex emoji_pattern(
        "[\\U0001F600-\\U0001F64F\\U0001F300-\\U0001F5FF\\U0001F680-\\U0001F6FF"
        "\\U0001F1E0-\\U0001F1FF\\U00002500-\\U00002BEF\\U00002702-\\U000027B0"
        "\\U000024C2-\\U0001F251\\U0001f926-\\U0001f937\\U00010000-\\U0010ffff"
        "\\u2640-\\u2642\\u2600-\\u2B55\\u200d\\u23cf\\u23e9\\u231a\\ufe0f\\u3030]+");

    std::string cleaned = std::regex_replace(text, emoji_pattern, "");

    // Remove Discord custom emojis
    std::regex discord_emoji("<a?:[a-zA-Z0-9_]+:[0-9]+>");
    cleaned = std::regex_replace(cleaned, discord_emoji, "");

    return cleaned.empty() ? text : cleaned;
}

std::string escape_markdown(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 2);
    for (char c : text) {
        if (c == '*' || c == '_' || c == '`' || c == '~' || c == '|' || c == '\\') {
            result += '\\';
        }
        result += c;
    }
    return result;
}

std::string strip_mentions(const std::string& text) {
    // Remove user mentions
    std::regex user_mention("<@!?[0-9]+>");
    std::string result = std::regex_replace(text, user_mention, "");

    // Remove role mentions
    std::regex role_mention("<@&[0-9]+>");
    result = std::regex_replace(result, role_mention, "");

    // Remove channel mentions
    std::regex channel_mention("<#[0-9]+>");
    result = std::regex_replace(result, channel_mention, "");

    return result;
}

std::string truncate(const std::string& str, size_t max_length, const std::string& suffix) {
    if (str.length() <= max_length) {
        return str;
    }
    if (max_length <= suffix.length()) {
        return suffix.substr(0, max_length);
    }
    return str.substr(0, max_length - suffix.length()) + suffix;
}

std::string replace_variables(const std::string& template_str, const std::map<std::string, std::string>& variables) {
    std::string result = template_str;
    for (const auto& [key, value] : variables) {
        std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    return result;
}

bool contains_word(const std::string& text, const std::string& word) {
    std::string lower_text = to_lower(text);
    std::string lower_word = to_lower(word);

    // Use word boundary matching
    std::regex word_regex("\\b" + lower_word + "\\b", std::regex::icase);
    return std::regex_search(lower_text, word_regex);
}

} // namespace string_utils
} // namespace bot
