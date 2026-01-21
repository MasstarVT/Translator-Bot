#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace bot {

struct Track {
    std::string url;
    std::string title;
    std::string author;
    int duration;  // seconds
    std::string thumbnail;
    dpp::snowflake requested_by;
};

struct GuildMusicState {
    std::queue<Track> queue;
    std::optional<Track> current_track;
    bool is_playing = false;
    bool is_paused = false;
    int volume = 100;
    enum class LoopMode { Off, Song, Queue } loop_mode = LoopMode::Off;
    dpp::snowflake voice_channel_id = 0;
    dpp::snowflake text_channel_id = 0;
    std::atomic<bool> should_stop{false};
    std::thread audio_thread;
    std::mutex queue_mutex;
};

class MusicModule {
public:
    MusicModule(dpp::cluster& bot);
    ~MusicModule();

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Handle voice state updates
    void handle_voice_state(const dpp::voice_state_update_t& event);

    // Get guild music state
    GuildMusicState* get_state(dpp::snowflake guild_id);

private:
    dpp::cluster& bot_;
    std::map<dpp::snowflake, std::unique_ptr<GuildMusicState>> guild_states_;
    std::mutex states_mutex_;

    // Command handlers
    void cmd_play(const dpp::slashcommand_t& event);
    void cmd_pause(const dpp::slashcommand_t& event);
    void cmd_resume(const dpp::slashcommand_t& event);
    void cmd_skip(const dpp::slashcommand_t& event);
    void cmd_stop(const dpp::slashcommand_t& event);
    void cmd_queue(const dpp::slashcommand_t& event);
    void cmd_nowplaying(const dpp::slashcommand_t& event);
    void cmd_volume(const dpp::slashcommand_t& event);
    void cmd_shuffle(const dpp::slashcommand_t& event);
    void cmd_loop(const dpp::slashcommand_t& event);
    void cmd_remove(const dpp::slashcommand_t& event);
    void cmd_seek(const dpp::slashcommand_t& event);
    void cmd_join(const dpp::slashcommand_t& event);
    void cmd_leave(const dpp::slashcommand_t& event);
    void cmd_playlist(const dpp::slashcommand_t& event);

    // Audio handling
    void join_voice_channel(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake text_channel_id);
    void leave_voice_channel(dpp::snowflake guild_id);
    void play_next(dpp::snowflake guild_id);
    void stream_audio(dpp::snowflake guild_id, const Track& track);
    void stop_audio(dpp::snowflake guild_id);

    // YouTube/yt-dlp integration
    std::optional<Track> get_track_info(const std::string& query);
    std::vector<Track> search_youtube(const std::string& query, int max_results = 1);
    std::string get_audio_url(const std::string& video_url);

    // Permission checks
    bool check_voice_channel(const dpp::slashcommand_t& event);
    bool check_same_channel(const dpp::slashcommand_t& event);
    bool check_dj_permissions(const dpp::slashcommand_t& event);

    // Helpers
    std::string format_duration(int seconds);
    GuildMusicState& get_or_create_state(dpp::snowflake guild_id);
};

} // namespace bot
