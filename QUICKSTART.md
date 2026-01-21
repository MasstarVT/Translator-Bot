# Quick Start Guide

Get your Discord Translation Bot running in 5 minutes!

## 1. Install Dependencies

Run the automated installer:
```bash
./INSTALL_DEPENDENCIES.sh
```

Or manually install:
```bash
sudo apt install -y build-essential cmake libcurl4-openssl-dev libssl-dev git
```

## 2. Build the Bot

```bash
./build.sh
```

This will download dependencies and compile the bot. The executable will be at `build/bin/discord-bot`.

## 3. Configure Discord Token

Create a `.env` file with your bot token:
```bash
echo "DISCORD_BOT_TOKEN=your_discord_bot_token_here" > .env
```

**To get a Discord bot token:**
1. Go to https://discord.com/developers/applications
2. Click "New Application"
3. Go to "Bot" section
4. Click "Add Bot"
5. Enable "Message Content Intent" under Privileged Gateway Intents
6. Copy the token

## 4. Run the Bot

```bash
./build/bin/discord-bot
```

You should see:
```
YourBotName has connected to Discord!
Bot ID: 1234567890
Slash commands registered!
```

## 5. Test in Discord

Try these commands:
```
/translate text:Hello, world! target_language:spanish
/detectlanguage text:Bonjour
/languages
```

## 6. Enable Auto-Translation (Optional)

In any channel, run:
```
/autotranslate languages:english enable:true
```

Now any message in another language will be automatically translated to English!

## Troubleshooting

**Build fails?**
- Make sure all dependencies are installed
- Try: `rm -rf build && ./build.sh`

**Bot doesn't connect?**
- Check your `.env` file has the correct token
- Enable Message Content Intent in Discord Developer Portal

**Commands don't appear?**
- Wait 5-10 minutes for Discord to sync commands
- Try kicking and re-inviting the bot

## Next Steps

- Read [README.md](README.md) for full documentation
- Set up Docker deployment with [Dockerfile.cpp](Dockerfile.cpp)
- Configure auto-translation in multiple channels

Happy translating! 🌐
