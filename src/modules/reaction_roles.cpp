#include "modules/reaction_roles.hpp"
#include "database.hpp"
#include "utils/common.hpp"
#include "utils/thread_pool.hpp"

namespace bot {

ReactionRolesModule::ReactionRolesModule(dpp::cluster& bot) : bot_(bot) {}

std::vector<dpp::slashcommand> ReactionRolesModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    dpp::slashcommand reactionrole("reactionrole", "Manage reaction roles", bot_.me.id);
    reactionrole.set_default_permissions(dpp::p_manage_roles);

    reactionrole.add_option(
        dpp::command_option(dpp::co_sub_command, "create", "Create a reaction role message")
            .add_option(dpp::command_option(dpp::co_channel, "channel", "Channel to send message", true))
            .add_option(dpp::command_option(dpp::co_string, "title", "Message title", false))
            .add_option(dpp::command_option(dpp::co_string, "mode", "Selection mode", false)
                .add_choice(dpp::command_option_choice("Normal (multiple roles)", std::string("normal")))
                .add_choice(dpp::command_option_choice("Unique (one role only)", std::string("unique")))
                .add_choice(dpp::command_option_choice("Verify (removes reaction)", std::string("verify"))))
    );

    reactionrole.add_option(
        dpp::command_option(dpp::co_sub_command, "add", "Add a role to a reaction role message")
            .add_option(dpp::command_option(dpp::co_string, "message_id", "Message ID", true))
            .add_option(dpp::command_option(dpp::co_string, "emoji", "Emoji to use", true))
            .add_option(dpp::command_option(dpp::co_role, "role", "Role to assign", true))
    );

    reactionrole.add_option(
        dpp::command_option(dpp::co_sub_command, "remove", "Remove a role from a reaction role message")
            .add_option(dpp::command_option(dpp::co_string, "message_id", "Message ID", true))
            .add_option(dpp::command_option(dpp::co_string, "emoji", "Emoji to remove", true))
    );

    reactionrole.add_option(
        dpp::command_option(dpp::co_sub_command, "list", "List all reaction role configurations")
    );

    reactionrole.add_option(
        dpp::command_option(dpp::co_sub_command, "delete", "Delete a reaction role configuration")
            .add_option(dpp::command_option(dpp::co_string, "message_id", "Message ID", true))
    );

    commands.push_back(reactionrole);

    return commands;
}

void ReactionRolesModule::handle_command(const dpp::slashcommand_t& event) {
    cmd_reactionrole(event);
}

void ReactionRolesModule::handle_reaction_add(const dpp::message_reaction_add_t& event) {
    if (event.reacting_user.id == 0) {
        return;
    }

    auto config = get_database().get_reaction_role_message(event.message_id);
    if (!config) {
        return;
    }

    std::string emoji = normalize_emoji(event.reacting_emoji.format());
    auto role = get_database().get_reaction_role(config->id, emoji);

    if (!role) {
        return;
    }

    // Handle unique mode - remove other roles from this message first
    if (config->mode == "unique") {
        auto all_roles = get_database().get_reaction_roles(config->id);
        for (const auto& r : all_roles) {
            if (r.emoji != emoji) {
                bot_.guild_member_remove_role(config->guild_id, event.reacting_user.id, r.role_id);
            }
        }
    }

    // Add the role
    bot_.guild_member_add_role(config->guild_id, event.reacting_user.id, role->role_id,
        [this, config, event](const dpp::confirmation_callback_t& callback) {
            // Handle verify mode - remove the reaction after adding role
            if (config->mode == "verify" && !callback.is_error()) {
                bot_.message_delete_reaction(event.message_id, event.channel_id,
                                             event.reacting_user.id, event.reacting_emoji.format());
            }
        }
    );
}

void ReactionRolesModule::handle_reaction_remove(const dpp::message_reaction_remove_t& event) {
    auto config = get_database().get_reaction_role_message(event.message_id);
    if (!config) {
        return;
    }

    // Don't remove roles in verify mode (they keep the role after reaction is removed)
    if (config->mode == "verify") {
        return;
    }

    std::string emoji = normalize_emoji(event.reacting_emoji.format());
    auto role = get_database().get_reaction_role(config->id, emoji);

    if (!role) {
        return;
    }

    bot_.guild_member_remove_role(config->guild_id, event.reacting_user_id, role->role_id);
}

