#include "modules/welcome.hpp"
#include "database.hpp"
#include "utils/string_utils.hpp"
#include "utils/thread_pool.hpp"
#include "utils/common.hpp"

namespace bot {

WelcomeModule::WelcomeModule(dpp::cluster& bot) : bot_(bot) {}

std::vector<dpp::slashcommand> WelcomeModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    // /welcome command with subcommands
    dpp::slashcommand welcome("welcome", "Configure welcome messages", bot_.me.id);
    welcome.set_default_permissions(dpp::p_manage_guild);

    welcome.add_option(
        dpp::command_option(dpp::co_sub_command, "enable", "Enable or disable welcome messages")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable welcome messages", true))
    );

    welcome.add_option(
        dpp::command_option(dpp::co_sub_command, "channel", "Set welcome channel")
            .add_option(dpp::command_option(dpp::co_channel, "channel", "The channel", true))
    );

    welcome.add_option(
        dpp::command_option(dpp::co_sub_command, "message", "Set welcome message")
            .add_option(dpp::command_option(dpp::co_string, "text", "Message (use {user}, {server}, {member_count})", true))
    );

    welcome.add_option(
        dpp::command_option(dpp::co_sub_command, "embed", "Configure embed settings")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Use embed", true))
            .add_option(dpp::command_option(dpp::co_string, "color", "Embed color (hex)", false))
    );

    welcome.add_option(
        dpp::command_option(dpp::co_sub_command, "dm", "Configure DM welcome")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable DM", true))
            .add_option(dpp::command_option(dpp::co_string, "message", "DM message", false))
    );

    welcome.add_option(
        dpp::command_option(dpp::co_sub_command, "role", "Set auto-assign role")
            .add_option(dpp::command_option(dpp::co_role, "role", "Role to assign (leave empty to disable)", false))
    );

    welcome.add_option(
        dpp::command_option(dpp::co_sub_command, "test", "Test welcome message")
    );

    commands.push_back(welcome);

    // /goodbye command with similar structure
    dpp::slashcommand goodbye("goodbye", "Configure goodbye messages", bot_.me.id);
    goodbye.set_default_permissions(dpp::p_manage_guild);

    goodbye.add_option(
        dpp::command_option(dpp::co_sub_command, "enable", "Enable or disable goodbye messages")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Enable goodbye messages", true))
    );

    goodbye.add_option(
        dpp::command_option(dpp::co_sub_command, "channel", "Set goodbye channel")
            .add_option(dpp::command_option(dpp::co_channel, "channel", "The channel", true))
    );

    goodbye.add_option(
        dpp::command_option(dpp::co_sub_command, "message", "Set goodbye message")
            .add_option(dpp::command_option(dpp::co_string, "text", "Message (use {user}, {server})", true))
    );

    goodbye.add_option(
        dpp::command_option(dpp::co_sub_command, "embed", "Configure embed settings")
            .add_option(dpp::command_option(dpp::co_boolean, "enabled", "Use embed", true))
            .add_option(dpp::command_option(dpp::co_string, "color", "Embed color (hex)", false))
    );

    goodbye.add_option(
        dpp::command_option(dpp::co_sub_command, "test", "Test goodbye message")
    );

    commands.push_back(goodbye);

    return commands;
}

void WelcomeModule::handle_command(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    if (cmd == "welcome") {
        cmd_welcome(event);
    } else if (cmd == "goodbye") {
        cmd_goodbye(event);
    }
}

