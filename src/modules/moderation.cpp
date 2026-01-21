#include "modules/moderation.hpp"
#include "database.hpp"
#include "utils/string_utils.hpp"
#include "utils/thread_pool.hpp"
#include "utils/common.hpp"
#include <regex>
#include <iostream>

namespace bot {

ModerationModule::ModerationModule(dpp::cluster& bot) : bot_(bot) {}

std::vector<dpp::slashcommand> ModerationModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    // /warn command
    commands.push_back(
        dpp::slashcommand("warn", "Issue a warning to a user", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to warn", true))
            .add_option(dpp::command_option(dpp::co_string, "reason", "Reason for the warning", false))
            .set_default_permissions(dpp::p_moderate_members)
    );

    // /warnings command
    commands.push_back(
        dpp::slashcommand("warnings", "View warnings for a user", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to check", false))
            .set_default_permissions(dpp::p_moderate_members)
    );

    // /clearwarnings command
    commands.push_back(
        dpp::slashcommand("clearwarnings", "Clear warnings for a user", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to clear warnings for", true))
            .add_option(dpp::command_option(dpp::co_integer, "amount", "Number of warnings to clear (default: all)", false))
            .set_default_permissions(dpp::p_moderate_members)
    );

    // /mute command
    commands.push_back(
        dpp::slashcommand("mute", "Timeout a user", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to mute", true))
            .add_option(dpp::command_option(dpp::co_string, "duration", "Duration (e.g., 10m, 1h, 1d)", true))
            .add_option(dpp::command_option(dpp::co_string, "reason", "Reason for the mute", false))
            .set_default_permissions(dpp::p_moderate_members)
    );

    // /unmute command
    commands.push_back(
        dpp::slashcommand("unmute", "Remove timeout from a user", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to unmute", true))
            .set_default_permissions(dpp::p_moderate_members)
    );

    // /kick command
    commands.push_back(
        dpp::slashcommand("kick", "Kick a user from the server", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to kick", true))
            .add_option(dpp::command_option(dpp::co_string, "reason", "Reason for the kick", false))
            .set_default_permissions(dpp::p_kick_members)
    );

    // /ban command
    commands.push_back(
        dpp::slashcommand("ban", "Ban a user from the server", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_user, "user", "The user to ban", true))
            .add_option(dpp::command_option(dpp::co_string, "reason", "Reason for the ban", false))
            .add_option(dpp::command_option(dpp::co_integer, "delete_days", "Days of messages to delete (0-7)", false)
                .set_min_value(0).set_max_value(7))
            .set_default_permissions(dpp::p_ban_members)
    );

    // /unban command
    commands.push_back(
        dpp::slashcommand("unban", "Unban a user from the server", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "user_id", "The user ID to unban", true))
            .set_default_permissions(dpp::p_ban_members)
    );

    // /automod command with subcommands
    dpp::slashcommand automod("automod", "Configure auto-moderation", bot_.me.id);
    automod.set_default_permissions(dpp::p_manage_guild);

    // Spam subcommand
    automod.add_option(
        dpp::command_option(dpp::co_sub_command, "spam", "Configure spam detection")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable spam detection", true))
            .add_option(dpp::command_option(dpp::co_integer, "threshold", "Messages per 5 seconds", false)
                .set_min_value(2).set_max_value(20))
            .add_option(dpp::command_option(dpp::co_string, "action", "Action to take", false)
                .add_choice(dpp::command_option_choice("Warn", std::string("warn")))
                .add_choice(dpp::command_option_choice("Mute", std::string("mute")))
                .add_choice(dpp::command_option_choice("Kick", std::string("kick")))
                .add_choice(dpp::command_option_choice("Ban", std::string("ban"))))
    );

    // Words subcommand
    automod.add_option(
        dpp::command_option(dpp::co_sub_command, "words", "Manage filtered words")
            .add_option(dpp::command_option(dpp::co_string, "action", "Add, remove, or list", true)
                .add_choice(dpp::command_option_choice("Add", std::string("add")))
                .add_choice(dpp::command_option_choice("Remove", std::string("remove")))
                .add_choice(dpp::command_option_choice("List", std::string("list"))))
            .add_option(dpp::command_option(dpp::co_string, "word", "The word to add/remove", false))
    );

    // Links subcommand
    automod.add_option(
        dpp::command_option(dpp::co_sub_command, "links", "Configure link filtering")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable link filtering", true))
    );

    // Mentions subcommand
    automod.add_option(
        dpp::command_option(dpp::co_sub_command, "mentions", "Configure mention spam detection")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable mention spam detection", true))
            .add_option(dpp::command_option(dpp::co_integer, "threshold", "Maximum mentions per message", false)
                .set_min_value(2).set_max_value(50))
    );

    // Whitelist subcommand
    automod.add_option(
        dpp::command_option(dpp::co_sub_command, "whitelist", "Manage automod whitelist")
            .add_option(dpp::command_option(dpp::co_string, "action", "Add or remove", true)
                .add_choice(dpp::command_option_choice("Add", std::string("add")))
                .add_choice(dpp::command_option_choice("Remove", std::string("remove"))))
            .add_option(dpp::command_option(dpp::co_mentionable, "target", "Channel, role, or user to whitelist", true))
    );

    commands.push_back(automod);

    return commands;
}

void ModerationModule::handle_command(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    if (cmd == "warn") {
        cmd_warn(event);
    } else if (cmd == "warnings") {
        cmd_warnings(event);
    } else if (cmd == "clearwarnings") {
        cmd_clearwarnings(event);
    } else if (cmd == "mute") {
        cmd_mute(event);
    } else if (cmd == "unmute") {
        cmd_unmute(event);
    } else if (cmd == "kick") {
        cmd_kick(event);
    } else if (cmd == "ban") {
        cmd_ban(event);
    } else if (cmd == "unban") {
        cmd_unban(event);
    } else if (cmd == "automod") {
        cmd_automod(event);
    }
}

void ModerationModule::handle_message(const dpp::message_create_t& event) {
    if (event.msg.author.is_bot() || !event.msg.guild_id) {
        return;
    }

    auto settings = get_database().get_moderation_settings(event.msg.guild_id);
    if (!settings) {
        return;
    }

    // Check if user/channel/role is whitelisted
    if (get_database().is_whitelisted(event.msg.guild_id, event.msg.author.id, "user") ||
        get_database().is_whitelisted(event.msg.guild_id, event.msg.channel_id, "channel")) {
        return;
    }

    // Run checks
    if (settings->anti_spam_enabled && check_spam(event)) {
        take_automod_action(event.msg.guild_id, event.msg.author.id, settings->spam_action, "Spam detection");
        bot_.message_delete(event.msg.id, event.msg.channel_id);
        return;
    }

    if (check_filtered_words(event)) {
        take_automod_action(event.msg.guild_id, event.msg.author.id, "warn", "Filtered word");
        bot_.message_delete(event.msg.id, event.msg.channel_id);
        return;
    }

    if (settings->anti_links_enabled && check_links(event)) {
        take_automod_action(event.msg.guild_id, event.msg.author.id, "warn", "Links not allowed");
        bot_.message_delete(event.msg.id, event.msg.channel_id);
        return;
    }

    if (settings->anti_mentions_enabled && check_mentions(event)) {
        take_automod_action(event.msg.guild_id, event.msg.author.id, settings->spam_action, "Mention spam");
        bot_.message_delete(event.msg.id, event.msg.channel_id);
        return;
    }
}

bool ModerationModule::check_spam(const dpp::message_create_t& event) {
    auto settings = get_database().get_moderation_settings(event.msg.guild_id);
    if (!settings || !settings->anti_spam_enabled) {
        return false;
    }

    std::lock_guard<std::mutex> lock(spam_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto& timestamps = message_timestamps_[event.msg.author.id];

    // Remove old timestamps (older than 5 seconds)
    timestamps.erase(
        std::remove_if(timestamps.begin(), timestamps.end(),
            [&now](const auto& ts) {
                return std::chrono::duration_cast<std::chrono::seconds>(now - ts).count() > 5;
            }),
        timestamps.end()
    );

    timestamps.push_back(now);

    return static_cast<int>(timestamps.size()) >= settings->spam_threshold;
}

bool ModerationModule::check_filtered_words(const dpp::message_create_t& event) {
    auto words = get_database().get_filtered_words(event.msg.guild_id);
    if (words.empty()) {
        return false;
    }

    std::string content = string_utils::to_lower(event.msg.content);
    for (const auto& word : words) {
        if (string_utils::contains_word(content, word)) {
            return true;
        }
    }

    return false;
}

bool ModerationModule::check_links(const dpp::message_create_t& event) {
    std::regex link_pattern("https?://[^\\s]+", std::regex::icase);
    return std::regex_search(event.msg.content, link_pattern);
}

bool ModerationModule::check_mentions(const dpp::message_create_t& event) {
    auto settings = get_database().get_moderation_settings(event.msg.guild_id);
    if (!settings || !settings->anti_mentions_enabled) {
        return false;
    }

    size_t mention_count = event.msg.mentions.size();
    return static_cast<int>(mention_count) >= settings->mention_threshold;
}

void ModerationModule::take_automod_action(dpp::snowflake guild_id, dpp::snowflake user_id,
                                           const std::string& action, const std::string& reason) {
    if (action == "warn") {
        warn_user(guild_id, user_id, bot_.me.id, "[AutoMod] " + reason);
    } else if (action == "mute") {
        mute_user(guild_id, user_id, bot_.me.id, std::chrono::seconds(600), "[AutoMod] " + reason);
    } else if (action == "kick") {
        kick_user(guild_id, user_id, bot_.me.id, "[AutoMod] " + reason);
    } else if (action == "ban") {
        ban_user(guild_id, user_id, bot_.me.id, "[AutoMod] " + reason, 1);
    }
}

void ModerationModule::warn_user(dpp::snowflake guild_id, dpp::snowflake user_id,
                                  dpp::snowflake mod_id, const std::string& reason) {
    Database::Warning warning;
    warning.guild_id = guild_id;
    warning.user_id = user_id;
    warning.moderator_id = mod_id;
    warning.reason = reason;
    warning.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    get_database().add_warning(warning);
    log_mod_action(guild_id, "warn", user_id, mod_id, reason);
}

void ModerationModule::mute_user(dpp::snowflake guild_id, dpp::snowflake user_id,
                                  dpp::snowflake mod_id, std::chrono::seconds duration,
                                  const std::string& reason) {
    // Use Discord's timeout feature
    auto timeout_until = std::chrono::system_clock::now() + duration;

    bot_.guild_member_timeout(guild_id, user_id,
        std::chrono::duration_cast<std::chrono::seconds>(timeout_until.time_since_epoch()).count(),
        [this, guild_id, user_id, mod_id, reason, duration](const dpp::confirmation_callback_t& callback) {
            if (!callback.is_error()) {
                // Record in database
                Database::Mute mute;
                mute.guild_id = guild_id;
                mute.user_id = user_id;
                mute.moderator_id = mod_id;
                mute.reason = reason;
                mute.start_time = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                mute.end_time = mute.start_time + duration.count();
                mute.active = true;

                get_database().add_mute(mute);
                log_mod_action(guild_id, "mute", user_id, mod_id, reason + " (Duration: " + format_duration(duration) + ")");
            }
        }
    );
}

void ModerationModule::unmute_user(dpp::snowflake guild_id, dpp::snowflake user_id) {
    // Remove Discord timeout
    bot_.guild_member_timeout(guild_id, user_id, 0,
        [this, guild_id, user_id](const dpp::confirmation_callback_t& callback) {
            if (!callback.is_error()) {
                get_database().deactivate_mute(guild_id, user_id);
            }
        }
    );
}

void ModerationModule::kick_user(dpp::snowflake guild_id, dpp::snowflake user_id,
                                  dpp::snowflake mod_id, const std::string& reason) {
    bot_.guild_member_kick(guild_id, user_id,
        [this, guild_id, user_id, mod_id, reason](const dpp::confirmation_callback_t& callback) {
            if (!callback.is_error()) {
                log_mod_action(guild_id, "kick", user_id, mod_id, reason);
            }
        }
    );
}

void ModerationModule::ban_user(dpp::snowflake guild_id, dpp::snowflake user_id,
                                 dpp::snowflake mod_id, const std::string& reason, uint32_t delete_days) {
    bot_.guild_ban_add(guild_id, user_id, delete_days,
        [this, guild_id, user_id, mod_id, reason](const dpp::confirmation_callback_t& callback) {
            if (!callback.is_error()) {
                log_mod_action(guild_id, "ban", user_id, mod_id, reason);
            }
        }
    );
}

void ModerationModule::unban_user(dpp::snowflake guild_id, dpp::snowflake user_id) {
    bot_.guild_ban_delete(guild_id, user_id);
}

void ModerationModule::check_expired_mutes() {
    auto expired = get_database().get_expired_mutes();
    for (const auto& mute : expired) {
        unmute_user(mute.guild_id, mute.user_id);
    }
}

void ModerationModule::start_mute_checker() {
    // This would typically be called from a timer, checking every minute
    get_thread_pool().enqueue([this]() {
        check_expired_mutes();
    });
}

void ModerationModule::log_mod_action(dpp::snowflake guild_id, const std::string& action,
                                       dpp::snowflake user_id, dpp::snowflake mod_id,
                                       const std::string& reason) {
    auto settings = get_database().get_moderation_settings(guild_id);
    if (!settings || settings->mod_log_channel_id == 0) {
        return;
    }

    std::string action_upper = string_utils::to_upper(action);
    uint32_t color = 0xff0000;  // Red by default

    if (action == "warn") color = 0xffff00;
    else if (action == "mute") color = 0xffa500;
    else if (action == "kick") color = 0xff6600;
    else if (action == "ban") color = 0xff0000;

    dpp::embed embed;
    embed.set_title("🔨 " + action_upper)
         .set_color(color)
         .add_field("User", "<@" + std::to_string(user_id) + ">", true)
         .add_field("Moderator", "<@" + std::to_string(mod_id) + ">", true)
         .add_field("Reason", reason.empty() ? "No reason provided" : reason, false)
         .set_timestamp(time(nullptr));

    bot_.message_create(dpp::message(settings->mod_log_channel_id, "").add_embed(embed));
}

// Command handlers

void ModerationModule::cmd_warn(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
    std::string reason = "No reason provided";

    auto reason_param = event.get_parameter("reason");
    if (std::holds_alternative<std::string>(reason_param)) {
        reason = std::get<std::string>(reason_param);
    }

    warn_user(event.command.guild_id, user_id, event.command.get_issuing_user().id, reason);

    int count = get_database().get_warning_count(event.command.guild_id, user_id);

    dpp::embed embed;
    embed.set_title("⚠️ Warning Issued")
         .set_color(0xffff00)
         .add_field("User", "<@" + std::to_string(user_id) + ">", true)
         .add_field("Total Warnings", std::to_string(count), true)
         .add_field("Reason", reason, false);

    event.reply(dpp::message().add_embed(embed));
}

void ModerationModule::cmd_warnings(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = event.command.get_issuing_user().id;

    auto user_param = event.get_parameter("user");
    if (std::holds_alternative<dpp::snowflake>(user_param)) {
        user_id = std::get<dpp::snowflake>(user_param);
    }

    auto warnings = get_database().get_warnings(event.command.guild_id, user_id);

    dpp::embed embed;
    embed.set_title("⚠️ Warnings for User")
         .set_color(0xffff00);

    if (warnings.empty()) {
        embed.set_description("<@" + std::to_string(user_id) + "> has no warnings.");
    } else {
        std::string desc;
        int count = 0;
        for (const auto& w : warnings) {
            if (++count > 10) {
                desc += "\n*... and " + std::to_string(warnings.size() - 10) + " more*";
                break;
            }
            desc += "**#" + std::to_string(w.id) + "** - " + w.reason + "\n";
            desc += "  By <@" + std::to_string(w.moderator_id) + "> • <t:" + std::to_string(w.timestamp) + ":R>\n\n";
        }
        embed.set_description(desc);
        embed.set_footer(dpp::embed_footer().set_text("Total: " + std::to_string(warnings.size()) + " warnings"));
    }

    event.reply(dpp::message().add_embed(embed));
}

void ModerationModule::cmd_clearwarnings(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
    int amount = -1;

    auto amount_param = event.get_parameter("amount");
    if (std::holds_alternative<int64_t>(amount_param)) {
        amount = static_cast<int>(std::get<int64_t>(amount_param));
    }

    get_database().clear_warnings(event.command.guild_id, user_id, amount);

    std::string msg = amount < 0 ? "all warnings" : std::to_string(amount) + " warning(s)";
    event.reply(success_embed("Warnings Cleared", "Cleared " + msg + " for <@" + std::to_string(user_id) + ">"));
}

void ModerationModule::cmd_mute(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
    std::string duration_str = std::get<std::string>(event.get_parameter("duration"));
    std::string reason = "No reason provided";

    auto reason_param = event.get_parameter("reason");
    if (std::holds_alternative<std::string>(reason_param)) {
        reason = std::get<std::string>(reason_param);
    }

    auto duration = parse_duration(duration_str);
    if (!duration) {
        event.reply(error_embed("Invalid Duration", "Could not parse duration: " + duration_str));
        return;
    }

    // Discord timeout limit is 28 days
    if (duration->count() > 28 * 24 * 3600) {
        event.reply(error_embed("Duration Too Long", "Maximum timeout duration is 28 days."));
        return;
    }

    mute_user(event.command.guild_id, user_id, event.command.get_issuing_user().id, *duration, reason);

    dpp::embed embed;
    embed.set_title("🔇 User Muted")
         .set_color(0xffa500)
         .add_field("User", "<@" + std::to_string(user_id) + ">", true)
         .add_field("Duration", format_duration(*duration), true)
         .add_field("Reason", reason, false);

    event.reply(dpp::message().add_embed(embed));
}

void ModerationModule::cmd_unmute(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));

    unmute_user(event.command.guild_id, user_id);

    event.reply(success_embed("User Unmuted", "Removed timeout from <@" + std::to_string(user_id) + ">"));
}

void ModerationModule::cmd_kick(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
    std::string reason = "No reason provided";

    auto reason_param = event.get_parameter("reason");
    if (std::holds_alternative<std::string>(reason_param)) {
        reason = std::get<std::string>(reason_param);
    }

    kick_user(event.command.guild_id, user_id, event.command.get_issuing_user().id, reason);

    dpp::embed embed;
    embed.set_title("👢 User Kicked")
         .set_color(0xff6600)
         .add_field("User", "<@" + std::to_string(user_id) + ">", true)
         .add_field("Reason", reason, false);

    event.reply(dpp::message().add_embed(embed));
}

void ModerationModule::cmd_ban(const dpp::slashcommand_t& event) {
    dpp::snowflake user_id = std::get<dpp::snowflake>(event.get_parameter("user"));
    std::string reason = "No reason provided";
    int delete_days = 0;

    auto reason_param = event.get_parameter("reason");
    if (std::holds_alternative<std::string>(reason_param)) {
        reason = std::get<std::string>(reason_param);
    }

    auto delete_param = event.get_parameter("delete_days");
    if (std::holds_alternative<int64_t>(delete_param)) {
        delete_days = static_cast<int>(std::get<int64_t>(delete_param));
    }

    ban_user(event.command.guild_id, user_id, event.command.get_issuing_user().id, reason, delete_days);

    dpp::embed embed;
    embed.set_title("🔨 User Banned")
         .set_color(0xff0000)
         .add_field("User", "<@" + std::to_string(user_id) + ">", true)
         .add_field("Reason", reason, false);

    event.reply(dpp::message().add_embed(embed));
}

void ModerationModule::cmd_unban(const dpp::slashcommand_t& event) {
    std::string user_id_str = std::get<std::string>(event.get_parameter("user_id"));

    dpp::snowflake user_id;
    try {
        user_id = std::stoull(user_id_str);
    } catch (...) {
        event.reply(error_embed("Invalid ID", "Please provide a valid user ID."));
        return;
    }

    unban_user(event.command.guild_id, user_id);

    event.reply(success_embed("User Unbanned", "Unbanned user with ID: " + user_id_str));
}

void ModerationModule::cmd_automod(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    // Get or create settings
    auto settings = get_database().get_moderation_settings(event.command.guild_id);
    Database::ModerationSettings mod_settings;
    if (settings) {
        mod_settings = *settings;
    } else {
        mod_settings.guild_id = event.command.guild_id;
    }

    if (subcmd == "spam") {
        bool enabled = false;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                enabled = std::get<bool>(opt.value);
            } else if (opt.name == "threshold" && std::holds_alternative<int64_t>(opt.value)) {
                mod_settings.spam_threshold = static_cast<int>(std::get<int64_t>(opt.value));
            } else if (opt.name == "action" && std::holds_alternative<std::string>(opt.value)) {
                mod_settings.spam_action = std::get<std::string>(opt.value);
            }
        }
        mod_settings.anti_spam_enabled = enabled;

        get_database().set_moderation_settings(mod_settings);
        event.reply(success_embed("Spam Detection Updated",
            "Spam detection " + std::string(enabled ? "enabled" : "disabled") +
            "\nThreshold: " + std::to_string(mod_settings.spam_threshold) + " messages/5s" +
            "\nAction: " + mod_settings.spam_action));

    } else if (subcmd == "words") {
        std::string action;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "action" && std::holds_alternative<std::string>(opt.value)) {
                action = std::get<std::string>(opt.value);
                break;
            }
        }

