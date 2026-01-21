#include <dpp/dpp.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <regex>
#include <thread>
#include <mutex>
#include <future>

using json = nlohmann::json;

// Language code mapping
std::map<std::string, std::string> LANGUAGE_NAMES = {
    {"english", "en"}, {"spanish", "es"}, {"french", "fr"}, {"german", "de"},
    {"italian", "it"}, {"portuguese", "pt"}, {"russian", "ru"}, {"japanese", "ja"},
    {"korean", "ko"}, {"chinese", "zh-CN"}, {"arabic", "ar"}, {"hindi", "hi"},
    {"dutch", "nl"}, {"polish", "pl"}, {"turkish", "tr"}, {"vietnamese", "vi"},
    {"thai", "th"}, {"swedish", "sv"}, {"norwegian", "no"}, {"danish", "da"},
    {"finnish", "fi"}, {"greek", "el"}, {"czech", "cs"}, {"romanian", "ro"},
    {"hungarian", "hu"}, {"hebrew", "iw"}, {"indonesian", "id"}, {"malay", "ms"},
    {"filipino", "tl"}, {"ukrainian", "uk"}, {"bengali", "bn"}, {"tamil", "ta"}
};

std::map<std::string, std::string> LANGUAGE_FLAGS = {
    {"en", "🇬🇧"}, {"es", "🇪🇸"}, {"fr", "🇫🇷"}, {"de", "🇩🇪"},
    {"it", "🇮🇹"}, {"pt", "🇵🇹"}, {"ru", "🇷🇺"}, {"ja", "🇯🇵"},
    {"ko", "🇰🇷"}, {"zh-CN", "🇨🇳"}, {"ar", "🇸🇦"}, {"hi", "🇮🇳"},
    {"nl", "🇳🇱"}, {"pl", "🇵🇱"}, {"tr", "🇹🇷"}, {"vi", "🇻🇳"},
    {"th", "🇹🇭"}, {"sv", "🇸🇪"}, {"no", "🇳🇴"}, {"da", "🇩🇰"},
    {"fi", "🇫🇮"}, {"el", "🇬🇷"}, {"cs", "🇨🇿"}, {"ro", "🇷🇴"},
    {"hu", "🇭🇺"}, {"iw", "🇮🇱"}, {"id", "🇮🇩"}, {"ms", "🇲🇾"},
    {"tl", "🇵🇭"}, {"uk", "🇺🇦"}, {"bn", "🇧🇩"}, {"ta", "🇮🇳"}
};


// Settings file
const std::string SETTINGS_FILE = "bot_settings.json";

// Auto-translate storage
std::map<dpp::snowflake, std::vector<std::string>> auto_translate_channels;
std::map<dpp::snowflake, std::vector<std::string>> auto_translate_servers;
std::map<dpp::snowflake, dpp::snowflake> translation_messages;
std::mutex settings_mutex;

// Curl write callback
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// URL encode function
std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

// Clean text for detection (remove emojis)
std::string clean_text_for_detection(const std::string& text) {
    std::regex emoji_pattern(
        "[\\U0001F600-\\U0001F64F\\U0001F300-\\U0001F5FF\\U0001F680-\\U0001F6FF"
        "\\U0001F1E0-\\U0001F1FF\\U00002500-\\U00002BEF\\U00002702-\\U000027B0"
        "\\U000024C2-\\U0001F251\\U0001f926-\\U0001f937\\U00010000-\\U0010ffff"
        "\\u2640-\\u2642\\u2600-\\u2B55\\u200d\\u23cf\\u23e9\\u231a\\ufe0f\\u3030]+");

    std::string cleaned = std::regex_replace(text, emoji_pattern, "");

    // Remove Discord custom emojis
    std::regex discord_emoji("<a?:[a-zA-Z0-9_]+:[0-9]+>");
    cleaned = std::regex_replace(cleaned, discord_emoji, "");

    return cleaned.empty() ? text : cleaned;
}

// Get language code
std::string get_language_code(const std::string& lang_input) {
    std::string lower = lang_input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Trim whitespace
    lower.erase(lower.begin(), std::find_if(lower.begin(), lower.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    lower.erase(std::find_if(lower.rbegin(), lower.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), lower.end());

    if (lower.length() == 2 || lower == "zh-cn") {
        return lower;
    }

    auto it = LANGUAGE_NAMES.find(lower);
    if (it != LANGUAGE_NAMES.end()) {
        return it->second;
    }

    return "";
}

// Detect language using Google Translate API
std::string detect_language(const std::string& text) {
    std::string cleaned = clean_text_for_detection(text);

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
        if (total_chars > 0 && (float)chinese_count / total_chars > 0.3f) {
            return "zh-CN";
        }
    }

    // Use Google Translate API for detection
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "en";
    }

    std::string response;
    std::string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=en&dt=t&q=" + url_encode(cleaned);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "en";
    }

    try {
        auto j = json::parse(response);
        if (j.is_array() && j.size() > 2 && j[2].is_string()) {
            return j[2].get<std::string>();
        }
    } catch (...) {
        // Default to English on error
    }

    return "en";
}

