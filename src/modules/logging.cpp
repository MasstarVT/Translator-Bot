#include "modules/logging.hpp"
#include "database.hpp"
#include "utils/common.hpp"
#include "utils/string_utils.hpp"

namespace bot {

LoggingModule::LoggingModule(dpp::cluster& bot) : bot_(bot) {}

std::vector<dpp::slashcommand> LoggingModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    dpp::slashcommand logging("logging", "Configure logging settings", bot_.me.id);
    logging.set_default_permissions(dpp::p_manage_guild);

    logging.add_option(
        dpp::command_option(dpp::co_sub_command, "channel", "Set a log channel")
            .add_option(dpp::command_option(dpp::co_string, "type", "Log type", true)
                .add_choice(dpp::command_option_choice("Messages", std::string("messages")))
                .add_choice(dpp::command_option_choice("Members", std::string("members")))
                .add_choice(dpp::command_option_choice("Moderation", std::string("moderation")))
                .add_choice(dpp::command_option_choice("Voice", std::string("voice")))
                .add_choice(dpp::command_option_choice("Server", std::string("server"))))
            .add_option(dpp::command_option(dpp::co_channel, "channel", "Log channel (leave empty to disable)", false))
    );

    logging.add_option(
        dpp::command_option(dpp::co_sub_command, "enable", "Enable or disable a log type")
            .add_option(dpp::command_option(dpp::co_string, "type", "Log type", true)
                .add_choice(dpp::command_option_choice("Message Edits", std::string("message_edits")))
                .add_choice(dpp::command_option_choice("Message Deletes", std::string("message_deletes")))
                .add_choice(dpp::command_option_choice("Member Joins", std::string("member_joins")))
                .add_choice(dpp::command_option_choice("Member Leaves", std::string("member_leaves")))
                .add_choice(dpp::command_option_choice("Member Bans", std::string("member_bans")))
                .add_choice(dpp::command_option_choice("Voice State", std::string("voice_state")))
                .add_choice(dpp::command_option_choice("Role Changes", std::string("role_changes")))
                .add_choice(dpp::command_option_choice("Nickname Changes", std::string("nickname_changes"))))
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable this log type", true))
    );

    logging.add_option(
        dpp::command_option(dpp::co_sub_command, "ignore", "Add or remove from ignore list")
            .add_option(dpp::command_option(dpp::co_string, "action", "Add or remove", true)
                .add_choice(dpp::command_option_choice("Add", std::string("add")))
                .add_choice(dpp::command_option_choice("Remove", std::string("remove"))))
            .add_option(dpp::command_option(dpp::co_mentionable, "target", "Channel or user to ignore", true))
    );

    commands.push_back(logging);

    return commands;
}

void LoggingModule::handle_command(const dpp::slashcommand_t& event) {
    cmd_logging(event);
}

void LoggingModule::log_message_delete(const dpp::message_delete_t& event) {
    if (!should_log(event.guild_id, "message_deletes")) {
        return;
    }

    auto channel = get_log_channel(event.guild_id, "messages");
    if (!channel) return;

    // Try to get cached message
    auto cached = get_cached_message(event.id);

    dpp::embed embed;
    embed.set_title("Message Deleted")
         .set_color(0xff6b6b)
         .add_field("Channel", "<#" + std::to_string(event.channel_id) + ">", true)
         .set_timestamp(time(nullptr));

    if (cached) {
        embed.add_field("Author", "<@" + std::to_string(cached->author.id) + ">", true);
        if (!cached->content.empty()) {
            embed.add_field("Content", string_utils::truncate(cached->content, 1024), false);
        }
    } else {
        embed.add_field("Note", "Message content not cached", false);
    }

    send_log(*channel, embed);
}

void LoggingModule::log_message_update(const dpp::message_update_t& event) {
    if (!should_log(event.msg.guild_id, "message_edits")) {
        return;
    }

    if (event.msg.author.is_bot()) {
        return;
    }

    auto channel = get_log_channel(event.msg.guild_id, "messages");
    if (!channel) return;

    // Get cached old content
    auto cached = get_cached_message(event.msg.id);

    dpp::embed embed;
    embed.set_title("Message Edited")
         .set_color(0xffa500)
         .add_field("Author", "<@" + std::to_string(event.msg.author.id) + ">", true)
         .add_field("Channel", "<#" + std::to_string(event.msg.channel_id) + ">", true)
         .set_timestamp(time(nullptr));

    if (cached && !cached->content.empty()) {
        embed.add_field("Before", string_utils::truncate(cached->content, 1024), false);
    }

    if (!event.msg.content.empty()) {
        embed.add_field("After", string_utils::truncate(event.msg.content, 1024), false);
    }

    // Update cache
    cache_message(event.msg);

    send_log(*channel, embed);
}

