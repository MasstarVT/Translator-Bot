#pragma once

#include <string>
#include <map>
#include <optional>
#include <functional>
#include <curl/curl.h>

namespace bot {

class CurlHelper {
public:
    struct Response {
        long status_code;
        std::string body;
        std::map<std::string, std::string> headers;
        bool success;
        std::string error;
    };

    // Initialize/cleanup curl globally
    static void global_init();
    static void global_cleanup();

    // HTTP methods
    static Response get(const std::string& url,
                       const std::map<std::string, std::string>& headers = {});

    static Response post(const std::string& url,
                        const std::string& body,
                        const std::map<std::string, std::string>& headers = {});

    static Response post_json(const std::string& url,
                             const std::string& json_body,
                             const std::map<std::string, std::string>& headers = {});

    // Async execution with callback
    static void get_async(const std::string& url,
                         std::function<void(Response)> callback,
                         const std::map<std::string, std::string>& headers = {});

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, std::map<std::string, std::string>* headers);
};

} // namespace bot
