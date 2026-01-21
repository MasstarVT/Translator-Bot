#include "modules/music.hpp"
#include "database.hpp"
#include "utils/common.hpp"
#include "utils/thread_pool.hpp"
#include "utils/string_utils.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <cstdio>
#include <random>
#include <algorithm>
#include <iomanip>

namespace bot {

using json = nlohmann::json;

MusicModule::MusicModule(dpp::cluster& bot) : bot_(bot) {}

MusicModule::~MusicModule() {
    // Stop all audio threads
    std::lock_guard<std::mutex> lock(states_mutex_);
    for (auto& [guild_id, state] : guild_states_) {
        state->should_stop = true;
        if (state->audio_thread.joinable()) {
            state->audio_thread.join();
        }
    }
}

std::vector<dpp::slashcommand> MusicModule::get_commands() {
    std::vector<dpp::slashcommand> commands;

    commands.push_back(
        dpp::slashcommand("play", "Play a song or add to queue", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "query", "Song name or YouTube URL", true))
    );

    commands.push_back(dpp::slashcommand("pause", "Pause playback", bot_.me.id));
    commands.push_back(dpp::slashcommand("resume", "Resume playback", bot_.me.id));

    commands.push_back(
        dpp::slashcommand("skip", "Skip current song", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_integer, "amount", "Number of songs to skip", false)
                .set_min_value(1))
    );

    commands.push_back(dpp::slashcommand("stop", "Stop playback and clear queue", bot_.me.id));

    commands.push_back(
        dpp::slashcommand("queue", "View the queue", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_integer, "page", "Page number", false)
                .set_min_value(1))
    );

    commands.push_back(dpp::slashcommand("nowplaying", "Show current song", bot_.me.id));

    commands.push_back(
        dpp::slashcommand("volume", "Set volume", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_integer, "level", "Volume level (0-100)", true)
                .set_min_value(0).set_max_value(100))
    );

    commands.push_back(dpp::slashcommand("shuffle", "Shuffle the queue", bot_.me.id));

    commands.push_back(
        dpp::slashcommand("loop", "Set loop mode", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "mode", "Loop mode", true)
                .add_choice(dpp::command_option_choice("Off", std::string("off")))
                .add_choice(dpp::command_option_choice("Song", std::string("song")))
                .add_choice(dpp::command_option_choice("Queue", std::string("queue"))))
    );

    commands.push_back(
        dpp::slashcommand("remove", "Remove a song from queue", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_integer, "position", "Position in queue", true)
                .set_min_value(1))
    );

    commands.push_back(
        dpp::slashcommand("seek", "Seek to position", bot_.me.id)
            .add_option(dpp::command_option(dpp::co_string, "time", "Time (e.g., 1:30, 90)", true))
    );

    commands.push_back(dpp::slashcommand("join", "Join your voice channel", bot_.me.id));
    commands.push_back(dpp::slashcommand("leave", "Leave voice channel", bot_.me.id));

    // Playlist commands
    dpp::slashcommand playlist("playlist", "Manage playlists", bot_.me.id);

    playlist.add_option(
        dpp::command_option(dpp::co_sub_command, "save", "Save current queue as playlist")
            .add_option(dpp::command_option(dpp::co_string, "name", "Playlist name", true))
    );

    playlist.add_option(
        dpp::command_option(dpp::co_sub_command, "load", "Load a playlist")
            .add_option(dpp::command_option(dpp::co_string, "name", "Playlist name", true))
    );

    playlist.add_option(
        dpp::command_option(dpp::co_sub_command, "list", "List your playlists")
    );

    playlist.add_option(
        dpp::command_option(dpp::co_sub_command, "delete", "Delete a playlist")
            .add_option(dpp::command_option(dpp::co_string, "name", "Playlist name", true))
    );

    commands.push_back(playlist);

    return commands;
}

void MusicModule::handle_command(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    if (cmd == "play") cmd_play(event);
    else if (cmd == "pause") cmd_pause(event);
    else if (cmd == "resume") cmd_resume(event);
    else if (cmd == "skip") cmd_skip(event);
    else if (cmd == "stop") cmd_stop(event);
    else if (cmd == "queue") cmd_queue(event);
    else if (cmd == "nowplaying") cmd_nowplaying(event);
    else if (cmd == "volume") cmd_volume(event);
    else if (cmd == "shuffle") cmd_shuffle(event);
    else if (cmd == "loop") cmd_loop(event);
    else if (cmd == "remove") cmd_remove(event);
    else if (cmd == "seek") cmd_seek(event);
    else if (cmd == "join") cmd_join(event);
    else if (cmd == "leave") cmd_leave(event);
    else if (cmd == "playlist") cmd_playlist(event);
}

