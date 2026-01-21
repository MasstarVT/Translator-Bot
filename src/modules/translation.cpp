#include "modules/translation.hpp"
#include "database.hpp"
#include "utils/string_utils.hpp"
#include "utils/curl_helper.hpp"
#include "utils/thread_pool.hpp"
#include "utils/common.hpp"
#include <nlohmann/json.hpp>
#include <regex>
#include <iostream>

namespace bot {

using json = nlohmann::json;

TranslationModule::TranslationModule(dpp::cluster& bot) : bot_(bot) {}

std::vector<dpp::slashcommand> TranslationModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    // /translate command
    commands.push_back(
        dpp::slashcommand("translate", "Translate text to a target language", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "text", "The text to translate", true))
            .add_option(dpp::command_option(dpp::co_string, "target_language", "Target language", true)
                .set_auto_complete(true))
    );

    // /detectlanguage command
    commands.push_back(
        dpp::slashcommand("detectlanguage", "Detect the language of text", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "text", "The text to analyze", true))
    );

    // /languages command
    commands.push_back(
        dpp::slashcommand("languages", "List all supported languages", bot_.me.id)
    );

    // /autotranslate command
    commands.push_back(
        dpp::slashcommand("autotranslate", "Enable/disable auto-translation", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "languages", "Target languages (comma-separated)", true))
            .add_option(dpp::command_option(dpp::co_boolean, "enable", "Enable or disable", true))
            .set_default_permissions(dpp::p_manage_guild)
    );

    return commands;
}

void TranslationModule::handle_command(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    if (cmd == "translate") {
        cmd_translate(event);
    } else if (cmd == "detectlanguage") {
        cmd_detect_language(event);
    } else if (cmd == "languages") {
        cmd_languages(event);
    } else if (cmd == "autotranslate") {
        cmd_auto_translate(event);
    }
}

void TranslationModule::handle_message(const dpp::message_create_t& event) {
    if (event.msg.author.is_bot() || event.msg.content.empty()) {
        return;
    }

    // Check for URL-only messages
    std::regex url_pattern("^https?://\\S+$", std::regex::icase);
    if (std::regex_match(event.msg.content, url_pattern)) {
        return;
    }

    std::string cleaned = string_utils::clean_text_for_detection(event.msg.content);
    if (cleaned.empty()) {
        return;
    }

    // Check if auto-translate is enabled for this channel
    auto channel_settings = get_database().get_auto_translate_channel(event.msg.channel_id);
    if (!channel_settings || channel_settings->target_languages.empty()) {
        return;
    }

    std::vector<std::string> target_langs = channel_settings->target_languages;

    // Process in thread pool
    get_thread_pool().enqueue([this, event, cleaned, target_langs]() {
        std::string source_lang = detect_language(cleaned);

        std::string description;
        bool has_translations = false;

        for (const auto& target_lang : target_langs) {
            // Skip if same language
            std::string source_base = source_lang.substr(0, 2);
            std::string target_base = target_lang.substr(0, 2);

            if (source_lang == target_lang || source_base == target_base) {
                continue;
            }

            std::string translated = translate_text(cleaned, source_lang, target_lang);
            if (!translated.empty()) {
                std::string flag = LANGUAGE_FLAGS.count(target_lang) ? LANGUAGE_FLAGS.at(target_lang) : "🌐";
                std::string upper_code = string_utils::to_upper(target_lang);

                description += flag + " **" + upper_code + ":** " + string_utils::truncate(translated, 500) + "\n";
                has_translations = true;
            }
        }

        if (has_translations) {
            dpp::embed embed = dpp::embed()
                .set_description(description)
                .set_color(dpp::colors::blue)
                .set_footer(dpp::embed_footer().set_text("🌐 Auto-translate"));

            bot_.message_create(dpp::message(event.msg.channel_id, "").add_embed(embed).set_reference(event.msg.id));
        }
    });
}

std::string TranslationModule::detect_language(const std::string& text) {
    std::string cleaned = string_utils::clean_text_for_detection(text);

    // Check for Chinese characters
    std::regex chinese_pattern("[\\u4e00-\\u9fff]");
    std::smatch matches;
    std::string::const_iterator searchStart(cleaned.cbegin());
    int chinese_count = 0;
    while (std::regex_search(searchStart, cleaned.cend(), matches, chinese_pattern)) {
        chinese_count++;
        searchStart = matches.suffix().first;
    }

    if (chinese_count > 0) {
        int total_chars = std::count_if(cleaned.begin(), cleaned.end(), [](unsigned char c) {
            return !std::isspace(c);
        });
        if (total_chars > 0 && static_cast<float>(chinese_count) / total_chars > 0.3f) {
            return "zh-CN";
        }
    }

    // Use Google Translate API for detection
    std::string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=en&dt=t&q=" +
                      string_utils::url_encode(cleaned);

    auto response = CurlHelper::get(url);
    if (!response.success) {
        return "en";
    }

    try {
        auto j = json::parse(response.body);
        if (j.is_array() && j.size() > 2 && j[2].is_string()) {
            return j[2].get<std::string>();
        }
    } catch (...) {
        // Default to English on error
    }

    return "en";
}

std::string TranslationModule::translate_text(const std::string& text, const std::string& source_lang, const std::string& target_lang) {
    std::string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=" +
                      source_lang + "&tl=" + target_lang + "&dt=t&q=" + string_utils::url_encode(text);

    auto response = CurlHelper::get(url);
    if (!response.success) {
        return "";
    }

    try {
        auto j = json::parse(response.body);
        if (j.is_array() && !j.empty() && j[0].is_array()) {
            std::string result;
            for (const auto& segment : j[0]) {
                if (segment.is_array() && !segment.empty() && segment[0].is_string()) {
                    result += segment[0].get<std::string>();
                }
            }
            return result;
        }
    } catch (const std::exception& e) {
        std::cerr << "Translation parse error: " << e.what() << std::endl;
    }

    return "";
}

