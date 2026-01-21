#include "modules/custom_commands.hpp"
#include "database.hpp"
#include "utils/string_utils.hpp"
#include "utils/common.hpp"

namespace bot {

CustomCommandsModule::CustomCommandsModule(dpp::cluster& bot) : bot_(bot) {}

std::vector<dpp::slashcommand> CustomCommandsModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    // /customcommand with subcommands
    dpp::slashcommand customcmd("customcommand", "Manage custom commands", bot_.me.id);
    customcmd.set_default_permissions(dpp::p_manage_guild);

    customcmd.add_option(
        dpp::command_option(dpp::co_sub_command, "create", "Create a custom command")
            .add_option(dpp::command_option(dpp::co_string, "name", "Command name", true))
            .add_option(dpp::command_option(dpp::co_string, "response", "Command response", true))
            .add_option(dpp::command_option(dpp::co_boolean, "embed", "Send as embed", false))
    );

    customcmd.add_option(
        dpp::command_option(dpp::co_sub_command, "delete", "Delete a custom command")
            .add_option(dpp::command_option(dpp::co_string, "name", "Command name", true))
    );

    customcmd.add_option(
        dpp::command_option(dpp::co_sub_command, "edit", "Edit a custom command")
            .add_option(dpp::command_option(dpp::co_string, "name", "Command name", true))
            .add_option(dpp::command_option(dpp::co_string, "response", "New response", false))
            .add_option(dpp::command_option(dpp::co_boolean, "embed", "Send as embed", false))
    );

    customcmd.add_option(
        dpp::command_option(dpp::co_sub_command, "list", "List all custom commands")
    );

    commands.push_back(customcmd);

    // /c command to execute custom commands
    commands.push_back(
        dpp::slashcommand("c", "Execute a custom command", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "name", "Command name", true)
                .set_auto_complete(true))
    );

    return commands;
}

void CustomCommandsModule::handle_command(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    if (cmd == "customcommand") {
        cmd_customcommand(event);
    } else if (cmd == "c") {
        cmd_c(event);
    }
}

void CustomCommandsModule::cmd_customcommand(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    if (subcmd == "create") {
        create_command(event);
    } else if (subcmd == "delete") {
        delete_command(event);
    } else if (subcmd == "edit") {
        edit_command(event);
    } else if (subcmd == "list") {
        list_commands(event);
    }
}

void CustomCommandsModule::cmd_c(const dpp::slashcommand_t& event) {
    std::string name = std::get<std::string>(event.get_parameter("name"));
    auto msg = execute_custom_command(event.command.guild_id, name,
                                      event.command.get_issuing_user(),
                                      event.command.channel_id);

    if (msg.content.empty() && msg.embeds.empty()) {
        event.reply(error_embed("Not Found", "Command `" + name + "` not found."));
    } else {
        event.reply(msg);
    }
}

void CustomCommandsModule::create_command(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];

    std::string name;
    std::string response;
    bool is_embed = false;

    for (const auto& opt : subcommand.options) {
        if (opt.name == "name" && std::holds_alternative<std::string>(opt.value)) {
            name = std::get<std::string>(opt.value);
        } else if (opt.name == "response" && std::holds_alternative<std::string>(opt.value)) {
            response = std::get<std::string>(opt.value);
        } else if (opt.name == "embed" && std::holds_alternative<bool>(opt.value)) {
            is_embed = std::get<bool>(opt.value);
        }
    }

    // Check if command already exists
    auto existing = get_database().get_custom_command(event.command.guild_id, name);
    if (existing) {
        event.reply(error_embed("Already Exists", "Command `" + name + "` already exists."));
        return;
    }

    Database::CustomCommand cmd;
    cmd.guild_id = event.command.guild_id;
    cmd.name = name;
    cmd.response = response;
    cmd.is_embed = is_embed;
    cmd.created_by = event.command.get_issuing_user().id;

    if (get_database().create_custom_command(cmd)) {
        event.reply(success_embed("Command Created", "Created command `" + name + "`"));
    } else {
        event.reply(error_embed("Error", "Failed to create command."));
    }
}

void CustomCommandsModule::delete_command(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string name;
    for (const auto& opt : subcommand.options) {
        if (opt.name == "name" && std::holds_alternative<std::string>(opt.value)) {
            name = std::get<std::string>(opt.value);
            break;
        }
    }

    if (get_database().delete_custom_command(event.command.guild_id, name)) {
        event.reply(success_embed("Command Deleted", "Deleted command `" + name + "`"));
    } else {
        event.reply(error_embed("Not Found", "Command `" + name + "` not found."));
    }
}

void CustomCommandsModule::edit_command(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string name;
    for (const auto& opt : subcommand.options) {
        if (opt.name == "name" && std::holds_alternative<std::string>(opt.value)) {
            name = std::get<std::string>(opt.value);
            break;
        }
    }

    auto existing = get_database().get_custom_command(event.command.guild_id, name);
    if (!existing) {
        event.reply(error_embed("Not Found", "Command `" + name + "` not found."));
        return;
    }

    Database::CustomCommand cmd = *existing;

    for (const auto& opt : subcommand.options) {
        if (opt.name == "response" && std::holds_alternative<std::string>(opt.value)) {
            cmd.response = std::get<std::string>(opt.value);
        } else if (opt.name == "embed" && std::holds_alternative<bool>(opt.value)) {
            cmd.is_embed = std::get<bool>(opt.value);
        }
    }

    if (get_database().update_custom_command(cmd)) {
        event.reply(success_embed("Command Updated", "Updated command `" + name + "`"));
    } else {
        event.reply(error_embed("Error", "Failed to update command."));
    }
}

void CustomCommandsModule::list_commands(const dpp::slashcommand_t& event) {
    auto commands = get_database().get_guild_custom_commands(event.command.guild_id);

    if (commands.empty()) {
        event.reply(info_embed("Custom Commands", "No custom commands found."));
        return;
    }

    std::string description;
    for (const auto& cmd : commands) {
        description += "`" + cmd.name + "` - " + std::to_string(cmd.uses) + " uses\n";
    }

    dpp::embed embed;
    embed.set_title("📝 Custom Commands")
         .set_description(description)
         .set_color(0x0099ff)
         .set_footer(dpp::embed_footer().set_text(std::to_string(commands.size()) + " commands"));

    event.reply(dpp::message().add_embed(embed));
}

dpp::message CustomCommandsModule::execute_custom_command(dpp::snowflake guild_id, const std::string& name,
                                                          const dpp::user& user, dpp::snowflake channel_id) {
    auto cmd = get_database().get_custom_command(guild_id, name);
    if (!cmd) {
        return dpp::message();
    }

    get_database().increment_command_uses(guild_id, name);

    std::string processed = process_response(cmd->response, user, guild_id, channel_id);

    dpp::message msg;
    if (cmd->is_embed) {
        dpp::embed embed;
        embed.set_description(processed)
             .set_color(0x0099ff);
        msg.add_embed(embed);
    } else {
        msg.set_content(processed);
    }

    return msg;
}

std::string CustomCommandsModule::process_response(const std::string& response, const dpp::user& user,
                                                    dpp::snowflake guild_id, dpp::snowflake channel_id) {
    std::map<std::string, std::string> vars = {
        {"user", "<@" + std::to_string(user.id) + ">"},
        {"user.name", user.username},
        {"user.id", std::to_string(user.id)},
        {"channel", "<#" + std::to_string(channel_id) + ">"},
        {"server.id", std::to_string(guild_id)}
    };

    return string_utils::replace_variables(response, vars);
}

} // namespace bot