void MusicModule::handle_voice_state(const dpp::voice_state_update_t& event) {
    // Check if bot was disconnected
    if (event.state.user_id == bot_.me.id && event.state.channel_id == 0) {
        auto* state = get_state(event.state.guild_id);
        if (state) {
            state->should_stop = true;
            state->is_playing = false;
        }
    }
}

GuildMusicState* MusicModule::get_state(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(states_mutex_);
    auto it = guild_states_.find(guild_id);
    return it != guild_states_.end() ? it->second.get() : nullptr;
}

GuildMusicState& MusicModule::get_or_create_state(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(states_mutex_);
    auto it = guild_states_.find(guild_id);
    if (it == guild_states_.end()) {
        guild_states_[guild_id] = std::make_unique<GuildMusicState>();
    }
    return *guild_states_[guild_id];
}

bool MusicModule::check_voice_channel(const dpp::slashcommand_t& event) {
    // Get user's voice state
    dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        return false;
    }

    auto vs = guild->voice_members.find(event.command.get_issuing_user().id);
    return vs != guild->voice_members.end() && vs->second.channel_id != 0;
}

bool MusicModule::check_same_channel(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state || state->voice_channel_id == 0) {
        return true;  // Bot not in voice, so any channel is fine
    }

    dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) return false;

    auto vs = guild->voice_members.find(event.command.get_issuing_user().id);
    if (vs == guild->voice_members.end()) return false;

    return vs->second.channel_id == state->voice_channel_id;
}

std::optional<Track> MusicModule::get_track_info(const std::string& query) {
    std::string cmd;
    if (query.find("youtube.com") != std::string::npos || query.find("youtu.be") != std::string::npos) {
        cmd = "yt-dlp --print-json --no-playlist \"" + query + "\" 2>/dev/null";
    } else {
        cmd = "yt-dlp --print-json --no-playlist \"ytsearch:" + query + "\" 2>/dev/null";
    }

    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    if (result.empty()) {
        return std::nullopt;
    }

    try {
        auto j = json::parse(result);

        Track track;
        track.url = j.value("webpage_url", j.value("url", ""));
        track.title = j.value("title", "Unknown");
        track.author = j.value("uploader", "Unknown");
        track.duration = j.value("duration", 0);
        track.thumbnail = j.value("thumbnail", "");

        return track;
    } catch (...) {
        return std::nullopt;
    }
}

