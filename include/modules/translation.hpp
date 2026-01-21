#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace bot {

class TranslationModule {
public:
    TranslationModule(dpp::cluster& bot);

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Handle message for auto-translation
    void handle_message(const dpp::message_create_t& event);

    // Translation functions
    std::string detect_language(const std::string& text);
    std::string translate_text(const std::string& text, const std::string& source_lang, const std::string& target_lang);
    std::string get_language_code(const std::string& lang_input);

    // Auto-translate management (now uses database)
    void set_auto_translate(dpp::snowflake channel_id, dpp::snowflake guild_id, const std::vector<std::string>& languages);
    void disable_auto_translate(dpp::snowflake channel_id);
    std::vector<std::string> get_auto_translate_languages(dpp::snowflake channel_id);

private:
    dpp::cluster& bot_;

    // Command handlers
    void cmd_translate(const dpp::slashcommand_t& event);
    void cmd_detect_language(const dpp::slashcommand_t& event);
    void cmd_languages(const dpp::slashcommand_t& event);
    void cmd_auto_translate(const dpp::slashcommand_t& event);
};

} // namespace bot
