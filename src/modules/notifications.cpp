#include "modules/notifications.hpp"
#include "database.hpp"
#include "config.hpp"
#include "utils/common.hpp"
#include "utils/curl_helper.hpp"

namespace bot {

NotificationsModule::NotificationsModule(dpp::cluster& bot) : bot_(bot) {}

NotificationsModule::~NotificationsModule() {
    stop();
}

std::vector<dpp::slashcommand> NotificationsModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    // Twitch notifications
    dpp::slashcommand twitch("twitch", "Manage Twitch stream notifications", bot_.me.id);
    twitch.set_default_permissions(dpp::p_manage_guild);

    twitch.add_option(
        dpp::command_option(dpp::co_sub_command, "add", "Add a Twitch streamer")
            .add_option(dpp::command_option(dpp::co_string, "username", "Twitch username", true))
            .add_option(dpp::command_option(dpp::co_channel, "channel", "Notification channel", true))
            .add_option(dpp::command_option(dpp::co_role, "role", "Role to ping", false))
            .add_option(dpp::command_option(dpp::co_string, "message", "Custom message", false))
    );

    twitch.add_option(
        dpp::command_option(dpp::co_sub_command, "remove", "Remove a Twitch streamer")
            .add_option(dpp::command_option(dpp::co_string, "username", "Twitch username", true))
    );

    twitch.add_option(
        dpp::command_option(dpp::co_sub_command, "list", "List Twitch notifications")
    );

    commands.push_back(twitch);

    // YouTube notifications
    dpp::slashcommand youtube("youtube", "Manage YouTube upload notifications", bot_.me.id);
    youtube.set_default_permissions(dpp::p_manage_guild);

    youtube.add_option(
        dpp::command_option(dpp::co_sub_command, "add", "Add a YouTube channel")
            .add_option(dpp::command_option(dpp::co_string, "channel_id", "YouTube channel ID", true))
            .add_option(dpp::command_option(dpp::co_channel, "channel", "Notification channel", true))
            .add_option(dpp::command_option(dpp::co_role, "role", "Role to ping", false))
            .add_option(dpp::command_option(dpp::co_string, "message", "Custom message", false))
    );

    youtube.add_option(
        dpp::command_option(dpp::co_sub_command, "remove", "Remove a YouTube channel")
            .add_option(dpp::command_option(dpp::co_string, "channel_id", "YouTube channel ID", true))
    );

    youtube.add_option(
        dpp::command_option(dpp::co_sub_command, "list", "List YouTube notifications")
    );

    commands.push_back(youtube);

    return commands;
}

void NotificationsModule::handle_command(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    if (cmd == "twitch") {
        cmd_twitch(event);
    } else if (cmd == "youtube") {
        cmd_youtube(event);
    }
}