void WelcomeModule::handle_member_join(const dpp::guild_member_add_t& event) {
    auto settings = get_database().get_welcome_settings(event.adding_guild->id);
    if (!settings || !settings->enabled || settings->channel_id == 0) {
        return;
    }

    // Get guild info for member count
    bot_.guild_get(event.adding_guild->id, [this, event, settings](const dpp::confirmation_callback_t& callback) {
        if (callback.is_error()) return;

        dpp::guild guild = std::get<dpp::guild>(callback.value);

        // Send welcome message
        auto msg = create_welcome_message(settings->channel_id, event.added, guild);
        bot_.message_create(msg);

        // Auto-assign role
        if (settings->auto_role_id != 0) {
            bot_.guild_member_add_role(event.adding_guild->id, event.added.user_id, settings->auto_role_id);
        }

        // Send DM if enabled
        if (settings->dm_enabled && !settings->dm_message.empty()) {
            send_welcome_dm(event.added, guild);
        }
    });
}

void WelcomeModule::handle_member_leave(const dpp::guild_member_remove_t& event) {
    auto settings = get_database().get_goodbye_settings(event.removing_guild->id);
    if (!settings || !settings->enabled || settings->channel_id == 0) {
        return;
    }

    // Get guild info
    bot_.guild_get(event.removing_guild->id, [this, event, settings](const dpp::confirmation_callback_t& callback) {
        if (callback.is_error()) return;

        dpp::guild guild = std::get<dpp::guild>(callback.value);

        auto msg = create_goodbye_message(settings->channel_id, event.removed, guild);
        bot_.message_create(msg);
    });
}

std::string WelcomeModule::process_message(const std::string& message, const dpp::guild_member& member,
                                           const dpp::guild& guild) {
    std::map<std::string, std::string> vars = {
        {"user", "<@" + std::to_string(member.user_id) + ">"},
        {"user.name", member.get_user() ? member.get_user()->username : "User"},
        {"server", guild.name},
        {"member_count", std::to_string(guild.member_count)}
    };

    return string_utils::replace_variables(message, vars);
}

dpp::message WelcomeModule::create_welcome_message(dpp::snowflake channel_id, const dpp::guild_member& member,
                                                    const dpp::guild& guild) {
    auto settings = get_database().get_welcome_settings(guild.id);
    if (!settings) return dpp::message();

    std::string processed = process_message(settings->message, member, guild);

    dpp::message msg(channel_id, "");

    if (settings->use_embed) {
        uint32_t color = 0x00ff00;
        if (!settings->embed_color.empty() && settings->embed_color[0] == '#') {
            try {
                color = std::stoul(settings->embed_color.substr(1), nullptr, 16);
            } catch (...) {}
        }

        dpp::embed embed;
        embed.set_title("Welcome!")
             .set_description(processed)
             .set_color(color)
             .set_thumbnail(member.get_user() ? member.get_user()->get_avatar_url() : "")
             .set_timestamp(time(nullptr));

        msg.add_embed(embed);
    } else {
        msg.set_content(processed);
    }

    return msg;
}

dpp::message WelcomeModule::create_goodbye_message(dpp::snowflake channel_id, const dpp::user& user,
                                                    const dpp::guild& guild) {
    auto settings = get_database().get_goodbye_settings(guild.id);
    if (!settings) return dpp::message();

    std::map<std::string, std::string> vars = {
        {"user", user.username},
        {"server", guild.name}
    };

    std::string processed = string_utils::replace_variables(settings->message, vars);

    dpp::message msg(channel_id, "");

    if (settings->use_embed) {
        uint32_t color = 0xff0000;
        if (!settings->embed_color.empty() && settings->embed_color[0] == '#') {
            try {
                color = std::stoul(settings->embed_color.substr(1), nullptr, 16);
            } catch (...) {}
        }

        dpp::embed embed;
        embed.set_title("Goodbye!")
             .set_description(processed)
             .set_color(color)
             .set_thumbnail(user.get_avatar_url())
             .set_timestamp(time(nullptr));

        msg.add_embed(embed);
    } else {
        msg.set_content(processed);
    }

    return msg;
}

void WelcomeModule::send_welcome_dm(const dpp::guild_member& member, const dpp::guild& guild) {
    auto settings = get_database().get_welcome_settings(guild.id);
    if (!settings || !settings->dm_enabled || settings->dm_message.empty()) {
        return;
    }

    std::string processed = process_message(settings->dm_message, member, guild);

    bot_.direct_message_create(member.user_id,
        dpp::message().set_content(processed),
        [](const dpp::confirmation_callback_t& callback) {
            // Ignore DM failures (user may have DMs disabled)
        }
    );
}

