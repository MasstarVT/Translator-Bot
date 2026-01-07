# 🌐 Discord Translation Bot

A powerful Discord bot that translates messages between multiple languages with support for automatic translation, multi-language translation, and language detection.

## ✨ Features

- **Single Language Translation**: Translate text to any supported language
- **Multi-Language Translation**: Translate text to multiple languages simultaneously
- **Auto-Translation**: Enable automatic translation of all messages in a channel
- **Language Detection**: Detect the language of any text
- **30+ Languages Supported**: Including English, Spanish, French, German, Japanese, Korean, Chinese, Arabic, and more
- **Slash Commands**: Modern Discord slash command interface
- **Traditional Commands**: Fallback prefix commands (!tr)

## 📋 Commands

### Slash Commands

- `/translate <text> <target_language>` - Translate text to a target language
  - Example: `/translate Hello, world! spanish`

- `/multitranslate <text> <languages>` - Translate to multiple languages at once
  - Example: `/multitranslate Hello! spanish, french, german`

- `/autotranslate <target_language> <enable>` - Enable/disable auto-translation in channel
  - Example: `/autotranslate spanish true`
  - Example: `/autotranslate spanish false` (to disable)

- `/detectlanguage <text>` - Detect the language of text
  - Example: `/detectlanguage Bonjour le monde`

- `/languages` - Show all supported languages

### Traditional Commands

- `!tr <language> <text>` - Translate using prefix command
  - Example: `!tr spanish Hello, world!`

## 🚀 Setup Instructions

### Option 1: Docker (Recommended)

**Prerequisites:**
- Docker and Docker Compose installed
- Discord bot token

**Quick Start:**

1. Clone the repository and navigate to the directory
2. Copy `.env.example` to `.env` and add your bot token:
   ```bash
   cp .env.example .env
   # Edit .env and add: DISCORD_BOT_TOKEN=your_bot_token_here
   ```

3. Run the automated setup script:
   ```bash
   ./docker-start.sh
   ```

**Manual Docker Commands:**

```bash
# Build the Docker image
docker-compose build

# Start the bot
docker-compose up -d

# View logs
docker-compose logs -f

# Stop the bot
docker-compose down

# Restart the bot
docker-compose restart
```

**Persistent Data:**
- Bot settings (auto-translate configurations) are stored in `bot_settings.json`
- This file is automatically mounted as a volume and persists between container restarts

### Option 2: Traditional Installation

### 1. Create a Discord Bot

1. Go to [Discord Developer Portal](https://discord.com/developers/applications)
2. Click "New Application" and give it a name
3. Go to the "Bot" section and click "Add Bot"
4. Under "Privileged Gateway Intents", enable:
   - Message Content Intent
   - Server Members Intent (optional)
5. Copy your bot token (you'll need this later)
6. Go to "OAuth2" → "URL Generator"
7. Select scopes: `bot` and `applications.commands`
8. Select bot permissions: 
   - Send Messages
   - Send Messages in Threads
   - Embed Links
   - Read Message History
   - Use Slash Commands
9. Copy the generated URL and use it to invite the bot to your server

### 2. Install Dependencies

```bash
# Create a virtual environment (recommended)
python -m venv venv

# Activate virtual environment
# On Linux/Mac:
source venv/bin/activate
# On Windows:
# venv\Scripts\activate

# Install required packages
pip install -r requirements.txt
```

### 3. Configure the Bot

1. Copy `.env.example` to `.env`:
   ```bash
   cp .env.example .env
   ```

2. Edit `.env` and add your Discord bot token:
   ```
   DISCORD_BOT_TOKEN=your_actual_bot_token_here
   ```

### 4. Run the Bot

```bash
python bot.py
```

You should see output like:
```
BotName#1234 has connected to Discord!
Bot is in 1 guilds
Synced 5 slash commands
```

## 🌍 Supported Languages

The bot supports 30+ languages includiSng:

- **European**: English, Spanish, French, German, Italian, Portuguese, Russian, Dutch, Polish, Turkish, Swedish, Norwegian, Danish, Finnish, Greek, Czech, Romanian, Hungarian, Ukrainian
- **Asian**: Japanese, Korean, Chinese, Hindi, Thai, Vietnamese, Indonesian, Malay, Filipino, Bengali, Tamil, Hebrew
- **Middle Eastern**: Arabic, Hebrew, Turkish

Use either language names (e.g., "spanish", "french") or language codes (e.g., "es", "fr") in commands.

## 💡 Usage Examples

### Basic Translation
```
/translate text:Hello, how are you? target_language:spanish
```
Result: "Hola, ¿cómo estás?"

### Multi-Language Translation
```
/multitranslate text:Good morning! languages:spanish, french, japanese
```
Results in Spanish, French, and Japanese translations

### Auto-Translation Setup
```
/autotranslate target_language:english enable:true
```
Now all non-English messages in this channel will be automatically translated to English!

### Language Detection
```
/detectlanguage text:こんにちは
```
Result: Japanese (ja)

## 🛠️ Technical Details

- **Framework**: discord.py 2.3.0+
- **Translation Engine**: deep-translator (Google Translate API)
- **Language Detection**: langdetect
- **Python**: 3.8+

## 📝 Notes

- Translation quality depends on Google Translate
- Very long messages may be truncated in embeds (Discord limit: 1024 characters per field)
- Auto-translation only works for messages sent after it's enabled
- Bot messages are never auto-translated to avoid loops
- Free tier of Google Translate is used (no API key required)

## 🔧 Troubleshooting

**Bot not responding to slash commands:**
- Make sure the bot has been invited with the `applications.commands` scope
- Wait a few minutes for commands to sync globally
- Try kicking and re-inviting the bot

**Translation errors:**
- Check your internet connection
- The translation service may be temporarily unavailable
- Very short text might not be detected properly

**Bot doesn't start:**
- Verify your bot token in `.env` is correct
- Make sure all dependencies are installed: `pip install -r requirements.txt`
- Check that you've enabled Message Content Intent in the Discord Developer Portal

## 📜 License

This project is open source and available for personal and commercial use.

## 🤝 Contributing

Feel free to fork, modify, and improve this bot! Suggestions and pull requests are welcome.

---

Made with ❤️ for the Discord community