void NotificationsModule::start() {
    if (running_) return;
    running_ = true;

    // Start Twitch checker thread
    twitch_checker_thread_ = std::thread([this]() {
        while (running_) {
            check_twitch_streams();
            // Check every 60 seconds
            for (int i = 0; i < 60 && running_; i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    });

    // Start YouTube checker thread
    youtube_checker_thread_ = std::thread([this]() {
        while (running_) {
            check_youtube_uploads();
            // Check every 5 minutes
            for (int i = 0; i < 300 && running_; i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    });
}

void NotificationsModule::stop() {
    running_ = false;

    if (twitch_checker_thread_.joinable()) {
        twitch_checker_thread_.join();
    }
    if (youtube_checker_thread_.joinable()) {
        youtube_checker_thread_.join();
    }
}

void NotificationsModule::cmd_twitch(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    if (subcmd == "add") {
        twitch_add(event);
    } else if (subcmd == "remove") {
        twitch_remove(event);
    } else if (subcmd == "list") {
        twitch_list(event);
    }
}

void NotificationsModule::cmd_youtube(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    if (subcmd == "add") {
        youtube_add(event);
    } else if (subcmd == "remove") {
        youtube_remove(event);
    } else if (subcmd == "list") {
        youtube_list(event);
    }
}

void NotificationsModule::twitch_add(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];

    std::string username;
    dpp::snowflake channel_id = 0;
    dpp::snowflake role_id = 0;
    std::string message = "";

    for (const auto& opt : subcommand.options) {
        if (opt.name == "username" && std::holds_alternative<std::string>(opt.value)) {
            username = std::get<std::string>(opt.value);
        } else if (opt.name == "channel" && std::holds_alternative<dpp::snowflake>(opt.value)) {
            channel_id = std::get<dpp::snowflake>(opt.value);
        } else if (opt.name == "role" && std::holds_alternative<dpp::snowflake>(opt.value)) {
            role_id = std::get<dpp::snowflake>(opt.value);
        } else if (opt.name == "message" && std::holds_alternative<std::string>(opt.value)) {
            message = std::get<std::string>(opt.value);
        }
    }

    Database::TwitchNotification notif;
    notif.guild_id = event.command.guild_id;
    notif.twitch_username = username;
    notif.channel_id = channel_id;
    notif.ping_role_id = role_id;
    notif.custom_message = message;

    if (get_database().add_twitch_notification(notif)) {
        event.reply(success_embed("Twitch Notification Added",
            "Now monitoring **" + username + "** for live streams.\n" +
            "Notifications will be sent to <#" + std::to_string(channel_id) + ">"));
    } else {
        event.reply(error_embed("Error", "Failed to add notification."));
    }
}

void NotificationsModule::twitch_remove(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string username;
    for (const auto& opt : subcommand.options) {
        if (opt.name == "username" && std::holds_alternative<std::string>(opt.value)) {
            username = std::get<std::string>(opt.value);
            break;
        }
    }

    if (get_database().remove_twitch_notification(event.command.guild_id, username)) {
        event.reply(success_embed("Removed", "Removed Twitch notifications for **" + username + "**"));
    } else {
        event.reply(error_embed("Not Found", "No notification found for **" + username + "**"));
    }
}

void NotificationsModule::twitch_list(const dpp::slashcommand_t& event) {
    auto notifications = get_database().get_twitch_notifications(event.command.guild_id);

    if (notifications.empty()) {
        event.reply(info_embed("Twitch Notifications", "No Twitch notifications configured."));
        return;
    }

    std::string description;
    for (const auto& n : notifications) {
        description += "**" + n.twitch_username + "**\n";
        description += "  Channel: <#" + std::to_string(n.channel_id) + ">\n";
        if (n.ping_role_id != 0) {
            description += "  Ping: <@&" + std::to_string(n.ping_role_id) + ">\n";
        }
        description += "  Status: " + std::string(n.is_live ? "🟢 Live" : "⚫ Offline") + "\n\n";
    }

    event.reply(info_embed("Twitch Notifications", description));
}

void NotificationsModule::youtube_add(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];

    std::string channel_id_str;
    dpp::snowflake discord_channel_id = 0;
    dpp::snowflake role_id = 0;
    std::string message = "";

    for (const auto& opt : subcommand.options) {
        if (opt.name == "channel_id" && std::holds_alternative<std::string>(opt.value)) {
            channel_id_str = std::get<std::string>(opt.value);
        } else if (opt.name == "channel" && std::holds_alternative<dpp::snowflake>(opt.value)) {
            discord_channel_id = std::get<dpp::snowflake>(opt.value);
        } else if (opt.name == "role" && std::holds_alternative<dpp::snowflake>(opt.value)) {
            role_id = std::get<dpp::snowflake>(opt.value);
        } else if (opt.name == "message" && std::holds_alternative<std::string>(opt.value)) {
            message = std::get<std::string>(opt.value);
        }
    }

    Database::YouTubeNotification notif;
    notif.guild_id = event.command.guild_id;
    notif.youtube_channel_id = channel_id_str;
    notif.discord_channel_id = discord_channel_id;
    notif.ping_role_id = role_id;
    notif.custom_message = message;

    if (get_database().add_youtube_notification(notif)) {
        event.reply(success_embed("YouTube Notification Added",
            "Now monitoring YouTube channel **" + channel_id_str + "** for new uploads.\n" +
            "Notifications will be sent to <#" + std::to_string(discord_channel_id) + ">"));
    } else {
        event.reply(error_embed("Error", "Failed to add notification."));
    }
}

void NotificationsModule::youtube_remove(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string channel_id;
    for (const auto& opt : subcommand.options) {
        if (opt.name == "channel_id" && std::holds_alternative<std::string>(opt.value)) {
            channel_id = std::get<std::string>(opt.value);
            break;
        }
    }

    if (get_database().remove_youtube_notification(event.command.guild_id, channel_id)) {
        event.reply(success_embed("Removed", "Removed YouTube notifications for channel **" + channel_id + "**"));
    } else {
        event.reply(error_embed("Not Found", "No notification found for that channel."));
    }
}

void NotificationsModule::youtube_list(const dpp::slashcommand_t& event) {
    auto notifications = get_database().get_youtube_notifications(event.command.guild_id);

    if (notifications.empty()) {
        event.reply(info_embed("YouTube Notifications", "No YouTube notifications configured."));
        return;
    }

    std::string description;
    for (const auto& n : notifications) {
        description += "**" + n.youtube_channel_id + "**\n";
        description += "  Channel: <#" + std::to_string(n.discord_channel_id) + ">\n";
        if (n.ping_role_id != 0) {
            description += "  Ping: <@&" + std::to_string(n.ping_role_id) + ">\n";
        }
        description += "\n";
    }

    event.reply(info_embed("YouTube Notifications", description));
}

void NotificationsModule::check_twitch_streams() {
    // This requires Twitch API credentials
    auto& config = get_config();
    auto client_id = config.get_twitch_client_id();
    auto client_secret = config.get_twitch_client_secret();

    if (!client_id || !client_secret) {
        return;  // No credentials configured
    }

    auto notifications = get_database().get_all_twitch_notifications();

    for (const auto& notif : notifications) {
        bool currently_live = is_twitch_live(notif.twitch_username);

        // Check for state change (went live)
        if (currently_live && !notif.is_live) {
            send_twitch_notification(notif.guild_id, notif.channel_id,
                                    notif.twitch_username, notif.ping_role_id,
                                    notif.custom_message);
        }

        // Update status
        if (currently_live != notif.is_live) {
            get_database().update_twitch_live_status(notif.guild_id, notif.twitch_username, currently_live);
        }
    }
}

void NotificationsModule::check_youtube_uploads() {
    // This requires YouTube API key
    auto& config = get_config();
    auto api_key = config.get_youtube_api_key();

    if (!api_key) {
        return;  // No API key configured
    }

    auto notifications = get_database().get_all_youtube_notifications();

    for (const auto& notif : notifications) {
        std::string latest_video = get_latest_youtube_video(notif.youtube_channel_id);

        if (!latest_video.empty() && latest_video != notif.last_video_id) {
            // New video uploaded
            send_youtube_notification(notif.guild_id, notif.discord_channel_id,
                                     latest_video, "", // Would need to fetch title
                                     notif.ping_role_id, notif.custom_message);

            get_database().update_youtube_last_video(notif.guild_id, notif.youtube_channel_id, latest_video);
        }
    }
}

bool NotificationsModule::is_twitch_live(const std::string& username) {
    // Placeholder - requires Twitch API implementation
    // Would use Helix API: GET https://api.twitch.tv/helix/streams?user_login={username}
    return false;
}

std::string NotificationsModule::get_latest_youtube_video(const std::string& channel_id) {
    // Placeholder - requires YouTube Data API implementation
    // Would use: GET https://www.googleapis.com/youtube/v3/search?channelId={channel_id}&order=date&type=video
    return "";
}

void NotificationsModule::send_twitch_notification(dpp::snowflake guild_id, dpp::snowflake channel_id,
                                                    const std::string& username, dpp::snowflake ping_role_id,
                                                    const std::string& custom_message) {
    std::string content;
    if (ping_role_id != 0) {
        content = "<@&" + std::to_string(ping_role_id) + "> ";
    }

    std::string message = custom_message.empty() ?
        "**" + username + "** is now live on Twitch!" :
        custom_message;

    dpp::embed embed;
    embed.set_title("🔴 " + username + " is Live!")
         .set_description(message)
         .set_color(0x9146FF)  // Twitch purple
         .set_url("https://twitch.tv/" + username)
         .set_timestamp(time(nullptr));

    bot_.message_create(dpp::message(channel_id, content).add_embed(embed));
}

void NotificationsModule::send_youtube_notification(dpp::snowflake guild_id, dpp::snowflake channel_id,
                                                     const std::string& video_id, const std::string& video_title,
                                                     dpp::snowflake ping_role_id, const std::string& custom_message) {
    std::string content;
    if (ping_role_id != 0) {
        content = "<@&" + std::to_string(ping_role_id) + "> ";
    }

    std::string title = video_title.empty() ? "New Video" : video_title;
    std::string message = custom_message.empty() ?
        "A new video has been uploaded!" :
        custom_message;

    dpp::embed embed;
    embed.set_title("🎬 " + title)
         .set_description(message)
         .set_color(0xFF0000)  // YouTube red
         .set_url("https://youtube.com/watch?v=" + video_id)
         .set_timestamp(time(nullptr));

    bot_.message_create(dpp::message(channel_id, content).add_embed(embed));
}

} // namespace bot
