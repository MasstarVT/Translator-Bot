#include "utils/common.hpp"
#include <regex>
#include <sstream>

namespace bot {

// Language mappings
const std::map<std::string, std::string> LANGUAGE_NAMES = {
    {"english", "en"}, {"spanish", "es"}, {"french", "fr"}, {"german", "de"},
    {"italian", "it"}, {"portuguese", "pt"}, {"russian", "ru"}, {"japanese", "ja"},
    {"korean", "ko"}, {"chinese", "zh-CN"}, {"arabic", "ar"}, {"hindi", "hi"},
    {"dutch", "nl"}, {"polish", "pl"}, {"turkish", "tr"}, {"vietnamese", "vi"},
    {"thai", "th"}, {"swedish", "sv"}, {"norwegian", "no"}, {"danish", "da"},
    {"finnish", "fi"}, {"greek", "el"}, {"czech", "cs"}, {"romanian", "ro"},
    {"hungarian", "hu"}, {"hebrew", "iw"}, {"indonesian", "id"}, {"malay", "ms"},
    {"filipino", "tl"}, {"ukrainian", "uk"}, {"bengali", "bn"}, {"tamil", "ta"}
};

const std::map<std::string, std::string> LANGUAGE_FLAGS = {
    {"en", "🇬🇧"}, {"es", "🇪🇸"}, {"fr", "🇫🇷"}, {"de", "🇩🇪"},
    {"it", "🇮🇹"}, {"pt", "🇵🇹"}, {"ru", "🇷🇺"}, {"ja", "🇯🇵"},
    {"ko", "🇰🇷"}, {"zh-CN", "🇨🇳"}, {"ar", "🇸🇦"}, {"hi", "🇮🇳"},
    {"nl", "🇳🇱"}, {"pl", "🇵🇱"}, {"tr", "🇹🇷"}, {"vi", "🇻🇳"},
    {"th", "🇹🇭"}, {"sv", "🇸🇪"}, {"no", "🇳🇴"}, {"da", "🇩🇰"},
    {"fi", "🇫🇮"}, {"el", "🇬🇷"}, {"cs", "🇨🇿"}, {"ro", "🇷🇴"},
    {"hu", "🇭🇺"}, {"iw", "🇮🇱"}, {"id", "🇮🇩"}, {"ms", "🇲🇾"},
    {"tl", "🇵🇭"}, {"uk", "🇺🇦"}, {"bn", "🇧🇩"}, {"ta", "🇮🇳"}
};

std::optional<std::chrono::seconds> parse_duration(const std::string& duration_str) {
    std::regex duration_regex(R"((\d+)\s*(s|sec|second|seconds|m|min|minute|minutes|h|hr|hour|hours|d|day|days|w|week|weeks))", std::regex::icase);
    std::smatch match;

    if (!std::regex_match(duration_str, match, duration_regex)) {
        // Try simple number (assume seconds)
        try {
            int value = std::stoi(duration_str);
            return std::chrono::seconds(value);
        } catch (...) {
            return std::nullopt;
        }
    }

    int value = std::stoi(match[1].str());
    std::string unit = match[2].str();

    // Convert to lowercase
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    if (unit[0] == 's') {
        return std::chrono::seconds(value);
    } else if (unit[0] == 'm') {
        return std::chrono::seconds(value * 60);
    } else if (unit[0] == 'h') {
        return std::chrono::seconds(value * 3600);
    } else if (unit[0] == 'd') {
        return std::chrono::seconds(value * 86400);
    } else if (unit[0] == 'w') {
        return std::chrono::seconds(value * 604800);
    }

    return std::nullopt;
}

std::string format_duration(std::chrono::seconds duration) {
    auto total_seconds = duration.count();

    if (total_seconds < 60) {
        return std::to_string(total_seconds) + " second" + (total_seconds != 1 ? "s" : "");
    }

    int days = total_seconds / 86400;
    int hours = (total_seconds % 86400) / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    std::ostringstream ss;
    if (days > 0) ss << days << "d ";
    if (hours > 0) ss << hours << "h ";
    if (minutes > 0) ss << minutes << "m ";
    if (seconds > 0 && days == 0) ss << seconds << "s";

    std::string result = ss.str();
    // Trim trailing space
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

std::string snowflake_to_string(dpp::snowflake id) {
    return std::to_string(static_cast<uint64_t>(id));
}

dpp::snowflake string_to_snowflake(const std::string& str) {
    try {
        return dpp::snowflake(std::stoull(str));
    } catch (...) {
        return 0;
    }
}

bool has_permission(const dpp::guild_member& member, const dpp::guild& guild, dpp::permission perm) {
    // Owner has all permissions
    if (member.user_id == guild.owner_id) {
        return true;
    }

    // Check member permissions through roles
    dpp::permission member_perms = guild.base_permissions(member);
    if (member_perms.has(dpp::p_administrator)) {
        return true;
    }

    return member_perms.has(perm);
}

bool is_moderator(const dpp::guild_member& member, const dpp::guild& guild) {
    return has_permission(member, guild, dpp::p_kick_members) ||
           has_permission(member, guild, dpp::p_ban_members) ||
           has_permission(member, guild, dpp::p_moderate_members);
}

dpp::message error_embed(const std::string& title, const std::string& description) {
    dpp::embed embed;
    embed.set_title("❌ " + title)
         .set_description(description)
         .set_color(0xff0000);
    return dpp::message().add_embed(embed);
}

dpp::message success_embed(const std::string& title, const std::string& description) {
    dpp::embed embed;
    embed.set_title("✅ " + title)
         .set_description(description)
         .set_color(0x00ff00);
    return dpp::message().add_embed(embed);
}

dpp::message info_embed(const std::string& title, const std::string& description) {
    dpp::embed embed;
    embed.set_title("ℹ️ " + title)
         .set_description(description)
         .set_color(0x0099ff);
    return dpp::message().add_embed(embed);
}

} // namespace bot
