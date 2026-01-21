#include "modules/leveling.hpp"
#include "database.hpp"
#include "utils/string_utils.hpp"
#include "utils/thread_pool.hpp"
#include "utils/common.hpp"
#include <random>
#include <cmath>
#include <iostream>

namespace bot {

LevelingModule::LevelingModule(dpp::cluster& bot) : bot_(bot) {}

std::vector<dpp::slashcommand> LevelingModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    // /rank command
    commands.push_back(
        dpp::slashcommand("rank", "View your or another user's rank", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to check", false))
    );

    // /leaderboard command
    commands.push_back(
        dpp::slashcommand("leaderboard", "View the server leaderboard", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_integer, "page", "Page number", false)
                .set_min_value(1))
    );

    // /setxp command
    commands.push_back(
        dpp::slashcommand("setxp", "Set a user's XP", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user", true))
            .add_option(dpp::command_option(dpp::co_integer, "amount", "XP amount", true)
                .set_min_value(0))
            .set_default_permissions(dpp::p_manage_guild)
    );

    // /addxp command
    commands.push_back(
        dpp::slashcommand("addxp", "Add XP to a user", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user", true))
            .add_option(dpp::command_option(dpp::co_integer, "amount", "XP to add", true))
            .set_default_permissions(dpp::p_manage_guild)
    );

    // /resetxp command
    commands.push_back(
        dpp::slashcommand("resetxp", "Reset XP for a user or the whole server", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user (leave empty for server-wide reset)", false))
            .set_default_permissions(dpp::p_manage_guild)
    );

    // /levelconfig command with subcommands
    dpp::slashcommand levelconfig("levelconfig", "Configure leveling settings", bot_.me.id);
    levelconfig.set_default_permissions(dpp::p_manage_guild);

    levelconfig.add_option(
        dpp::command_option(dpp::co_sub_command, "enable", "Enable or disable leveling")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable leveling", true))
    );

    levelconfig.add_option(
        dpp::command_option(dpp::co_sub_command, "xp", "Set XP per message range")
            .add_option(dpp::command_option(dpp::co_integer, "min", "Minimum XP", true).set_min_value(1).set_max_value(1000))
            .add_option(dpp::command_option(dpp::co_integer, "max", "Maximum XP", true).set_min_value(1).set_max_value(1000))
    );

    levelconfig.add_option(
        dpp::command_option(dpp::co_sub_command, "cooldown", "Set XP cooldown")
            .add_option(dpp::command_option(dpp::co_integer, "seconds", "Cooldown in seconds", true).set_min_value(0).set_max_value(3600))
    );

    levelconfig.add_option(
        dpp::command_option(dpp::co_sub_command, "voice", "Configure voice XP")
            .add_option(dpp::command_option(dpp::co_integer, "xp", "XP per minute in voice", true).set_min_value(0).set_max_value(100))
            .add_option(dpp::command_option(dpp::co_integer, "min_users", "Minimum users in channel", true).set_min_value(1).set_max_value(50))
    );

    levelconfig.add_option(
        dpp::command_option(dpp::co_sub_command, "message", "Set level-up message")
            .add_option(dpp::command_option(dpp::co_string, "text", "Message (use {user}, {level})", true))
    );

    commands.push_back(levelconfig);

    // /levelreward command
    dpp::slashcommand levelreward("levelreward", "Manage level rewards", bot_.me.id);
    levelreward.set_default_permissions(dpp::p_manage_guild);

    levelreward.add_option(
        dpp::command_option(dpp::co_sub_command, "add", "Add a level reward")
            .add_option(dpp::command_option(dpp::co_integer, "level", "Level required", true).set_min_value(1))
            .add_option(dpp::command_option(dpp::co_role, "role", "Role to give", true))
    );

    levelreward.add_option(
        dpp::command_option(dpp::co_sub_command, "remove", "Remove a level reward")
            .add_option(dpp::command_option(dpp::co_integer, "level", "Level", true).set_min_value(1))
    );

    levelreward.add_option(
        dpp::command_option(dpp::co_sub_command, "list", "List all level rewards")
    );

    commands.push_back(levelreward);

    return commands;
}

void LevelingModule::handle_command(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    if (cmd == "rank") {
        cmd_rank(event);
    } else if (cmd == "leaderboard") {
        cmd_leaderboard(event);
    } else if (cmd == "setxp") {
        cmd_setxp(event);
    } else if (cmd == "addxp") {
        cmd_addxp(event);
    } else if (cmd == "resetxp") {
        cmd_resetxp(event);
    } else if (cmd == "levelconfig") {
        cmd_levelconfig(event);
    } else if (cmd == "levelreward") {
        cmd_levelreward(event);
    }
}

void LevelingModule::handle_message(const dpp::message_create_t& event) {
    if (event.msg.author.is_bot() || !event.msg.guild_id) {
        return;
    }

    // Check if leveling is enabled
    auto settings = get_database().get_leveling_settings(event.msg.guild_id);
    if (!settings || !settings->enabled) {
        return;
    }

    // Check XP blacklist
    if (get_database().is_xp_blacklisted(event.msg.guild_id, event.msg.channel_id, "channel") ||
        get_database().is_xp_blacklisted(event.msg.guild_id, event.msg.author.id, "user")) {
        return;
    }

    // Get user XP
    auto user_xp = get_database().get_user_xp(event.msg.guild_id, event.msg.author.id);
    Database::UserXP xp_data;
    if (user_xp) {
        xp_data = *user_xp;
    } else {
        xp_data.guild_id = event.msg.guild_id;
        xp_data.user_id = event.msg.author.id;
    }

    // Check cooldown
    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (now - xp_data.last_xp_time < settings->xp_cooldown) {
        return;
    }

    // Award random XP
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(settings->xp_min, settings->xp_max);
    int xp_gained = dis(gen);

    int old_level = xp_data.level;
    xp_data.xp += xp_gained;
    xp_data.total_messages++;
    xp_data.last_xp_time = now;
    xp_data.level = calculate_level(xp_data.xp);

    get_database().set_user_xp(xp_data);

    // Check for level up
    if (xp_data.level > old_level) {
        check_level_up(event.msg.guild_id, event.msg.author.id, old_level, xp_data.level, event.msg.channel_id);
    }
}

void LevelingModule::handle_voice_state(const dpp::voice_state_update_t& event) {
    if (!event.state.guild_id) return;

    std::lock_guard<std::mutex> lock(voice_mutex_);

    auto& guild_voice = voice_users_[event.state.guild_id];

    // User joined a voice channel
    if (event.state.channel_id != 0) {
        guild_voice[event.state.user_id] = std::chrono::steady_clock::now();
    } else {
        // User left voice
        guild_voice.erase(event.state.user_id);
    }
}

int LevelingModule::calculate_level(int64_t xp) {
    // Formula: xp = 100 * level^2
    // So level = sqrt(xp / 100)
    return static_cast<int>(std::sqrt(xp / 100.0));
}

int64_t LevelingModule::xp_for_level(int level) {
    return 100LL * level * level;
}

int64_t LevelingModule::xp_to_next_level(int64_t current_xp) {
    int current_level = calculate_level(current_xp);
    int64_t next_level_xp = xp_for_level(current_level + 1);
    return next_level_xp - current_xp;
}

void LevelingModule::add_xp(dpp::snowflake guild_id, dpp::snowflake user_id, int amount) {
    auto user_xp = get_database().get_user_xp(guild_id, user_id);
    Database::UserXP xp_data;
    if (user_xp) {
        xp_data = *user_xp;
    } else {
        xp_data.guild_id = guild_id;
        xp_data.user_id = user_id;
    }

    xp_data.xp = std::max(static_cast<int64_t>(0), xp_data.xp + amount);
    xp_data.level = calculate_level(xp_data.xp);

    get_database().set_user_xp(xp_data);
}

void LevelingModule::check_level_up(dpp::snowflake guild_id, dpp::snowflake user_id,
                                     int old_level, int new_level, dpp::snowflake channel_id) {
    auto settings = get_database().get_leveling_settings(guild_id);
    if (!settings) return;

    // Send level up message
    std::map<std::string, std::string> vars = {
        {"user", "<@" + std::to_string(user_id) + ">"},
        {"level", std::to_string(new_level)}
    };

    std::string message = string_utils::replace_variables(settings->level_up_message, vars);

    dpp::snowflake target_channel = settings->level_up_channel_id != 0 ? settings->level_up_channel_id : channel_id;

    dpp::embed embed;
    embed.set_title("🎉 Level Up!")
         .set_description(message)
         .set_color(0x00ff00);

    bot_.message_create(dpp::message(target_channel, "").add_embed(embed));

    // Grant level rewards
    grant_level_rewards(guild_id, user_id, new_level);
}

void LevelingModule::grant_level_rewards(dpp::snowflake guild_id, dpp::snowflake user_id, int level) {
    auto rewards = get_database().get_rewards_for_level(guild_id, level);

    for (const auto& reward : rewards) {
        bot_.guild_member_add_role(guild_id, user_id, reward.role_id);
    }
}

dpp::embed LevelingModule::create_rank_card(dpp::snowflake guild_id, dpp::snowflake user_id,
                                             const std::string& username, const std::string& avatar_url) {
    auto user_xp = get_database().get_user_xp(guild_id, user_id);
    int rank = get_database().get_user_rank(guild_id, user_id);

    int64_t xp = user_xp ? user_xp->xp : 0;
    int level = user_xp ? user_xp->level : 0;
    int64_t current_level_xp = xp_for_level(level);
    int64_t next_level_xp = xp_for_level(level + 1);
    int64_t progress_xp = xp - current_level_xp;
    int64_t needed_xp = next_level_xp - current_level_xp;

    // Progress bar
    int bar_length = 20;
    int filled = needed_xp > 0 ? static_cast<int>((progress_xp * bar_length) / needed_xp) : 0;
    std::string progress_bar;
    for (int i = 0; i < filled; i++) progress_bar += "█";
    for (int i = filled; i < bar_length; i++) progress_bar += "░";

    dpp::embed embed;
    embed.set_title("📊 Rank Card")
         .set_color(0x0099ff)
         .set_thumbnail(avatar_url)
         .add_field("User", username, true)
         .add_field("Rank", "#" + std::to_string(rank), true)
         .add_field("Level", std::to_string(level), true)
         .add_field("XP", std::to_string(xp) + " total", true)
         .add_field("Progress", progress_bar + "\n" + std::to_string(progress_xp) + " / " + std::to_string(needed_xp), false);

    if (user_xp) {
        embed.add_field("Messages", std::to_string(user_xp->total_messages), true);
        embed.add_field("Voice Time", std::to_string(user_xp->voice_minutes) + " min", true);
    }

    return embed;
}

void LevelingModule::start_voice_xp_tracker() {
    // Called periodically to grant voice XP
    get_thread_pool().enqueue([this]() {
        check_voice_channels();
    });
}

void LevelingModule::check_voice_channels() {
    std::lock_guard<std::mutex> lock(voice_mutex_);

    auto now = std::chrono::steady_clock::now();

    for (auto& [guild_id, users] : voice_users_) {
        auto settings = get_database().get_leveling_settings(guild_id);
        if (!settings || !settings->enabled || settings->voice_xp == 0) {
            continue;
        }

        // Check if enough users in voice
        if (static_cast<int>(users.size()) < settings->voice_min_users) {
            continue;
        }

        for (auto& [user_id, join_time] : users) {
            auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - join_time);
            if (duration.count() >= 1) {
                add_xp(guild_id, user_id, settings->voice_xp);
                join_time = now;  // Reset timer
            }
        }
    }
}

// Command handlers

void LevelingModule::cmd_rank(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = event.command.get_issuing_user().id;
    std::string username = event.command.get_issuing_user().username;
    std::string avatar = event.command.get_issuing_user().get_avatar_url();

    auto user_param = event.get_parameter("user");
    if (std::holds_alternative<dpp::snowflake>(user_param)) {
        user_id = std::get<dpp::snowflake>(user_param);
        // We'd need to fetch user info for other users
        // For now, just use the ID
        username = "User";
    }

    auto embed = create_rank_card(event.command.guild_id, user_id, username, avatar);
    event.reply(dpp::message().add_embed(embed));
}

void LevelingModule::cmd_leaderboard(const dpp::slashcommand_t& event) {
    int page = 1;
    auto page_param = event.get_parameter("page");
    if (std::holds_alternative<int64_t>(page_param)) {
        page = static_cast<int>(std::get<int64_t>(page_param));
    }

    int per_page = 10;
    int offset = (page - 1) * per_page;

    auto leaderboard = get_database().get_leaderboard(event.command.guild_id, per_page, offset);

    if (leaderboard.empty()) {
        event.reply(info_embed("Leaderboard", "No users found on this page."));
        return;
    }

    std::string description;
    int rank = offset;
    for (const auto& entry : leaderboard) {
        rank++;
        std::string medal;
        if (rank == 1) medal = "🥇";
        else if (rank == 2) medal = "🥈";
        else if (rank == 3) medal = "🥉";
        else medal = std::to_string(rank) + ".";

        description += medal + " <@" + std::to_string(entry.user_id) + "> - Level " +
                      std::to_string(entry.level) + " (" + std::to_string(entry.xp) + " XP)\n";
    }

    dpp::embed embed;
    embed.set_title("🏆 Leaderboard")
         .set_description(description)
         .set_color(0xffd700)
         .set_footer(dpp::embed_footer().set_text("Page " + std::to_string(page)));

    event.reply(dpp::message().add_embed(embed));
}

void LevelingModule::cmd_setxp(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
    int64_t amount = std::get<int64_t>(event.get_parameter("amount"));

    auto user_xp = get_database().get_user_xp(event.command.guild_id, user_id);
    Database::UserXP xp_data;
    if (user_xp) {
        xp_data = *user_xp;
    } else {
        xp_data.guild_id = event.command.guild_id;
        xp_data.user_id = user_id;
    }

    xp_data.xp = amount;
    xp_data.level = calculate_level(amount);
    get_database().set_user_xp(xp_data);

    event.reply(success_embed("XP Set",
        "Set <@" + std::to_string(user_id) + ">'s XP to " + std::to_string(amount) +
        " (Level " + std::to_string(xp_data.level) + ")"));
}

void LevelingModule::cmd_addxp(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
    int64_t amount = std::get<int64_t>(event.get_parameter("amount"));

    add_xp(event.command.guild_id, user_id, static_cast<int>(amount));

    auto user_xp = get_database().get_user_xp(event.command.guild_id, user_id);
    int64_t new_xp = user_xp ? user_xp->xp : amount;

    event.reply(success_embed("XP Added",
        "Added " + std::to_string(amount) + " XP to <@" + std::to_string(user_id) + ">\n" +
        "New total: " + std::to_string(new_xp) + " XP"));
}

void LevelingModule::cmd_resetxp(const dpp::slashcommand_t& event) {
    auto user_param = event.get_parameter("user");

    if (std::holds_alternative<dpp::snowflake>(user_param)) {
        dpp::snowflake user_id = std::get<dpp::snowflake>(user_param);
        get_database().reset_user_xp(event.command.guild_id, user_id);
        event.reply(success_embed("XP Reset", "Reset XP for <@" + std::to_string(user_id) + ">"));
    } else {
        get_database().reset_guild_xp(event.command.guild_id);
        event.reply(success_embed("XP Reset", "Reset XP for the entire server."));
    }
}

void LevelingModule::cmd_levelconfig(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    auto settings = get_database().get_leveling_settings(event.command.guild_id);
    Database::LevelingSettings lvl_settings;
    if (settings) {
        lvl_settings = *settings;
    } else {
        lvl_settings.guild_id = event.command.guild_id;
    }

    if (subcmd == "enable") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                lvl_settings.enabled = std::get<bool>(opt.value);
            }
        }
        get_database().set_leveling_settings(lvl_settings);
        event.reply(success_embed("Leveling Updated",
            "Leveling " + std::string(lvl_settings.enabled ? "enabled" : "disabled")));

    } else if (subcmd == "xp") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "min" && std::holds_alternative<int64_t>(opt.value)) {
                lvl_settings.xp_min = static_cast<int>(std::get<int64_t>(opt.value));
            } else if (opt.name == "max" && std::holds_alternative<int64_t>(opt.value)) {
                lvl_settings.xp_max = static_cast<int>(std::get<int64_t>(opt.value));
            }
        }
        get_database().set_leveling_settings(lvl_settings);
        event.reply(success_embed("XP Range Updated",
            "XP per message: " + std::to_string(lvl_settings.xp_min) + " - " + std::to_string(lvl_settings.xp_max)));

    } else if (subcmd == "cooldown") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "seconds" && std::holds_alternative<int64_t>(opt.value)) {
                lvl_settings.xp_cooldown = static_cast<int>(std::get<int64_t>(opt.value));
            }
        }
        get_database().set_leveling_settings(lvl_settings);
        event.reply(success_embed("Cooldown Updated",
            "XP cooldown: " + std::to_string(lvl_settings.xp_cooldown) + " seconds"));

    } else if (subcmd == "voice") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "xp" && std::holds_alternative<int64_t>(opt.value)) {
                lvl_settings.voice_xp = static_cast<int>(std::get<int64_t>(opt.value));
            } else if (opt.name == "min_users" && std::holds_alternative<int64_t>(opt.value)) {
                lvl_settings.voice_min_users = static_cast<int>(std::get<int64_t>(opt.value));
            }
        }
        get_database().set_leveling_settings(lvl_settings);
        event.reply(success_embed("Voice XP Updated",
            "Voice XP: " + std::to_string(lvl_settings.voice_xp) + " per minute\n" +
            "Minimum users: " + std::to_string(lvl_settings.voice_min_users)));

    } else if (subcmd == "message") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "text" && std::holds_alternative<std::string>(opt.value)) {
                lvl_settings.level_up_message = std::get<std::string>(opt.value);
            }
        }
        get_database().set_leveling_settings(lvl_settings);
        event.reply(success_embed("Level-Up Message Updated",
            "New message: " + lvl_settings.level_up_message));
    }
}