std::string MusicModule::get_audio_url(const std::string& video_url) {
    std::string cmd = "yt-dlp -f bestaudio -g \"" + video_url + "\" 2>/dev/null";

    std::array<char, 2048> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    // Trim newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

void MusicModule::join_voice_channel(dpp::snowflake guild_id, dpp::snowflake channel_id, dpp::snowflake text_channel_id) {
    auto& state = get_or_create_state(guild_id);
    state.voice_channel_id = channel_id;
    state.text_channel_id = text_channel_id;

    // Note: DPP v10 voice connection requires different approach
    // This is a placeholder - actual voice connection would use
    // the guild->connect_member_voice() method with proper setup
    dpp::guild* g = dpp::find_guild(guild_id);
    if (g) {
        g->connect_member_voice(bot_.me.id);
    }
}

void MusicModule::leave_voice_channel(dpp::snowflake guild_id) {
    auto* state = get_state(guild_id);
    if (state) {
        state->should_stop = true;
        state->voice_channel_id = 0;

        // Clear queue
        std::lock_guard<std::mutex> lock(state->queue_mutex);
        while (!state->queue.empty()) state->queue.pop();
        state->current_track.reset();
        state->is_playing = false;
    }

    // Disconnect voice - in DPP v10, this is handled through the guild
    dpp::guild* g = dpp::find_guild(guild_id);
    if (g) {
        // Voice disconnect is handled automatically when leaving
    }
}

void MusicModule::play_next(dpp::snowflake guild_id) {
    auto* state = get_state(guild_id);
    if (!state) return;

    std::lock_guard<std::mutex> lock(state->queue_mutex);

    // Handle loop mode
    if (state->loop_mode == GuildMusicState::LoopMode::Song && state->current_track) {
        // Re-add current track
        stream_audio(guild_id, *state->current_track);
        return;
    }

    if (state->loop_mode == GuildMusicState::LoopMode::Queue && state->current_track) {
        state->queue.push(*state->current_track);
    }

    if (state->queue.empty()) {
        state->current_track.reset();
        state->is_playing = false;

        // Send "queue ended" message
        if (state->text_channel_id != 0) {
            bot_.message_create(dpp::message(state->text_channel_id, "")
                .add_embed(info_embed("Queue Ended", "No more songs in queue.").embeds[0]));
        }
        return;
    }

    Track next = state->queue.front();
    state->queue.pop();
    state->current_track = next;

    stream_audio(guild_id, next);
}

void MusicModule::stream_audio(dpp::snowflake guild_id, const Track& track) {
    auto* state = get_state(guild_id);
    if (!state) return;

    state->is_playing = true;
    state->is_paused = false;

    // Send now playing message
    if (state->text_channel_id != 0) {
        dpp::embed embed;
        embed.set_title("Now Playing")
             .set_description("**" + track.title + "**")
             .add_field("Duration", format_duration(track.duration), true)
             .add_field("Requested by", "<@" + std::to_string(track.requested_by) + ">", true)
             .set_color(0x00ff00);

        if (!track.thumbnail.empty()) {
            embed.set_thumbnail(track.thumbnail);
        }

        bot_.message_create(dpp::message(state->text_channel_id, "").add_embed(embed));
    }

    // Note: Audio streaming requires DPP v10 voice client setup
    // This is a simplified placeholder that tracks queue state
    // Full implementation would require setting up voice_client callbacks
    get_thread_pool().enqueue([this, guild_id, track]() {
        auto* state = get_state(guild_id);
        if (!state) return;

        // For now, just simulate playback by waiting for track duration
        // Actual implementation would stream audio through DPP voice client
        int remaining = track.duration;
        while (remaining > 0 && !state->should_stop) {
            if (!state->is_paused) {
                remaining--;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (!state->should_stop) {
            play_next(guild_id);
        }
    });
}

void MusicModule::stop_audio(dpp::snowflake guild_id) {
    auto* state = get_state(guild_id);
    if (state) {
        state->should_stop = true;

        std::lock_guard<std::mutex> lock(state->queue_mutex);
        while (!state->queue.empty()) state->queue.pop();
        state->current_track.reset();
        state->is_playing = false;
    }
}

std::string MusicModule::format_duration(int seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << ":" << std::setfill('0') << std::setw(2);
    }
    oss << minutes << ":" << std::setfill('0') << std::setw(2) << secs;

    return oss.str();
}

// Command handlers

void MusicModule::cmd_play(const dpp::slashcommand_t& event) {
    if (!check_voice_channel(event)) {
        event.reply(error_embed("Not in Voice", "You must be in a voice channel."));
        return;
    }

    std::string query = std::get<std::string>(event.get_parameter("query"));

    event.thinking();

    get_thread_pool().enqueue([this, event, query]() {
        auto track = get_track_info(query);
        if (!track) {
            event.edit_response(error_embed("Not Found", "Could not find a track for: " + query));
            return;
        }

        track->requested_by = event.command.get_issuing_user().id;

        auto& state = get_or_create_state(event.command.guild_id);

        // Join voice if not already
        if (state.voice_channel_id == 0) {
            dpp::guild* guild = dpp::find_guild(event.command.guild_id);
            if (guild) {
                auto vs = guild->voice_members.find(event.command.get_issuing_user().id);
                if (vs != guild->voice_members.end()) {
                    join_voice_channel(event.command.guild_id, vs->second.channel_id, event.command.channel_id);
                }
            }
        }

        // Add to queue
        {
            std::lock_guard<std::mutex> lock(state.queue_mutex);
            state.queue.push(*track);
        }

        dpp::embed embed;
        embed.set_title("Added to Queue")
             .set_description("**" + track->title + "**")
             .add_field("Duration", format_duration(track->duration), true)
             .set_color(0x0099ff);

        if (!track->thumbnail.empty()) {
            embed.set_thumbnail(track->thumbnail);
        }

        event.edit_response(dpp::message().add_embed(embed));

        // Start playing if not already
        if (!state.is_playing) {
            state.should_stop = false;
            play_next(event.command.guild_id);
        }
    });
}

void MusicModule::cmd_pause(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state || !state->is_playing) {
        event.reply(error_embed("Nothing Playing", "There's nothing playing right now."));
        return;
    }

    state->is_paused = true;
    event.reply(success_embed("Paused", "Playback paused."));
}

void MusicModule::cmd_resume(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state || !state->is_paused) {
        event.reply(error_embed("Not Paused", "Playback is not paused."));
        return;
    }

    state->is_paused = false;
    event.reply(success_embed("Resumed", "Playback resumed."));
}

