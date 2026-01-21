#pragma once

#include <dpp/dpp.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <optional>
#include <functional>
#include <chrono>

using json = nlohmann::json;

namespace bot {

// Language mappings
extern const std::map<std::string, std::string> LANGUAGE_NAMES;
extern const std::map<std::string, std::string> LANGUAGE_FLAGS;

// Duration parsing
std::optional<std::chrono::seconds> parse_duration(const std::string& duration_str);
std::string format_duration(std::chrono::seconds duration);

// Snowflake utilities
std::string snowflake_to_string(dpp::snowflake id);
dpp::snowflake string_to_snowflake(const std::string& str);

// Permission checking
bool has_permission(const dpp::guild_member& member, const dpp::guild& guild, dpp::permission perm);
bool is_moderator(const dpp::guild_member& member, const dpp::guild& guild);

// Error response helper
dpp::message error_embed(const std::string& title, const std::string& description);
dpp::message success_embed(const std::string& title, const std::string& description);
dpp::message info_embed(const std::string& title, const std::string& description);

} // namespace bot