void LevelingModule::cmd_levelreward(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    if (subcmd == "add") {
        int level = 0;
        dpp::snowflake role_id = 0;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "level" && std::holds_alternative<int64_t>(opt.value)) {
                level = static_cast<int>(std::get<int64_t>(opt.value));
            } else if (opt.name == "role" && std::holds_alternative<dpp::snowflake>(opt.value)) {
                role_id = std::get<dpp::snowflake>(opt.value);
            }
        }

        get_database().add_level_reward(event.command.guild_id, level, role_id);
        event.reply(success_embed("Reward Added",
            "Added <@&" + std::to_string(role_id) + "> as reward for level " + std::to_string(level)));

    } else if (subcmd == "remove") {
        int level = 0;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "level" && std::holds_alternative<int64_t>(opt.value)) {
                level = static_cast<int>(std::get<int64_t>(opt.value));
            }
        }
        get_database().remove_level_reward(event.command.guild_id, level);
        event.reply(success_embed("Reward Removed", "Removed reward for level " + std::to_string(level)));

    } else if (subcmd == "list") {
        auto rewards = get_database().get_level_rewards(event.command.guild_id);

        if (rewards.empty()) {
            event.reply(info_embed("Level Rewards", "No level rewards configured."));
            return;
        }

        std::string description;
        for (const auto& r : rewards) {
            description += "**Level " + std::to_string(r.level) + "**: <@&" + std::to_string(r.role_id) + ">\n";
        }

        dpp::embed embed;
        embed.set_title("🎁 Level Rewards")
             .set_description(description)
             .set_color(0x00ff00);

        event.reply(dpp::message().add_embed(embed));
    }
}

} // namespace bot