void LoggingModule::log_member_join(const dpp::guild_member_add_t& event) {
    if (!should_log(event.adding_guild->id, "member_joins")) {
        return;
    }

    auto channel = get_log_channel(event.adding_guild->id, "members");
    if (!channel) return;

    dpp::embed embed;
    embed.set_title("Member Joined")
         .set_color(0x00ff00)
         .set_thumbnail(event.added.get_user() ? event.added.get_user()->get_avatar_url() : "")
         .add_field("User", "<@" + std::to_string(event.added.user_id) + ">", true)
         .add_field("Account Created", "<t:" + std::to_string(event.added.user_id.get_creation_time()) + ":R>", true)
         .set_timestamp(time(nullptr));

    send_log(*channel, embed);
}

void LoggingModule::log_member_leave(const dpp::guild_member_remove_t& event) {
    if (!should_log(event.removing_guild->id, "member_leaves")) {
        return;
    }

    auto channel = get_log_channel(event.removing_guild->id, "members");
    if (!channel) return;

    dpp::embed embed;
    embed.set_title("Member Left")
         .set_color(0xff0000)
         .set_thumbnail(event.removed.get_avatar_url())
         .add_field("User", event.removed.username + "#" + std::to_string(event.removed.discriminator), true)
         .add_field("User ID", std::to_string(event.removed.id), true)
         .set_timestamp(time(nullptr));

    send_log(*channel, embed);
}

void LoggingModule::log_member_ban(const dpp::guild_ban_add_t& event) {
    if (!should_log(event.banning_guild->id, "member_bans")) {
        return;
    }

    auto channel = get_log_channel(event.banning_guild->id, "moderation");
    if (!channel) return;

    dpp::embed embed;
    embed.set_title("Member Banned")
         .set_color(0xff0000)
         .set_thumbnail(event.banned.get_avatar_url())
         .add_field("User", event.banned.username, true)
         .add_field("User ID", std::to_string(event.banned.id), true)
         .set_timestamp(time(nullptr));

    send_log(*channel, embed);
}

void LoggingModule::log_member_unban(const dpp::guild_ban_remove_t& event) {
    if (!should_log(event.unbanning_guild->id, "member_bans")) {
        return;
    }

    auto channel = get_log_channel(event.unbanning_guild->id, "moderation");
    if (!channel) return;

    dpp::embed embed;
    embed.set_title("Member Unbanned")
         .set_color(0x00ff00)
         .set_thumbnail(event.unbanned.get_avatar_url())
         .add_field("User", event.unbanned.username, true)
         .add_field("User ID", std::to_string(event.unbanned.id), true)
         .set_timestamp(time(nullptr));

    send_log(*channel, embed);
}

void LoggingModule::log_voice_state(const dpp::voice_state_update_t& event) {
    if (!should_log(event.state.guild_id, "voice_state")) {
        return;
    }

    auto channel = get_log_channel(event.state.guild_id, "voice");
    if (!channel) return;

    // Determine the action
    std::string action;
    uint32_t color;

    if (event.state.channel_id == 0) {
        action = "Left Voice Channel";
        color = 0xff0000;
    } else {
        action = "Joined Voice Channel";
        color = 0x00ff00;
    }

    dpp::embed embed;
    embed.set_title(action)
         .set_color(color)
         .add_field("User", "<@" + std::to_string(event.state.user_id) + ">", true);

    if (event.state.channel_id != 0) {
        embed.add_field("Channel", "<#" + std::to_string(event.state.channel_id) + ">", true);
    }

    embed.set_timestamp(time(nullptr));

    send_log(*channel, embed);
}

void LoggingModule::log_custom(dpp::snowflake guild_id, const std::string& type, const dpp::embed& embed) {
    auto channel = get_log_channel(guild_id, type);
    if (!channel) return;

    send_log(*channel, embed);
}

void LoggingModule::cache_message(const dpp::message& msg) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Clean up if cache is too large
    if (message_cache_.size() >= MAX_CACHE_SIZE) {
        cleanup_cache();
    }

    message_cache_[msg.id] = msg;
}

