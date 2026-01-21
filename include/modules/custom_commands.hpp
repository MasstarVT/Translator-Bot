#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace bot {

class CustomCommandsModule {
public:
    CustomCommandsModule(dpp::cluster& bot);

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

private:
    dpp::cluster& bot_;

    // Command handlers
    void cmd_customcommand(const dpp::slashcommand_t& event);
    void cmd_c(const dpp::slashcommand_t& event);

    // Subcommand handlers
    void create_command(const dpp::slashcommand_t& event);
    void delete_command(const dpp::slashcommand_t& event);
    void edit_command(const dpp::slashcommand_t& event);
    void list_commands(const dpp::slashcommand_t& event);

    // Execute custom command and return response
    dpp::message execute_custom_command(dpp::snowflake guild_id, const std::string& name, const dpp::user& user, dpp::snowflake channel_id);

    // Variable replacement in command response
    std::string process_response(const std::string& response, const dpp::user& user, dpp::snowflake guild_id, dpp::snowflake channel_id);
};

} // namespace bot