void WelcomeModule::cmd_welcome(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    auto settings = get_database().get_welcome_settings(event.command.guild_id);
    Database::WelcomeSettings welcome_settings;
    if (settings) {
        welcome_settings = *settings;
    } else {
        welcome_settings.guild_id = event.command.guild_id;
    }

    if (subcmd == "enable") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                welcome_settings.enabled = std::get<bool>(opt.value);
            }
        }
        get_database().set_welcome_settings(welcome_settings);
        event.reply(success_embed("Welcome Messages",
            "Welcome messages " + std::string(welcome_settings.enabled ? "enabled" : "disabled")));

    } else if (subcmd == "channel") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "channel" && std::holds_alternative<dpp::snowflake>(opt.value)) {
                welcome_settings.channel_id = std::get<dpp::snowflake>(opt.value);
            }
        }
        get_database().set_welcome_settings(welcome_settings);
        event.reply(success_embed("Welcome Channel Set",
            "Welcome channel set to <#" + std::to_string(welcome_settings.channel_id) + ">"));

    } else if (subcmd == "message") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "text" && std::holds_alternative<std::string>(opt.value)) {
                welcome_settings.message = std::get<std::string>(opt.value);
            }
        }
        get_database().set_welcome_settings(welcome_settings);
        event.reply(success_embed("Welcome Message Set",
            "Message: " + welcome_settings.message));

    } else if (subcmd == "embed") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                welcome_settings.use_embed = std::get<bool>(opt.value);
            } else if (opt.name == "color" && std::holds_alternative<std::string>(opt.value)) {
                welcome_settings.embed_color = std::get<std::string>(opt.value);
            }
        }
        get_database().set_welcome_settings(welcome_settings);
        event.reply(success_embed("Embed Settings Updated",
            "Embed: " + std::string(welcome_settings.use_embed ? "enabled" : "disabled")));

    } else if (subcmd == "dm") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                welcome_settings.dm_enabled = std::get<bool>(opt.value);
            } else if (opt.name == "message" && std::holds_alternative<std::string>(opt.value)) {
                welcome_settings.dm_message = std::get<std::string>(opt.value);
            }
        }
        get_database().set_welcome_settings(welcome_settings);
        event.reply(success_embed("DM Settings Updated",
            "DM welcome: " + std::string(welcome_settings.dm_enabled ? "enabled" : "disabled")));

    } else if (subcmd == "role") {
        welcome_settings.auto_role_id = 0;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "role" && std::holds_alternative<dpp::snowflake>(opt.value)) {
                welcome_settings.auto_role_id = std::get<dpp::snowflake>(opt.value);
            }
        }
        get_database().set_welcome_settings(welcome_settings);

        if (welcome_settings.auto_role_id == 0) {
            event.reply(success_embed("Auto-Role Disabled", "Auto-role assignment disabled."));
        } else {
            event.reply(success_embed("Auto-Role Set",
                "New members will receive <@&" + std::to_string(welcome_settings.auto_role_id) + ">"));
        }

    } else if (subcmd == "test") {
        // Get the guild and create a fake welcome message
        bot_.guild_get(event.command.guild_id, [this, event, welcome_settings](const dpp::confirmation_callback_t& callback) {
            if (callback.is_error()) {
                event.reply(error_embed("Error", "Failed to get guild info."));
                return;
            }

            dpp::guild guild = std::get<dpp::guild>(callback.value);

            // Create a member from the command issuer
            dpp::guild_member member;
            member.user_id = event.command.get_issuing_user().id;

            // Process the message
            std::string processed = process_message(welcome_settings.message, member, guild);

            if (welcome_settings.use_embed) {
                uint32_t color = 0x00ff00;
                if (!welcome_settings.embed_color.empty() && welcome_settings.embed_color[0] == '#') {
                    try {
                        color = std::stoul(welcome_settings.embed_color.substr(1), nullptr, 16);
                    } catch (...) {}
                }

                dpp::embed embed;
                embed.set_title("Welcome! (Test)")
                     .set_description(processed)
                     .set_color(color)
                     .set_thumbnail(event.command.get_issuing_user().get_avatar_url())
                     .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));
            } else {
                event.reply("**[Test]** " + processed);
            }
        });
    }
}