        if (action == "list") {
            auto words = get_database().get_filtered_words(event.command.guild_id);
            if (words.empty()) {
                event.reply(info_embed("Filtered Words", "No filtered words configured."));
            } else {
                std::string list;
                for (const auto& w : words) {
                    list += "`" + w + "`, ";
                }
                if (!list.empty()) list = list.substr(0, list.size() - 2);
                event.reply(info_embed("Filtered Words", list));
            }
        } else {
            std::string word;
            for (const auto& opt : subcommand.options) {
                if (opt.name == "word" && std::holds_alternative<std::string>(opt.value)) {
                    word = std::get<std::string>(opt.value);
                }
            }

            if (word.empty()) {
                event.reply(error_embed("Error", "Please provide a word."));
                return;
            }

            if (action == "add") {
                get_database().add_filtered_word(event.command.guild_id, word);
                event.reply(success_embed("Word Added", "Added `" + word + "` to filter."));
            } else if (action == "remove") {
                get_database().remove_filtered_word(event.command.guild_id, word);
                event.reply(success_embed("Word Removed", "Removed `" + word + "` from filter."));
            }
        }

    } else if (subcmd == "links") {
        bool enabled = false;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                enabled = std::get<bool>(opt.value);
                break;
            }
        }
        mod_settings.anti_links_enabled = enabled;
        get_database().set_moderation_settings(mod_settings);
        event.reply(success_embed("Link Filter Updated",
            "Link filtering " + std::string(enabled ? "enabled" : "disabled")));

    } else if (subcmd == "mentions") {
        bool enabled = false;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                enabled = std::get<bool>(opt.value);
            } else if (opt.name == "threshold" && std::holds_alternative<int64_t>(opt.value)) {
                mod_settings.mention_threshold = static_cast<int>(std::get<int64_t>(opt.value));
            }
        }
        mod_settings.anti_mentions_enabled = enabled;

        get_database().set_moderation_settings(mod_settings);
        event.reply(success_embed("Mention Spam Detection Updated",
            "Mention spam detection " + std::string(enabled ? "enabled" : "disabled") +
            "\nThreshold: " + std::to_string(mod_settings.mention_threshold) + " mentions"));

    } else if (subcmd == "whitelist") {
        std::string action;
        dpp::snowflake target_id = 0;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "action" && std::holds_alternative<std::string>(opt.value)) {
                action = std::get<std::string>(opt.value);
            } else if (opt.name == "target" && std::holds_alternative<dpp::snowflake>(opt.value)) {
                target_id = std::get<dpp::snowflake>(opt.value);
            }
        }

        // Determine type (we'll use "user" as default since we can't easily distinguish)
        std::string type = "user";

        if (action == "add") {
            get_database().add_whitelist(event.command.guild_id, target_id, type);
            event.reply(success_embed("Whitelist Updated", "Added <@" + std::to_string(target_id) + "> to whitelist."));
        } else if (action == "remove") {
            get_database().remove_whitelist(event.command.guild_id, target_id, type);
            event.reply(success_embed("Whitelist Updated", "Removed <@" + std::to_string(target_id) + "> from whitelist."));
        }
    }
}

} // namespace bot
