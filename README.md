# Discord Multi-Feature Bot

A high-performance Discord bot built with C++ and the D++ library featuring translation, moderation, leveling, music, and more.

## Features

- **Translation** - Translate messages between 30+ languages with auto-translation support
- **Moderation** - Auto-mod, warnings, mutes, kicks, bans with configurable actions
- **Leveling** - XP system with levels, rewards, leaderboards, and voice XP
- **Custom Commands** - Create server-specific custom commands
- **Welcome/Goodbye** - Customizable welcome and farewell messages
- **Music** - YouTube playback with queue, playlists, and controls
- **Reaction Roles** - Self-assignable roles via reactions
- **Logging** - Comprehensive event logging for messages, members, and moderation
- **Notifications** - Twitch and YouTube stream/upload alerts (placeholder)

## Performance

- **Fast startup**: Compiled binary starts in under 500ms
- **Low memory**: Typically 15-30MB RAM
- **Efficient**: Native C++ threading with minimal overhead
- **Scalable**: SQLite database for persistent storage

---

## Commands

### Translation

| Command | Description |
|---------|-------------|
| `/translate <text> <target_language>` | Translate text to a target language |
| `/detectlanguage <text>` | Detect the language of text |
| `/languages` | Show all supported languages |
| `/autotranslate <action> [languages] [channel]` | Configure auto-translation for channels |

**Supported Languages**: English, Spanish, French, German, Italian, Portuguese, Russian, Japanese, Korean, Chinese, Arabic, Hindi, Dutch, Polish, Turkish, Vietnamese, Thai, Swedish, Norwegian, Danish, Finnish, Greek, Czech, Romanian, Hungarian, Hebrew, Indonesian, Malay, Filipino, Ukrainian, Bengali, Tamil

---

### Moderation

| Command | Description |
|---------|-------------|
| `/warn <user> [reason]` | Issue a warning to a user |
| `/warnings [user]` | View warnings for a user |
| `/clearwarnings <user> [amount]` | Clear warnings (all or specific amount) |
| `/mute <user> <duration> [reason]` | Timeout a user (e.g., "10m", "1h", "1d") |
| `/unmute <user>` | Remove timeout from a user |
| `/kick <user> [reason]` | Kick a user from the server |
| `/ban <user> [reason] [delete_days]` | Ban a user from the server |
| `/unban <user_id>` | Unban a user by ID |
| `/automod spam <enabled> [threshold] [action]` | Configure spam detection |
| `/automod words <add\|remove\|list> [word]` | Manage filtered words |
| `/automod links <enabled>` | Enable/disable link filtering |
| `/automod mentions <enabled> [threshold]` | Configure mention spam detection |
| `/automod whitelist <add\|remove> <target>` | Manage auto-mod whitelist |

**Auto-Mod Actions**: `warn`, `mute`, `kick`, `ban`

---

### Leveling

| Command | Description |
|---------|-------------|
| `/rank [user]` | View your or another user's rank card |
| `/leaderboard [page]` | View the server XP leaderboard |
| `/setxp <user> <amount>` | Set a user's XP (Admin) |
| `/addxp <user> <amount>` | Add XP to a user (Admin) |
| `/resetxp [user]` | Reset XP for a user or entire server (Admin) |
| `/levelconfig enable <bool>` | Enable/disable the leveling system |
| `/levelconfig xp <min> <max>` | Set XP range per message |
| `/levelconfig cooldown <seconds>` | Set XP cooldown between messages |
| `/levelconfig voice <xp> <min_users>` | Configure voice channel XP |
| `/levelconfig message <text>` | Set custom level-up message |
| `/levelreward add <level> <role>` | Add a role reward for reaching a level |
| `/levelreward remove <level>` | Remove a level reward |
| `/levelreward list` | List all level rewards |

**Level-Up Message Variables**: `{user}`, `{level}`, `{server}`

---

### Custom Commands