void MusicModule::cmd_skip(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state || !state->is_playing) {
        event.reply(error_embed("Nothing Playing", "There's nothing playing right now."));
        return;
    }

    int amount = 1;
    auto amount_param = event.get_parameter("amount");
    if (std::holds_alternative<int64_t>(amount_param)) {
        amount = static_cast<int>(std::get<int64_t>(amount_param));
    }

    // Skip additional songs from queue
    {
        std::lock_guard<std::mutex> lock(state->queue_mutex);
        for (int i = 1; i < amount && !state->queue.empty(); i++) {
            state->queue.pop();
        }
    }

    state->should_stop = true;
    state->is_playing = false;

    event.reply(success_embed("Skipped", "Skipped " + std::to_string(amount) + " song(s)."));

    // Restart playback
    state->should_stop = false;
    play_next(event.command.guild_id);
}

void MusicModule::cmd_stop(const dpp::slashcommand_t& event) {
    stop_audio(event.command.guild_id);
    event.reply(success_embed("Stopped", "Stopped playback and cleared the queue."));
}

void MusicModule::cmd_queue(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state) {
        event.reply(info_embed("Queue", "The queue is empty."));
        return;
    }

    int page = 1;
    auto page_param = event.get_parameter("page");
    if (std::holds_alternative<int64_t>(page_param)) {
        page = static_cast<int>(std::get<int64_t>(page_param));
    }

    std::lock_guard<std::mutex> lock(state->queue_mutex);

    if (state->queue.empty() && !state->current_track) {
        event.reply(info_embed("Queue", "The queue is empty."));
        return;
    }

    std::string description;

    if (state->current_track) {
        description += "**Now Playing:**\n";
        description += "🎵 " + state->current_track->title + " [" + format_duration(state->current_track->duration) + "]\n\n";
    }

    // Copy queue to vector for display
    std::queue<Track> temp = state->queue;
    std::vector<Track> tracks;
    while (!temp.empty()) {
        tracks.push_back(temp.front());
        temp.pop();
    }

    int per_page = 10;
    int start = (page - 1) * per_page;
    int end = std::min(start + per_page, static_cast<int>(tracks.size()));

    if (start < static_cast<int>(tracks.size())) {
        description += "**Up Next:**\n";
        for (int i = start; i < end; i++) {
            description += std::to_string(i + 1) + ". " + tracks[i].title +
                          " [" + format_duration(tracks[i].duration) + "]\n";
        }
    }

    dpp::embed embed;
    embed.set_title("Queue")
         .set_description(description)
         .set_color(0x0099ff)
         .set_footer(dpp::embed_footer().set_text(
             std::to_string(tracks.size()) + " songs in queue | Page " + std::to_string(page)));

    event.reply(dpp::message().add_embed(embed));
}

void MusicModule::cmd_nowplaying(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state || !state->current_track) {
        event.reply(info_embed("Now Playing", "Nothing is playing right now."));
        return;
    }

    const auto& track = *state->current_track;

    dpp::embed embed;
    embed.set_title("Now Playing")
         .set_description("**" + track.title + "**")
         .add_field("Author", track.author, true)
         .add_field("Duration", format_duration(track.duration), true)
         .add_field("Requested by", "<@" + std::to_string(track.requested_by) + ">", true)
         .set_color(0x00ff00);

    if (!track.thumbnail.empty()) {
        embed.set_thumbnail(track.thumbnail);
    }

    // Show loop and volume status
    std::string status;
    if (state->is_paused) status += "⏸️ Paused | ";
    if (state->loop_mode == GuildMusicState::LoopMode::Song) status += "🔂 Loop Song | ";
    else if (state->loop_mode == GuildMusicState::LoopMode::Queue) status += "🔁 Loop Queue | ";
    status += "🔊 " + std::to_string(state->volume) + "%";

    embed.add_field("Status", status, false);

    event.reply(dpp::message().add_embed(embed));
}

