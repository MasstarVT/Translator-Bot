# Discord Translation Bot

A high-performance Discord bot for translating messages between multiple languages using C++ and the D++ library.

## Features

- Single Language Translation
- Language Detection
- Auto-Translation (channel-based)
- 30+ Languages Supported
- Slash Commands
- Persistent Settings

## Performance Benefits

- **Fast startup time**: Compiled binary starts in under 500ms
- **Low memory usage**: Typically 10-20MB RAM
- **Efficient concurrency**: Native threading with minimal overhead
- **Optimized translation**: Fast HTTP requests and JSON parsing

## Requirements

- CMake 3.15 or higher
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- libcurl development libraries
- OpenSSL development libraries
- Git

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev libssl-dev git
```

### Fedora/RHEL

```bash
sudo dnf install -y gcc-c++ cmake libcurl-devel openssl-devel git
```

### Arch Linux

```bash
sudo pacman -S base-devel cmake curl openssl git
```

### macOS

```bash
brew install cmake curl openssl git
```

## Building

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Configure the project:
```bash
cmake ..
```

3. Build the bot:
```bash
cmake --build . -j$(nproc)
```

The executable will be in `build/bin/discord-bot`

## Setup

1. Make sure you have a `.env` file in the project root with your Discord bot token:
```bash
DISCORD_BOT_TOKEN=your_bot_token_here
```

2. Run the bot from the project root directory (where .env is located):
```bash
./build/bin/discord-bot
```

## Running

After building, run the bot:

```bash
cd /path/to/project/root
./build/bin/discord-bot
```

The bot will:
- Load settings from `bot_settings.json`
- Connect to Discord
- Register slash commands
- Start listening for messages

## Commands

### Slash Commands

- `/translate <text> <target_language>` - Translate text to a target language
- `/detectlanguage <text>` - Detect the language of text
- `/languages` - Show all supported languages
- `/autotranslate <languages> <enable>` - Enable/disable auto-translation in channel

## Supported Languages

30+ languages supported:
- **European**: English, Spanish, French, German, Italian, Portuguese, Russian, Dutch, Polish, Turkish, Swedish, Norwegian, Danish, Finnish, Greek, Czech, Romanian, Hungarian, Ukrainian
- **Asian**: Japanese, Korean, Chinese, Hindi, Thai, Vietnamese, Indonesian, Malay, Filipino, Bengali, Tamil
- **Middle Eastern**: Arabic, Hebrew

## Configuration

Settings are stored in `bot_settings.json` and persist between restarts. The format is:

```json
{
  "auto_translate_channels": {
    "channel_id": ["en", "es"]
  },
  "auto_translate_servers": {}
}
```

## Docker Support

You can also build and run using Docker:

```bash
# Build Docker image
docker build -t discord-bot-cpp -f Dockerfile.cpp .

# Run the container
docker run -d --name discord-bot --env-file .env -v $(pwd)/bot_settings.json:/app/bot_settings.json discord-bot-cpp
```

## Troubleshooting

### Build Errors

**CMake can't find CURL:**
- Install libcurl development libraries: `sudo apt install libcurl4-openssl-dev`

**Linker errors:**
- Make sure you have OpenSSL installed: `sudo apt install libssl-dev`

**D++ fetch fails:**
- Check your internet connection
- Try clearing CMake cache: `rm -rf build && mkdir build`

### Runtime Errors

**Bot doesn't connect:**
- Verify your token in `.env` is correct
- Check that Message Content Intent is enabled in Discord Developer Portal

**Translation errors:**
- Check your internet connection
- Google Translate API may be temporarily unavailable

**Settings not persisting:**
- Make sure the bot has write permissions in the directory
- Check that `bot_settings.json` is in the same directory as the executable

## Performance Tips

1. **Compile with optimizations:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

2. **Use static linking for deployment:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF ..
```

3. **Run with nice priority:**
```bash
nice -n -10 ./build/bin/discord-bot
```

## Quick Start

1. **Install dependencies** (or use `./INSTALL_DEPENDENCIES.sh`):
   ```bash
   sudo apt install -y build-essential cmake libcurl4-openssl-dev libssl-dev git
   ```

2. **Build the bot** (or use `./build.sh`):
   ```bash
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   cmake --build . -j$(nproc)
   cd ..
   ```

3. **Configure your bot**:
   ```bash
   echo "DISCORD_BOT_TOKEN=your_token_here" > .env
   ```

4. **Run the bot**:
   ```bash
   ./build/bin/discord-bot
   ```

## License

This project is open source and available for personal and commercial use.

## Contributing

Feel free to fork, modify, and improve this bot! Pull requests are welcome.