void ReactionRolesModule::cmd_reactionrole(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    if (subcmd == "create") {
        create_reaction_role(event);
    } else if (subcmd == "add") {
        add_role_to_message(event);
    } else if (subcmd == "remove") {
        remove_role_from_message(event);
    } else if (subcmd == "list") {
        list_reaction_roles(event);
    } else if (subcmd == "delete") {
        delete_reaction_role(event);
    }
}

void ReactionRolesModule::create_reaction_role(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];

    dpp::snowflake channel_id = 0;
    std::string title = "React to get roles!";
    std::string mode = "normal";

    for (const auto& opt : subcommand.options) {
        if (opt.name == "channel" && std::holds_alternative<dpp::snowflake>(opt.value)) {
            channel_id = std::get<dpp::snowflake>(opt.value);
        } else if (opt.name == "title" && std::holds_alternative<std::string>(opt.value)) {
            title = std::get<std::string>(opt.value);
        } else if (opt.name == "mode" && std::holds_alternative<std::string>(opt.value)) {
            mode = std::get<std::string>(opt.value);
        }
    }

    // Create the embed message
    dpp::embed embed;
    embed.set_title(title)
         .set_description("React below to get your roles!\n\n*No roles configured yet*")
         .set_color(0x5865F2);

    bot_.message_create(dpp::message(channel_id, "").add_embed(embed),
        [this, event, title, mode](const dpp::confirmation_callback_t& callback) {
            if (callback.is_error()) {
                event.reply(error_embed("Error", "Failed to create message."));
                return;
            }

            dpp::message msg = std::get<dpp::message>(callback.value);

            Database::ReactionRoleMessage config;
            config.guild_id = event.command.guild_id;
            config.channel_id = msg.channel_id;
            config.message_id = msg.id;
            config.title = title;
            config.mode = mode;

            get_database().create_reaction_role_message(config);

            event.reply(success_embed("Reaction Roles Created",
                "Created reaction role message in <#" + std::to_string(msg.channel_id) + ">\n" +
                "Message ID: `" + std::to_string(msg.id) + "`\n" +
                "Mode: " + mode + "\n\n" +
                "Use `/reactionrole add` to add roles."));
        }
    );
}

void ReactionRolesModule::add_role_to_message(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];

    std::string message_id_str;
    std::string emoji;
    dpp::snowflake role_id = 0;
    for (const auto& opt : subcommand.options) {
        if (opt.name == "message_id" && std::holds_alternative<std::string>(opt.value)) {
            message_id_str = std::get<std::string>(opt.value);
        } else if (opt.name == "emoji" && std::holds_alternative<std::string>(opt.value)) {
            emoji = std::get<std::string>(opt.value);
        } else if (opt.name == "role" && std::holds_alternative<dpp::snowflake>(opt.value)) {
            role_id = std::get<dpp::snowflake>(opt.value);
        }
    }

    dpp::snowflake message_id;
    try {
        message_id = std::stoull(message_id_str);
    } catch (...) {
        event.reply(error_embed("Invalid ID", "Please provide a valid message ID."));
        return;
    }

    auto config = get_database().get_reaction_role_message(message_id);
    if (!config) {
        event.reply(error_embed("Not Found", "No reaction role configuration found for that message."));
        return;
    }

    std::string normalized_emoji = normalize_emoji(emoji);
    get_database().add_reaction_role(config->id, normalized_emoji, role_id);

    // Add reaction to the message
    bot_.message_add_reaction(message_id, config->channel_id, emoji,
        [this, event, emoji, role_id, message_id, config](const dpp::confirmation_callback_t& callback) {
            if (callback.is_error()) {
                event.reply(error_embed("Error", "Failed to add reaction. Make sure the emoji is valid."));
                return;
            }

            // Update the message embed
            update_reaction_role_message(message_id, config->channel_id);

            event.reply(success_embed("Role Added",
                "Added " + emoji + " -> <@&" + std::to_string(role_id) + ">"));
        }
    );
}