std::optional<dpp::message> LoggingModule::get_cached_message(dpp::snowflake message_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = message_cache_.find(message_id);
    if (it != message_cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void LoggingModule::cmd_logging(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    auto settings = get_database().get_logging_settings(event.command.guild_id);
    Database::LoggingSettings log_settings;
    if (settings) {
        log_settings = *settings;
    } else {
        log_settings.guild_id = event.command.guild_id;
    }

    if (subcmd == "channel") {
        std::string type;
        dpp::snowflake channel_id = 0;

        for (const auto& opt : subcommand.options) {
            if (opt.name == "type" && std::holds_alternative<std::string>(opt.value)) {
                type = std::get<std::string>(opt.value);
            } else if (opt.name == "channel" && std::holds_alternative<dpp::snowflake>(opt.value)) {
                channel_id = std::get<dpp::snowflake>(opt.value);
            }
        }

        if (type == "messages") log_settings.message_log_channel = channel_id;
        else if (type == "members") log_settings.member_log_channel = channel_id;
        else if (type == "moderation") log_settings.mod_log_channel = channel_id;
        else if (type == "voice") log_settings.voice_log_channel = channel_id;
        else if (type == "server") log_settings.server_log_channel = channel_id;

        get_database().set_logging_settings(log_settings);

        if (channel_id == 0) {
            event.reply(success_embed("Log Channel Disabled", "Disabled " + type + " logging."));
        } else {
            event.reply(success_embed("Log Channel Set",
                "Set " + type + " log channel to <#" + std::to_string(channel_id) + ">"));
        }

    } else if (subcmd == "enable") {
        std::string type;
        bool enabled = false;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "type" && std::holds_alternative<std::string>(opt.value)) {
                type = std::get<std::string>(opt.value);
            } else if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                enabled = std::get<bool>(opt.value);
            }
        }

        if (type == "message_edits") log_settings.log_message_edits = enabled;
        else if (type == "message_deletes") log_settings.log_message_deletes = enabled;
        else if (type == "member_joins") log_settings.log_member_joins = enabled;
        else if (type == "member_leaves") log_settings.log_member_leaves = enabled;
        else if (type == "member_bans") log_settings.log_member_bans = enabled;
        else if (type == "voice_state") log_settings.log_voice_state = enabled;
        else if (type == "role_changes") log_settings.log_role_changes = enabled;
        else if (type == "nickname_changes") log_settings.log_nickname_changes = enabled;

        get_database().set_logging_settings(log_settings);
        event.reply(success_embed("Logging Updated",
            type + " logging " + std::string(enabled ? "enabled" : "disabled")));

    } else if (subcmd == "ignore") {
        std::string action;
        dpp::snowflake target_id = 0;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "action" && std::holds_alternative<std::string>(opt.value)) {
                action = std::get<std::string>(opt.value);
            } else if (opt.name == "target" && std::holds_alternative<dpp::snowflake>(opt.value)) {
                target_id = std::get<dpp::snowflake>(opt.value);
            }
        }

        if (action == "add") {
            get_database().add_logging_ignore(event.command.guild_id, target_id, "user");
            event.reply(success_embed("Ignore List Updated",
                "Added <@" + std::to_string(target_id) + "> to ignore list."));
        } else if (action == "remove") {
            get_database().remove_logging_ignore(event.command.guild_id, target_id, "user");
            event.reply(success_embed("Ignore List Updated",
                "Removed <@" + std::to_string(target_id) + "> from ignore list."));
        }
    }
}

std::optional<dpp::snowflake> LoggingModule::get_log_channel(dpp::snowflake guild_id, const std::string& type) {
    auto settings = get_database().get_logging_settings(guild_id);
    if (!settings) return std::nullopt;

    dpp::snowflake channel_id = 0;

    if (type == "messages") channel_id = settings->message_log_channel;
    else if (type == "members") channel_id = settings->member_log_channel;
    else if (type == "moderation") channel_id = settings->mod_log_channel;
    else if (type == "voice") channel_id = settings->voice_log_channel;
    else if (type == "server") channel_id = settings->server_log_channel;

    if (channel_id == 0) return std::nullopt;
    return channel_id;
}

void LoggingModule::send_log(dpp::snowflake channel_id, const dpp::embed& embed) {
    bot_.message_create(dpp::message(channel_id, "").add_embed(embed));
}

bool LoggingModule::should_log(dpp::snowflake guild_id, const std::string& event_type) {
    auto settings = get_database().get_logging_settings(guild_id);
    if (!settings) return false;

    if (event_type == "message_edits") return settings->log_message_edits;
    if (event_type == "message_deletes") return settings->log_message_deletes;
    if (event_type == "member_joins") return settings->log_member_joins;
    if (event_type == "member_leaves") return settings->log_member_leaves;
    if (event_type == "member_bans") return settings->log_member_bans;
    if (event_type == "voice_state") return settings->log_voice_state;
    if (event_type == "role_changes") return settings->log_role_changes;
    if (event_type == "nickname_changes") return settings->log_nickname_changes;

    return true;
}

bool LoggingModule::is_ignored(dpp::snowflake guild_id, dpp::snowflake id) {
    return get_database().is_logging_ignored(guild_id, id, "user") ||
           get_database().is_logging_ignored(guild_id, id, "channel");
}

void LoggingModule::cleanup_cache() {
    // Remove oldest entries (first half)
    size_t to_remove = message_cache_.size() / 2;
    auto it = message_cache_.begin();
    while (to_remove > 0 && it != message_cache_.end()) {
        it = message_cache_.erase(it);
        to_remove--;
    }
}

} // namespace bot
