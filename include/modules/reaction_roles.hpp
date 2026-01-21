#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace bot {

class ReactionRolesModule {
public:
    ReactionRolesModule(dpp::cluster& bot);

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Handle reaction events
    void handle_reaction_add(const dpp::message_reaction_add_t& event);
    void handle_reaction_remove(const dpp::message_reaction_remove_t& event);

private:
    dpp::cluster& bot_;

    // Command handlers
    void cmd_reactionrole(const dpp::slashcommand_t& event);

    // Subcommand handlers
    void create_reaction_role(const dpp::slashcommand_t& event);
    void add_role_to_message(const dpp::slashcommand_t& event);
    void remove_role_from_message(const dpp::slashcommand_t& event);
    void list_reaction_roles(const dpp::slashcommand_t& event);
    void delete_reaction_role(const dpp::slashcommand_t& event);

    // Helpers
    std::string normalize_emoji(const std::string& emoji);
    void update_reaction_role_message(dpp::snowflake message_id, dpp::snowflake channel_id);
};

} // namespace bot
