#include "utils/curl_helper.hpp"
#include "utils/thread_pool.hpp"
#include <sstream>

namespace bot {

void CurlHelper::global_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void CurlHelper::global_cleanup() {
    curl_global_cleanup();
}

size_t CurlHelper::write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

size_t CurlHelper::header_callback(char* buffer, size_t size, size_t nitems, std::map<std::string, std::string>* headers) {
    size_t total_size = size * nitems;
    std::string header(buffer, total_size);

    size_t colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(key);
        trim(value);

        (*headers)[key] = value;
    }

    return total_size;
}

CurlHelper::Response CurlHelper::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    Response response;
    response.success = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL";
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (compatible; DiscordBot/1.0)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // Set custom headers
    struct curl_slist* header_list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header_str = key + ": " + value;
        header_list = curl_slist_append(header_list, header_str.c_str());
    }
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        response.success = true;
    } else {
        response.error = curl_easy_strerror(res);
    }

    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);

    return response;
}

CurlHelper::Response CurlHelper::post(const std::string& url, const std::string& body,
                                       const std::map<std::string, std::string>& headers) {
    Response response;
    response.success = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL";
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (compatible; DiscordBot/1.0)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    struct curl_slist* header_list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header_str = key + ": " + value;
        header_list = curl_slist_append(header_list, header_str.c_str());
    }
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        response.success = true;
    } else {
        response.error = curl_easy_strerror(res);
    }

    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);

    return response;
}

CurlHelper::Response CurlHelper::post_json(const std::string& url, const std::string& json_body,
                                            const std::map<std::string, std::string>& headers) {
    auto combined_headers = headers;
    combined_headers["Content-Type"] = "application/json";
    return post(url, json_body, combined_headers);
}

void CurlHelper::get_async(const std::string& url, std::function<void(Response)> callback,
                           const std::map<std::string, std::string>& headers) {
    get_thread_pool().enqueue([url, callback, headers]() {
        auto response = get(url, headers);
        callback(response);
    });
}

} // namespace bot