void MusicModule::cmd_volume(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state) {
        event.reply(error_embed("Error", "Not connected to voice."));
        return;
    }

    int level = static_cast<int>(std::get<int64_t>(event.get_parameter("level")));
    state->volume = level;

    event.reply(success_embed("Volume Set", "Volume set to " + std::to_string(level) + "%"));
}

void MusicModule::cmd_shuffle(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state) {
        event.reply(error_embed("Error", "The queue is empty."));
        return;
    }

    std::lock_guard<std::mutex> lock(state->queue_mutex);

    if (state->queue.size() < 2) {
        event.reply(error_embed("Error", "Not enough songs to shuffle."));
        return;
    }

    // Convert to vector, shuffle, convert back
    std::vector<Track> tracks;
    while (!state->queue.empty()) {
        tracks.push_back(state->queue.front());
        state->queue.pop();
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(tracks.begin(), tracks.end(), gen);

    for (const auto& track : tracks) {
        state->queue.push(track);
    }

    event.reply(success_embed("Shuffled", "Shuffled " + std::to_string(tracks.size()) + " songs."));
}

void MusicModule::cmd_loop(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state) {
        event.reply(error_embed("Error", "Not connected to voice."));
        return;
    }

    std::string mode = std::get<std::string>(event.get_parameter("mode"));

    if (mode == "off") {
        state->loop_mode = GuildMusicState::LoopMode::Off;
        event.reply(success_embed("Loop Mode", "Loop disabled."));
    } else if (mode == "song") {
        state->loop_mode = GuildMusicState::LoopMode::Song;
        event.reply(success_embed("Loop Mode", "Now looping the current song."));
    } else if (mode == "queue") {
        state->loop_mode = GuildMusicState::LoopMode::Queue;
        event.reply(success_embed("Loop Mode", "Now looping the queue."));
    }
}

void MusicModule::cmd_remove(const dpp::slashcommand_t& event) {
    auto* state = get_state(event.command.guild_id);
    if (!state) {
        event.reply(error_embed("Error", "The queue is empty."));
        return;
    }

    int position = static_cast<int>(std::get<int64_t>(event.get_parameter("position")));

    std::lock_guard<std::mutex> lock(state->queue_mutex);

    if (position < 1 || position > static_cast<int>(state->queue.size())) {
        event.reply(error_embed("Invalid Position", "Position must be between 1 and " + std::to_string(state->queue.size())));
        return;
    }

    // Convert to vector, remove, convert back
    std::vector<Track> tracks;
    while (!state->queue.empty()) {
        tracks.push_back(state->queue.front());
        state->queue.pop();
    }

    std::string removed_title = tracks[position - 1].title;
    tracks.erase(tracks.begin() + position - 1);

    for (const auto& track : tracks) {
        state->queue.push(track);
    }

    event.reply(success_embed("Removed", "Removed **" + removed_title + "** from queue."));
}

void MusicModule::cmd_seek(const dpp::slashcommand_t& event) {
    // Seek functionality requires more complex audio handling
    // This is a placeholder
    event.reply(info_embed("Not Implemented", "Seek functionality is not yet implemented."));
}

void MusicModule::cmd_join(const dpp::slashcommand_t& event) {
    if (!check_voice_channel(event)) {
        event.reply(error_embed("Not in Voice", "You must be in a voice channel."));
        return;
    }

    dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        event.reply(error_embed("Error", "Could not find guild."));
        return;
    }

    auto vs = guild->voice_members.find(event.command.get_issuing_user().id);
    if (vs == guild->voice_members.end()) {
        event.reply(error_embed("Error", "Could not find your voice channel."));
        return;
    }

    join_voice_channel(event.command.guild_id, vs->second.channel_id, event.command.channel_id);
    event.reply(success_embed("Joined", "Joined <#" + std::to_string(vs->second.channel_id) + ">"));
}

void MusicModule::cmd_leave(const dpp::slashcommand_t& event) {
    leave_voice_channel(event.command.guild_id);
    event.reply(success_embed("Left", "Disconnected from voice channel."));
}