std::string TranslationModule::get_language_code(const std::string& lang_input) {
    std::string lower = string_utils::to_lower(string_utils::trim(lang_input));

    if (lower.length() == 2 || lower == "zh-cn") {
        return lower;
    }

    auto it = LANGUAGE_NAMES.find(lower);
    if (it != LANGUAGE_NAMES.end()) {
        return it->second;
    }

    return "";
}

void TranslationModule::set_auto_translate(dpp::snowflake channel_id, dpp::snowflake guild_id, const std::vector<std::string>& languages) {
    Database::AutoTranslateChannel channel;
    channel.channel_id = channel_id;
    channel.guild_id = guild_id;
    channel.target_languages = languages;
    get_database().set_auto_translate_channel(channel);
}

void TranslationModule::disable_auto_translate(dpp::snowflake channel_id) {
    get_database().remove_auto_translate_channel(channel_id);
}

std::vector<std::string> TranslationModule::get_auto_translate_languages(dpp::snowflake channel_id) {
    auto settings = get_database().get_auto_translate_channel(channel_id);
    return settings ? settings->target_languages : std::vector<std::string>{};
}

// Command handlers

void TranslationModule::cmd_translate(const dpp::slashcommand_t& event) {
    event.thinking();

    std::string text = std::get<std::string>(event.get_parameter("text"));
    std::string target_lang = std::get<std::string>(event.get_parameter("target_language"));

    std::string target_code = get_language_code(target_lang);
    if (target_code.empty()) {
        event.edit_response(error_embed("Invalid Language", "Unknown language: `" + target_lang + "`"));
        return;
    }

    get_thread_pool().enqueue([this, event, text, target_code]() {
        std::string source_lang = detect_language(text);
        std::string translated = translate_text(text, source_lang, target_code);

        if (translated.empty()) {
            event.edit_response(error_embed("Translation Error", "Failed to translate the text."));
            return;
        }

        dpp::embed embed = dpp::embed()
            .set_title("🌐 Translation")
            .set_color(dpp::colors::blue)
            .add_field("Original (" + source_lang + ")", string_utils::truncate(text, 1024))
            .add_field("Translation (" + target_code + ")", string_utils::truncate(translated, 1024))
            .set_footer(dpp::embed_footer().set_text("Requested by " + event.command.get_issuing_user().username));

        event.edit_response(dpp::message().add_embed(embed));
    });
}

void TranslationModule::cmd_detect_language(const dpp::slashcommand_t& event) {
    std::string text = std::get<std::string>(event.get_parameter("text"));

    get_thread_pool().enqueue([this, event, text]() {
        std::string detected = detect_language(text);

        std::string lang_name = detected;
        for (const auto& [name, code] : LANGUAGE_NAMES) {
            if (code == detected) {
                lang_name = name;
                lang_name[0] = std::toupper(lang_name[0]);
                break;
            }
        }

        dpp::embed embed = dpp::embed()
            .set_title("🔍 Language Detection")
            .set_color(dpp::colors::purple)
            .add_field("Text", string_utils::truncate(text, 1024))
            .add_field("Detected Language", lang_name + " (" + detected + ")");

        event.reply(dpp::message().add_embed(embed));
    });
}

void TranslationModule::cmd_languages(const dpp::slashcommand_t& event) {
    std::string lang_list;
    for (const auto& [name, code] : LANGUAGE_NAMES) {
        std::string display_name = name;
        display_name[0] = std::toupper(display_name[0]);
        lang_list += "**" + display_name + "**: `" + code + "`\n";
    }

    dpp::embed embed = dpp::embed()
        .set_title("🌍 Supported Languages")
        .set_description(lang_list)
        .set_color(dpp::colors::gold)
        .set_footer(dpp::embed_footer().set_text("Use language names or codes in commands"));

    event.reply(dpp::message().add_embed(embed));
}

void TranslationModule::cmd_auto_translate(const dpp::slashcommand_t& event) {
    std::string languages = std::get<std::string>(event.get_parameter("languages"));
    bool enable = std::get<bool>(event.get_parameter("enable"));

    dpp::snowflake channel_id = event.command.channel_id;
    dpp::snowflake guild_id = event.command.guild_id;

    if (enable) {
        std::vector<std::string> target_codes;
        auto parts = string_utils::split(languages, ',');

        for (auto& lang : parts) {
            lang = string_utils::trim(lang);
            std::string code = get_language_code(lang);
            if (code.empty()) {
                event.reply(error_embed("Invalid Language", "Unknown language: `" + lang + "`"));
                return;
            }
            target_codes.push_back(code);
        }

        if (target_codes.empty()) {
            event.reply(error_embed("Error", "No valid target languages specified"));
            return;
        }

        set_auto_translate(channel_id, guild_id, target_codes);

        std::string lang_display = string_utils::join(target_codes, ", ");
        event.reply(success_embed("Auto-Translation Enabled",
            "Auto-translation enabled for this channel\n🌐 Target languages: **" + lang_display + "**"));
    } else {
        disable_auto_translate(channel_id);
        event.reply(success_embed("Auto-Translation Disabled",
            "Auto-translation disabled for this channel"));
    }
}

} // namespace bot