| Command | Description |
|---------|-------------|
| `/customcommand create <name> <response> [embed]` | Create a custom command |
| `/customcommand delete <name>` | Delete a custom command |
| `/customcommand edit <name> [response] [embed]` | Edit an existing command |
| `/customcommand list` | List all custom commands |
| `/c <name>` | Execute a custom command |

**Response Variables**: `{user}`, `{server}`, `{channel}`, `{membercount}`

---

### Welcome & Goodbye

| Command | Description |
|---------|-------------|
| `/welcome enable <bool>` | Enable/disable welcome messages |
| `/welcome channel <channel>` | Set the welcome channel |
| `/welcome message <text>` | Set the welcome message |
| `/welcome embed <bool> [color]` | Toggle embed mode and set color |
| `/welcome dm <bool> [message]` | Enable DM welcome messages |
| `/welcome role [role]` | Set auto-assign role for new members |
| `/welcome test` | Test the welcome message |
| `/goodbye enable <bool>` | Enable/disable goodbye messages |
| `/goodbye channel <channel>` | Set the goodbye channel |
| `/goodbye message <text>` | Set the goodbye message |
| `/goodbye embed <bool> [color]` | Toggle embed mode and set color |
| `/goodbye test` | Test the goodbye message |

**Message Variables**: `{user}`, `{username}`, `{server}`, `{membercount}`

---

### Music

| Command | Description |
|---------|-------------|
| `/play <query>` | Play a song or add to queue (YouTube URL or search) |
| `/pause` | Pause the current track |
| `/resume` | Resume playback |
| `/skip [amount]` | Skip current or multiple tracks |
| `/stop` | Stop playback and clear the queue |
| `/queue [page]` | View the current queue |
| `/nowplaying` | Show the currently playing track |
| `/volume <0-100>` | Set playback volume |
| `/shuffle` | Shuffle the queue |
| `/loop <off\|song\|queue>` | Set loop mode |
| `/remove <position>` | Remove a track from the queue |
| `/seek <time>` | Seek to a position (e.g., "1:30", "90") |
| `/join` | Join your voice channel |
| `/leave` | Leave the voice channel |
| `/playlist save <name>` | Save current queue as a playlist |
| `/playlist load <name>` | Load a saved playlist |
| `/playlist list` | List your saved playlists |
| `/playlist delete <name>` | Delete a saved playlist |

---

### Reaction Roles

| Command | Description |
|---------|-------------|
| `/reactionrole create <channel> [title] [mode]` | Create a reaction role message |
| `/reactionrole add <message_id> <emoji> <role>` | Add a role to a reaction role message |
| `/reactionrole remove <message_id> <emoji>` | Remove a role from a message |
| `/reactionrole list` | List all reaction role configurations |
| `/reactionrole delete <message_id>` | Delete a reaction role configuration |

**Modes**:
- `normal` - Toggle role on react/unreact
- `unique` - Only one role at a time (removes others)
- `verify` - Keep role after reaction removed

---

### Logging

| Command | Description |
|---------|-------------|
| `/logging channel <type> [channel]` | Set log channel (leave empty to disable) |
| `/logging enable <type> <bool>` | Enable/disable specific log types |
| `/logging ignore <add\|remove> <target>` | Ignore users/channels from logging |

**Channel Types**: `messages`, `members`, `moderation`, `voice`, `server`

**Log Types**: `message_edits`, `message_deletes`, `member_joins`, `member_leaves`, `member_bans`, `voice_state`, `role_changes`, `nickname_changes`

---

### Notifications (Placeholder)

| Command | Description |
|---------|-------------|
| `/twitch add <username> <channel> [role] [message]` | Add Twitch stream notification |
| `/twitch remove <username>` | Remove Twitch notification |
| `/twitch list` | List Twitch notifications |
| `/youtube add <channel_id> <channel> [role] [message]` | Add YouTube upload notification |
| `/youtube remove <channel_id>` | Remove YouTube notification |
| `/youtube list` | List YouTube notifications |