void MusicModule::cmd_playlist(const dpp::slashcommand_t& event) {
    auto subcommand = event.command.get_command_interaction().options[0];
    std::string subcmd = subcommand.name;

    if (subcmd == "save") {
        auto* state = get_state(event.command.guild_id);
        if (!state) {
            event.reply(error_embed("Error", "No queue to save."));
            return;
        }

        std::string name;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "name" && std::holds_alternative<std::string>(opt.value)) {
                name = std::get<std::string>(opt.value);
                break;
            }
        }

        Database::Playlist playlist;
        playlist.guild_id = event.command.guild_id;
        playlist.user_id = event.command.get_issuing_user().id;
        playlist.name = name;

        int64_t playlist_id = get_database().create_playlist(playlist);
        if (playlist_id < 0) {
            event.reply(error_embed("Error", "Failed to create playlist. It may already exist."));
            return;
        }

        // Add tracks
        std::lock_guard<std::mutex> lock(state->queue_mutex);
        std::queue<Track> temp = state->queue;
        int pos = 0;

        // Add current track first
        if (state->current_track) {
            Database::PlaylistTrack track;
            track.playlist_id = playlist_id;
            track.url = state->current_track->url;
            track.title = state->current_track->title;
            track.duration = state->current_track->duration;
            track.position = pos++;
            get_database().add_playlist_track(track);
        }

        while (!temp.empty()) {
            Database::PlaylistTrack track;
            track.playlist_id = playlist_id;
            track.url = temp.front().url;
            track.title = temp.front().title;
            track.duration = temp.front().duration;
            track.position = pos++;
            get_database().add_playlist_track(track);
            temp.pop();
        }

        event.reply(success_embed("Playlist Saved", "Saved " + std::to_string(pos) + " tracks as **" + name + "**"));

    } else if (subcmd == "load") {
        std::string name;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "name" && std::holds_alternative<std::string>(opt.value)) {
                name = std::get<std::string>(opt.value);
                break;
            }
        }

        auto playlist = get_database().get_playlist(event.command.get_issuing_user().id, name);
        if (!playlist) {
            event.reply(error_embed("Not Found", "Playlist **" + name + "** not found."));
            return;
        }

        auto tracks = get_database().get_playlist_tracks(playlist->id);
        if (tracks.empty()) {
            event.reply(error_embed("Empty", "Playlist is empty."));
            return;
        }

        auto& state = get_or_create_state(event.command.guild_id);

        // Join voice if needed
        if (state.voice_channel_id == 0) {
            dpp::guild* guild = dpp::find_guild(event.command.guild_id);
            if (guild) {
                auto vs = guild->voice_members.find(event.command.get_issuing_user().id);
                if (vs != guild->voice_members.end()) {
                    join_voice_channel(event.command.guild_id, vs->second.channel_id, event.command.channel_id);
                }
            }
        }

        // Add tracks to queue
        {
            std::lock_guard<std::mutex> lock(state.queue_mutex);
            for (const auto& t : tracks) {
                Track track;
                track.url = t.url;
                track.title = t.title;
                track.duration = t.duration;
                track.requested_by = event.command.get_issuing_user().id;
                state.queue.push(track);
            }
        }

        event.reply(success_embed("Playlist Loaded", "Loaded " + std::to_string(tracks.size()) + " tracks from **" + name + "**"));

        if (!state.is_playing) {
            state.should_stop = false;
            play_next(event.command.guild_id);
        }

    } else if (subcmd == "list") {
        auto playlists = get_database().get_user_playlists(event.command.get_issuing_user().id);

        if (playlists.empty()) {
            event.reply(info_embed("Playlists", "You have no saved playlists."));
            return;
        }

        std::string description;
        for (const auto& p : playlists) {
            auto tracks = get_database().get_playlist_tracks(p.id);
            description += "**" + p.name + "** - " + std::to_string(tracks.size()) + " tracks\n";
        }

        event.reply(info_embed("Your Playlists", description));

    } else if (subcmd == "delete") {
        std::string name;
        for (const auto& opt : subcommand.options) {
            if (opt.name == "name" && std::holds_alternative<std::string>(opt.value)) {
                name = std::get<std::string>(opt.value);
                break;
            }
        }

        auto playlist = get_database().get_playlist(event.command.get_issuing_user().id, name);
        if (!playlist) {
            event.reply(error_embed("Not Found", "Playlist **" + name + "** not found."));
            return;
        }

        get_database().delete_playlist(playlist->id);
        event.reply(success_embed("Deleted", "Deleted playlist **" + name + "**"));
    }
}

} // namespace bot