// Translate text using Google Translate API
std::string translate_text(const std::string& text, const std::string& source_lang, const std::string& target_lang) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    std::string response;
    std::string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=" +
                      source_lang + "&tl=" + target_lang + "&dt=t&q=" + url_encode(text);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "";
    }

    try {
        auto j = json::parse(response);
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

// Load settings
void load_settings() {
    std::lock_guard<std::mutex> lock(settings_mutex);

    std::ifstream file(SETTINGS_FILE);
    if (!file.is_open()) {
        return;
    }

    try {
        json data;
        file >> data;

        if (data.contains("auto_translate_channels")) {
            for (auto& [key, value] : data["auto_translate_channels"].items()) {
                dpp::snowflake channel_id = std::stoull(key);
                std::vector<std::string> langs;

                if (value.is_string()) {
                    langs.push_back(value.get<std::string>());
                } else if (value.is_array()) {
                    for (const auto& lang : value) {
                        langs.push_back(lang.get<std::string>());
                    }
                }

                auto_translate_channels[channel_id] = langs;
            }
        }

        if (data.contains("auto_translate_servers")) {
            for (auto& [key, value] : data["auto_translate_servers"].items()) {
                dpp::snowflake server_id = std::stoull(key);
                std::vector<std::string> langs;

                if (value.is_string()) {
                    langs.push_back(value.get<std::string>());
                } else if (value.is_array()) {
                    for (const auto& lang : value) {
                        langs.push_back(lang.get<std::string>());
                    }
                }

                auto_translate_servers[server_id] = langs;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading settings: " << e.what() << std::endl;
    }
}

// Save settings
void save_settings() {
    std::lock_guard<std::mutex> lock(settings_mutex);

    json data;
    data["auto_translate_channels"] = json::object();
    data["auto_translate_servers"] = json::object();

    for (const auto& [channel_id, langs] : auto_translate_channels) {
        data["auto_translate_channels"][std::to_string(channel_id)] = langs;
    }

    for (const auto& [server_id, langs] : auto_translate_servers) {
        data["auto_translate_servers"][std::to_string(server_id)] = langs;
    }

    std::ofstream file(SETTINGS_FILE);
    if (file.is_open()) {
        file << data.dump(2);
    }
}

int main() {
    // Load environment variables
    std::string token;
    std::ifstream env_file(".env");
    if (env_file.is_open()) {
        std::string line;
        while (std::getline(env_file, line)) {
            if (line.find("DISCORD_BOT_TOKEN=") == 0) {
                token = line.substr(18);
                break;
            }
        }
    }

    if (token.empty()) {
        std::cerr << "ERROR: DISCORD_BOT_TOKEN not found in .env file!" << std::endl;
        return 1;
    }

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Load settings
    load_settings();

    // Create bot
    dpp::cluster bot(token, dpp::i_default_intents | dpp::i_message_content);

    bot.on_log(dpp::utility::cout_logger());

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::cout << bot.me.username << " has connected to Discord!" << std::endl;
            std::cout << "Bot ID: " << bot.me.id << std::endl;

            // Register slash commands
            std::vector<dpp::slashcommand> commands = {
                dpp::slashcommand("translate", "Translate text to a target language", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_string, "text", "The text to translate", true))
                    .add_option(dpp::command_option(dpp::co_string, "target_language", "Target language", true)),

                dpp::slashcommand("detectlanguage", "Detect the language of text", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_string, "text", "The text to analyze", true)),

                dpp::slashcommand("languages", "List all supported languages", bot.me.id),

                dpp::slashcommand("autotranslate", "Enable/disable auto-translation", bot.me.id)
                    .add_option(dpp::command_option(dpp::co_string, "languages", "Target languages (comma-separated)", true))
                    .add_option(dpp::command_option(dpp::co_boolean, "enable", "Enable or disable", true))
                    .set_default_permissions(dpp::p_manage_guild)
            };

            bot.global_bulk_command_create(commands);
            std::cout << "Slash commands registered!" << std::endl;
        }
    });

    // Handle slash commands
    bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "translate") {
            event.thinking();

            std::string text = std::get<std::string>(event.get_parameter("text"));
            std::string target_lang = std::get<std::string>(event.get_parameter("target_language"));

            std::string target_code = get_language_code(target_lang);
            if (target_code.empty()) {
                event.edit_response("Invalid language: `" + target_lang + "`");
                return;
            }

            // Run translation in separate thread to avoid blocking
            std::thread([&bot, event, text, target_code]() mutable {
                std::string source_lang = detect_language(text);
                std::string translated = translate_text(text, source_lang, target_code);

                if (translated.empty()) {
                    event.edit_response("Translation error occurred");
                    return;
                }

                dpp::embed embed = dpp::embed()
                    .set_title("🌐 Translation")
                    .set_color(dpp::colors::blue)
                    .add_field("Original (" + source_lang + ")", text.substr(0, 1024))
                    .add_field("Translation (" + target_code + ")", translated.substr(0, 1024))
                    .set_footer("Requested by " + event.command.get_issuing_user().username, "");

                event.edit_response(dpp::message().add_embed(embed));
            }).detach();
        }
        else if (event.command.get_command_name() == "detectlanguage") {
            std::string text = std::get<std::string>(event.get_parameter("text"));

            std::thread([event, text]() mutable {
                std::string detected = detect_language(text);

                std::string lang_name = detected;
                for (const auto& [name, code] : LANGUAGE_NAMES) {
                    if (code == detected) {
                        lang_name = name;
                        break;
                    }
                }

                dpp::embed embed = dpp::embed()
                    .set_title("🔍 Language Detection")
                    .set_color(dpp::colors::purple)
                    .add_field("Text", text.substr(0, 1024))
                    .add_field("Detected Language", lang_name + " (" + detected + ")");

                event.reply(dpp::message().add_embed(embed));
            }).detach();
        }
        else if (event.command.get_command_name() == "languages") {
            std::string lang_list;
            for (const auto& [name, code] : LANGUAGE_NAMES) {
                lang_list += "**" + name + "**: `" + code + "`\n";
            }

            dpp::embed embed = dpp::embed()
                .set_title("🌍 Supported Languages")
                .set_description(lang_list)
                .set_color(dpp::colors::gold)
                .set_footer("Use language names or codes in commands", "");

            event.reply(dpp::message().add_embed(embed));
        }
        else if (event.command.get_command_name() == "autotranslate") {
            std::string languages = std::get<std::string>(event.get_parameter("languages"));
            bool enable = std::get<bool>(event.get_parameter("enable"));

            dpp::snowflake channel_id = event.command.channel_id;

            if (enable) {
                std::vector<std::string> target_codes;
                std::istringstream ss(languages);
                std::string lang;

                while (std::getline(ss, lang, ',')) {
                    // Trim whitespace
                    lang.erase(lang.begin(), std::find_if(lang.begin(), lang.end(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }));
                    lang.erase(std::find_if(lang.rbegin(), lang.rend(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }).base(), lang.end());

                    std::string code = get_language_code(lang);
                    if (code.empty()) {
                        event.reply("Invalid language: `" + lang + "`");
                        return;
                    }
                    target_codes.push_back(code);
                }

                if (target_codes.empty()) {
                    event.reply("No valid target languages specified");
                    return;
                }

                auto_translate_channels[channel_id] = target_codes;
                save_settings();

                std::string lang_display;
                for (const auto& code : target_codes) {
                    lang_display += "**" + code + "**, ";
                }
                if (!lang_display.empty()) {
                    lang_display = lang_display.substr(0, lang_display.length() - 2);
                }

                event.reply("✅ Auto-translation enabled for this channel\n🌐 Target languages: " + lang_display);
            } else {
                auto it = auto_translate_channels.find(channel_id);
                if (it != auto_translate_channels.end()) {
                    auto_translate_channels.erase(it);
                    save_settings();
                }
                event.reply("✅ Auto-translation disabled for this channel");
            }
        }
    });

    // Handle messages for auto-translation
    bot.on_message_create([&bot](const dpp::message_create_t& event) {
        if (event.msg.author.is_bot()) {
            return;
        }

        if (event.msg.content.empty()) {
            return;
        }

        // Check for URL/link only messages
        std::regex url_pattern("^https?://\\S+$", std::regex::icase);
        if (std::regex_match(event.msg.content, url_pattern)) {
            return;
        }

        std::string cleaned = clean_text_for_detection(event.msg.content);
        if (cleaned.empty()) {
            return;
        }

        // Check if auto-translate is enabled
        std::vector<std::string> target_langs;

        auto channel_it = auto_translate_channels.find(event.msg.channel_id);
        if (channel_it != auto_translate_channels.end()) {
            target_langs = channel_it->second;
        } else if (event.msg.guild_id) {
            auto server_it = auto_translate_servers.find(event.msg.guild_id);
            if (server_it != auto_translate_servers.end()) {
                target_langs = server_it->second;
            }
        }

        if (!target_langs.empty()) {
            // Process auto-translation in a separate thread
            std::thread([&bot, event, cleaned, target_langs]() {
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
                        std::string flag = LANGUAGE_FLAGS.count(target_lang) ? LANGUAGE_FLAGS[target_lang] : "🌐";
                        std::string upper_code = target_lang;
                        std::transform(upper_code.begin(), upper_code.end(), upper_code.begin(), ::toupper);

                        description += flag + " **" + upper_code + ":** " + translated.substr(0, 500) + "\n";
                        has_translations = true;
                    }
                }

                if (has_translations) {
                    dpp::embed embed = dpp::embed()
                        .set_description(description)
                        .set_color(dpp::colors::blue)
                        .set_footer("🌐 Auto-translate", "");

                    bot.message_create(dpp::message(event.msg.channel_id, "").add_embed(embed).set_reference(event.msg.id));
                }
            }).detach();
        }
    });

    // Start the bot
    bot.start(dpp::st_wait);

    curl_global_cleanup();
    return 0;
}
