#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace bot {

class WelcomeModule {
public:
    WelcomeModule(dpp::cluster& bot);

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Handle member events
    void handle_member_join(const dpp::guild_member_add_t& event);
    void handle_member_leave(const dpp::guild_member_remove_t& event);

private:
    dpp::cluster& bot_;

    // Command handlers
    void cmd_welcome(const dpp::slashcommand_t& event);
    void cmd_goodbye(const dpp::slashcommand_t& event);

    // Message processing
    std::string process_message(const std::string& message, const dpp::guild_member& member, const dpp::guild& guild);
    dpp::message create_welcome_message(dpp::snowflake channel_id, const dpp::guild_member& member, const dpp::guild& guild);
    dpp::message create_goodbye_message(dpp::snowflake channel_id, const dpp::user& user, const dpp::guild& guild);

    // Auto-role assignment
    void assign_auto_role(const dpp::guild_member_add_t& event);

    // DM handling
    void send_welcome_dm(const dpp::guild_member& member, const dpp::guild& guild);
};

} // namespace bot