void WelcomeModule::cmd_goodbye(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    auto settings = get_database().get_goodbye_settings(event.command.guild_id);
    Database::GoodbyeSettings goodbye_settings;
    if (settings) {
        goodbye_settings = *settings;
    } else {
        goodbye_settings.guild_id = event.command.guild_id;
    }

    if (subcmd == "enable") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                goodbye_settings.enabled = std::get<bool>(opt.value);
            }
        }
        get_database().set_goodbye_settings(goodbye_settings);
        event.reply(success_embed("Goodbye Messages",
            "Goodbye messages " + std::string(goodbye_settings.enabled ? "enabled" : "disabled")));

    } else if (subcmd == "channel") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "channel" && std::holds_alternative<dpp::snowflake>(opt.value)) {
                goodbye_settings.channel_id = std::get<dpp::snowflake>(opt.value);
            }
        }
        get_database().set_goodbye_settings(goodbye_settings);
        event.reply(success_embed("Goodbye Channel Set",
            "Goodbye channel set to <#" + std::to_string(goodbye_settings.channel_id) + ">"));

    } else if (subcmd == "message") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "text" && std::holds_alternative<std::string>(opt.value)) {
                goodbye_settings.message = std::get<std::string>(opt.value);
            }
        }
        get_database().set_goodbye_settings(goodbye_settings);
        event.reply(success_embed("Goodbye Message Set",
            "Message: " + goodbye_settings.message));

    } else if (subcmd == "embed") {
        for (const auto& opt : subcommand.options) {
            if (opt.name == "enabled" && std::holds_alternative<bool>(opt.value)) {
                goodbye_settings.use_embed = std::get<bool>(opt.value);
            } else if (opt.name == "color" && std::holds_alternative<std::string>(opt.value)) {
                goodbye_settings.embed_color = std::get<std::string>(opt.value);
            }
        }
        get_database().set_goodbye_settings(goodbye_settings);
        event.reply(success_embed("Embed Settings Updated",
            "Embed: " + std::string(goodbye_settings.use_embed ? "enabled" : "disabled")));

    } else if (subcmd == "test") {
        bot_.guild_get(event.command.guild_id, [this, event, goodbye_settings](const dpp::confirmation_callback_t& callback) {
            if (callback.is_error()) {
                event.reply(error_embed("Error", "Failed to get guild info."));
                return;
            }

            dpp::guild guild = std::get<dpp::guild>(callback.value);

            std::map<std::string, std::string> vars = {
                {"user", event.command.get_issuing_user().username},
                {"server", guild.name}
            };

            std::string processed = string_utils::replace_variables(goodbye_settings.message, vars);

            if (goodbye_settings.use_embed) {
                uint32_t color = 0xff0000;
                if (!goodbye_settings.embed_color.empty() && goodbye_settings.embed_color[0] == '#') {
                    try {
                        color = std::stoul(goodbye_settings.embed_color.substr(1), nullptr, 16);
                    } catch (...) {}
                }

                dpp::embed embed;
                embed.set_title("Goodbye! (Test)")
                     .set_description(processed)
                     .set_color(color)
                     .set_thumbnail(event.command.get_issuing_user().get_avatar_url())
                     .set_timestamp(time(nullptr));

                event.reply(dpp::message().add_embed(embed));
            } else {
                event.reply("**[Test]** " + processed);
            }
        });
    }
}

} // namespace bot