void ReactionRolesModule::remove_role_from_message(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];

    std::string message_id_str;
    std::string emoji;
    for (const auto& opt : subcommand.options) {
        if (opt.name == "message_id" && std::holds_alternative<std::string>(opt.value)) {
            message_id_str = std::get<std::string>(opt.value);
        } else if (opt.name == "emoji" && std::holds_alternative<std::string>(opt.value)) {
            emoji = std::get<std::string>(opt.value);
        }
    }

    dpp::snowflake message_id;
    try {
        message_id = std::stoull(message_id_str);
    } catch (...) {
        event.reply(error_embed("Invalid ID", "Please provide a valid message ID."));
        return;
    }

    auto config = get_database().get_reaction_role_message(message_id);
    if (!config) {
        event.reply(error_embed("Not Found", "No reaction role configuration found for that message."));
        return;
    }

    std::string normalized_emoji = normalize_emoji(emoji);
    get_database().remove_reaction_role(config->id, normalized_emoji);

    // Update the message embed
    update_reaction_role_message(message_id, config->channel_id);

    event.reply(success_embed("Role Removed", "Removed " + emoji + " from reaction roles."));
}

void ReactionRolesModule::list_reaction_roles(const dpp::slashcommand_t& event) {
    auto configs = get_database().get_guild_reaction_role_messages(event.command.guild_id);

    if (configs.empty()) {
        event.reply(info_embed("Reaction Roles", "No reaction role configurations found."));
        return;
    }

    std::string description;
    for (const auto& config : configs) {
        auto roles = get_database().get_reaction_roles(config.id);

        description += "**" + config.title + "**\n";
        description += "Channel: <#" + std::to_string(config.channel_id) + ">\n";
        description += "Message ID: `" + std::to_string(config.message_id) + "`\n";
        description += "Mode: " + config.mode + "\n";
        description += "Roles: " + std::to_string(roles.size()) + "\n\n";
    }

    dpp::embed embed;
    embed.set_title("Reaction Role Configurations")
         .set_description(description)
         .set_color(0x5865F2);

    event.reply(dpp::message().add_embed(embed));
}

void ReactionRolesModule::delete_reaction_role(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string message_id_str;
    for (const auto& opt : subcommand.options) {
        if (opt.name == "message_id" && std::holds_alternative<std::string>(opt.value)) {
            message_id_str = std::get<std::string>(opt.value);
            break;
        }
    }

    dpp::snowflake message_id;
    try {
        message_id = std::stoull(message_id_str);
    } catch (...) {
        event.reply(error_embed("Invalid ID", "Please provide a valid message ID."));
        return;
    }

    auto config = get_database().get_reaction_role_message(message_id);
    if (!config) {
        event.reply(error_embed("Not Found", "No reaction role configuration found for that message."));
        return;
    }

    get_database().delete_reaction_role_message(message_id);

    // Optionally delete the message
    bot_.message_delete(message_id, config->channel_id);

    event.reply(success_embed("Configuration Deleted", "Deleted reaction role configuration."));
}

std::string ReactionRolesModule::normalize_emoji(const std::string& emoji) {
    // Handle custom emoji format
    if (emoji.find(':') != std::string::npos) {
        // Extract just the name:id part
        size_t start = emoji.find_first_of('<');
        size_t end = emoji.find_last_of('>');
        if (start != std::string::npos && end != std::string::npos) {
            return emoji.substr(start, end - start + 1);
        }
    }
    return emoji;
}

void ReactionRolesModule::update_reaction_role_message(dpp::snowflake message_id, dpp::snowflake channel_id) {
    auto config = get_database().get_reaction_role_message(message_id);
    if (!config) return;

    auto roles = get_database().get_reaction_roles(config->id);

    std::string description = "React below to get your roles!\n\n";

    if (roles.empty()) {
        description += "*No roles configured yet*";
    } else {
        for (const auto& role : roles) {
            description += role.emoji + " - <@&" + std::to_string(role.role_id) + ">\n";
        }
    }

    dpp::embed embed;
    embed.set_title(config->title)
         .set_description(description)
         .set_color(0x5865F2);

    bot_.message_get(message_id, channel_id,
        [this, embed, message_id, channel_id](const dpp::confirmation_callback_t& callback) {
            if (callback.is_error()) return;

            dpp::message msg = std::get<dpp::message>(callback.value);
            msg.embeds.clear();
            msg.add_embed(embed);

            bot_.message_edit(msg);
        }
    );
}

} // namespace bot