*Note: Requires Twitch/YouTube API credentials in `.env`*

---

## Requirements

### Build Dependencies

- CMake 3.15+
- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- libcurl development libraries
- OpenSSL development libraries
- SQLite3 development libraries
- libopus (for voice support)
- libsodium (for voice support)

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config \
    libcurl4-openssl-dev libssl-dev zlib1g-dev \
    libsqlite3-dev libopus-dev libsodium-dev
```

---

## Installation

### Option 1: Docker (Recommended)

```bash
# Clone the repository
git clone https://github.com/yourusername/discord-bot.git
cd discord-bot

# Create .env file
echo "DISCORD_TOKEN=your_bot_token_here" > .env

# Build and run
docker-compose up -d

# View logs
docker-compose logs -f
```

### Option 2: Build from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/discord-bot.git
cd discord-bot

# Create build directory
mkdir build && cd build

# Configure and build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)

# Return to project root
cd ..

# Create .env file
echo "DISCORD_TOKEN=your_bot_token_here" > .env

# Run the bot
./build/bin/discord-bot
```

---

## Configuration

### Environment Variables (.env)

```bash
# Required
DISCORD_TOKEN=your_discord_bot_token

# Optional - for translation (uses free Google Translate API by default)
GOOGLE_API_KEY=your_google_api_key

# Optional - for Twitch notifications
TWITCH_CLIENT_ID=your_twitch_client_id
TWITCH_CLIENT_SECRET=your_twitch_client_secret

# Optional - for YouTube notifications
YOUTUBE_API_KEY=your_youtube_api_key

# Optional - database path (default: ./data/bot.db)
DATABASE_PATH=/path/to/bot.db
```

### Discord Bot Setup

1. Go to [Discord Developer Portal](https://discord.com/developers/applications)
2. Create a new application
3. Go to "Bot" section and create a bot
4. Enable these Privileged Gateway Intents:
   - **Presence Intent**
   - **Server Members Intent**
   - **Message Content Intent**
5. Copy the bot token to your `.env` file
6. Go to "OAuth2" > "URL Generator"
7. Select scopes: `bot`, `applications.commands`
8. Select permissions: `Administrator` (or specific permissions)
9. Use the generated URL to invite the bot

---

## Data Storage

All data is stored in an SQLite database (`data/bot.db`):

- Guild settings and configurations
- User XP and levels
- Warnings and moderation history
- Custom commands
- Welcome/goodbye settings
- Reaction role configurations
- Logging settings
- Playlists
- Notification subscriptions

---

## Docker

### docker-compose.yml

```yaml
services:
  discord-bot:
    build: .
    container_name: discord-bot
    restart: unless-stopped
    volumes:
      - ./.env:/app/.env:ro
      - ./data:/app/data
    environment:
      - DATABASE_PATH=/app/data/bot.db
    security_opt:
      - no-new-privileges:true
```

### Commands

```bash
# Build and start
docker-compose up -d

# View logs
docker-compose logs -f

# Stop
docker-compose down

# Rebuild after changes
docker-compose build && docker-compose up -d
```

---

## Troubleshooting

### Bot doesn't connect
- Verify your token in `.env` is correct
- Check that all required intents are enabled in Discord Developer Portal

### Commands not appearing
- Wait up to 1 hour for global commands to propagate
- Try kicking and re-inviting the bot with `applications.commands` scope

### Database errors
- Ensure the `data/` directory exists and is writable
- Check `DATABASE_PATH` environment variable

### Music not playing
- Note: Voice playback requires additional DPP voice client setup
- Ensure `ffmpeg` and `yt-dlp` are installed (included in Docker)

### Translation errors
- Check your internet connection
- Google Translate API may be rate-limited

---

## License

This project is open source and available for personal and commercial use.

## Contributing

Pull requests are welcome! Please feel free to submit issues and feature requests.
