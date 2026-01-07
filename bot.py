import discord
from discord.ext import commands
from discord import app_commands
import os
import json
import re
from dotenv import load_dotenv
from deep_translator import GoogleTranslator
from langdetect import detect, DetectorFactory
import asyncio

# Make language detection deterministic
DetectorFactory.seed = 0

# Load environment variables
load_dotenv()
TOKEN = os.getenv('DISCORD_BOT_TOKEN')

# Bot setup with intents
intents = discord.Intents.default()
# Message Content Intent is required for auto-translate and prefix commands
# Enable it at: https://discord.com/developers/applications
intents.message_content = True
intents.messages = True

bot = commands.Bot(command_prefix='!', intents=intents)

# Settings file path
SETTINGS_FILE = 'bot_settings.json'

# Load and save functions for persistent settings
def load_settings():
    """Load auto-translate settings from file"""
    try:
        if os.path.exists(SETTINGS_FILE):
            with open(SETTINGS_FILE, 'r') as f:
                data = json.load(f)
                # Convert string keys back to integers and ensure values are lists
                settings = {}
                for k, v in data.get('auto_translate_channels', {}).items():
                    # Convert old single-language format to list format
                    if isinstance(v, str):
                        settings[int(k)] = [v]
                    else:
                        settings[int(k)] = v
                
                # Load server-wide settings
                server_settings = {}
                for k, v in data.get('auto_translate_servers', {}).items():
                    if isinstance(v, str):
                        server_settings[int(k)] = [v]
                    else:
                        server_settings[int(k)] = v
                
                return settings, server_settings
    except Exception as e:
        print(f"Error loading settings: {e}")
    return {}, {}

def save_settings(auto_translate_channels, auto_translate_servers):
    """Save auto-translate settings to file"""
    try:
        data = {
            'auto_translate_channels': auto_translate_channels,
            'auto_translate_servers': auto_translate_servers
        }
        with open(SETTINGS_FILE, 'w') as f:
            json.dump(data, f, indent=2)
    except Exception as e:
        print(f"Error saving settings: {e}")

# Language code mapping for common languages
LANGUAGE_NAMES = {
    'english': 'en', 'spanish': 'es', 'french': 'fr', 'german': 'de',
    'italian': 'it', 'portuguese': 'pt', 'russian': 'ru', 'japanese': 'ja',
    'korean': 'ko', 'chinese': 'zh-CN', 'arabic': 'ar', 'hindi': 'hi',
    'dutch': 'nl', 'polish': 'pl', 'turkish': 'tr', 'vietnamese': 'vi',
    'thai': 'th', 'swedish': 'sv', 'norwegian': 'no', 'danish': 'da',
    'finnish': 'fi', 'greek': 'el', 'czech': 'cs', 'romanian': 'ro',
    'hungarian': 'hu', 'hebrew': 'iw', 'indonesian': 'id', 'malay': 'ms',
    'filipino': 'tl', 'ukrainian': 'uk', 'bengali': 'bn', 'tamil': 'ta'
}

# Language flags mapping
LANGUAGE_FLAGS = {
    'en': '🇬🇧', 'es': '🇪🇸', 'fr': '🇫🇷', 'de': '🇩🇪',
    'it': '🇮🇹', 'pt': '🇵🇹', 'ru': '🇷🇺', 'ja': '🇯🇵',
    'ko': '🇰🇷', 'zh-CN': '🇨🇳', 'ar': '🇸🇦', 'hi': '🇮🇳',
    'nl': '🇳🇱', 'pl': '🇵🇱', 'tr': '🇹🇷', 'vi': '🇻🇳',
    'th': '🇹🇭', 'sv': '🇸🇪', 'no': '🇳🇴', 'da': '🇩🇰',
    'fi': '🇫🇮', 'el': '🇬🇷', 'cs': '🇨🇿', 'ro': '🇷🇴',
    'hu': '🇭🇺', 'iw': '🇮🇱', 'id': '🇮🇩', 'ms': '🇲🇾',
    'tl': '🇵🇭', 'uk': '🇺🇦', 'bn': '🇧🇩', 'ta': '🇮🇳'
}

def get_language_code(lang_input):
    """Convert language name or code to language code"""
    lang_input = lang_input.lower().strip()
    
    # Check if it's already a valid code
    if len(lang_input) == 2 or lang_input == 'zh-cn':
        return lang_input
    
    # Check if it's a language name
    return LANGUAGE_NAMES.get(lang_input, None)

def clean_text_for_detection(text):
    """Remove emojis and special characters for better language detection"""
    # Remove emojis and other unicode symbols
    emoji_pattern = re.compile(
        "["
        u"\U0001F600-\U0001F64F"  # emoticons
        u"\U0001F300-\U0001F5FF"  # symbols & pictographs
        u"\U0001F680-\U0001F6FF"  # transport & map symbols
        u"\U0001F1E0-\U0001F1FF"  # flags (iOS)
        u"\U00002500-\U00002BEF"  # chinese char
        u"\U00002702-\U000027B0"
        u"\U000024C2-\U0001F251"
        u"\U0001f926-\U0001f937"
        u"\U00010000-\U0010ffff"
        u"\u2640-\u2642"
        u"\u2600-\u2B55"
        u"\u200d"
        u"\u23cf"
        u"\u23e9"
        u"\u231a"
        u"\ufe0f"  # dingbats
        u"\u3030"
        "]+", flags=re.UNICODE)
    
    cleaned = emoji_pattern.sub('', text).strip()
    return cleaned if cleaned else text

def detect_language(text):
    """Detect language with better error handling"""
    try:
        # Clean text for detection (remove emojis)
        cleaned_text = clean_text_for_detection(text)
        
        # If cleaned text is too short or empty, default to English
        if len(cleaned_text) < 3:
            print(f"⚠️ Text too short for detection: '{text[:50]}' - defaulting to 'en'")
            return 'en'
        
        detected = detect(cleaned_text)
        print(f"🔍 Detected language: {detected} for text: {cleaned_text[:50]}")
        
        # Common misdetections - Catalan is often confused with Spanish
        # If detected as Catalan, it might be Spanish (they're very similar)
        if detected == 'ca':
            # Try to detect again with more context if possible
            print(f"⚠️ Detected Catalan (ca) - may be Spanish (es). Text: {cleaned_text[:100]}")
        
        return detected
    except Exception as e:
        print(f"❌ Language detection error: {e}")
        # Default to English if detection fails
        return 'en'

@bot.event
async def on_ready():
    print(f'{bot.user} has connected to Discord!')
    print(f'Bot is in {len(bot.guilds)} guilds')
    print(f'Bot ID: {bot.user.id}')
    
    # Generate invite URLs for both server and user installation
    server_invite = f"https://discord.com/api/oauth2/authorize?client_id={bot.user.id}&permissions=274878286848&scope=bot%20applications.commands"
    user_invite = f"https://discord.com/api/oauth2/authorize?client_id={bot.user.id}&scope=applications.commands"
    
    print(f'Server Invite: {server_invite}')
    print(f'User Install (DMs/Groups): {user_invite}')
    
    # Load persistent settings
    bot.auto_translate_channels, bot.auto_translate_servers = load_settings()
    if bot.auto_translate_channels:
        print(f"📋 Loaded {len(bot.auto_translate_channels)} auto-translate channel(s)")
        for channel_id, lang in bot.auto_translate_channels.items():
            print(f"  - Channel {channel_id} → {lang}")
    if bot.auto_translate_servers:
        print(f"🌍 Loaded {len(bot.auto_translate_servers)} server-wide auto-translate(s)")
        for guild_id, lang in bot.auto_translate_servers.items():
            print(f"  - Server {guild_id} → {lang}")
    
    try:
        synced = await bot.tree.sync()
        print(f"✅ Synced {len(synced)} slash commands")
        for cmd in synced:
            print(f"  - /{cmd.name}")
    except Exception as e:
        print(f"❌ Failed to sync commands: {e}")

@bot.tree.error
async def on_app_command_error(interaction: discord.Interaction, error: app_commands.AppCommandError):
    """Handle errors from slash commands"""
    print(f"Command error: {error}")
    if interaction.response.is_done():
        await interaction.followup.send(f"❌ An error occurred: {str(error)}", ephemeral=True)
    else:
        await interaction.response.send_message(f"❌ An error occurred: {str(error)}", ephemeral=True)

async def language_autocomplete(
    interaction: discord.Interaction,
    current: str,
) -> list[app_commands.Choice[str]]:
    """Autocomplete for language names - supports comma-separated lists"""
    # Get all language names
    all_languages = list(LANGUAGE_NAMES.keys())
    
    # Check if user is typing multiple languages (comma-separated)
    if ',' in current:
        # Split by comma and get the part being typed
        parts = current.rsplit(',', 1)
        prefix = parts[0] + ','  # Everything before the last comma
        last_part = parts[1].strip()  # The language currently being typed
        
        # Filter based on what user is typing after the last comma
        if last_part:
            matches = [lang for lang in all_languages if last_part.lower() in lang.lower()]
        else:
            matches = all_languages[:25]
        
        # Return choices with the prefix included
        return [
            app_commands.Choice(name=f"{prefix} {lang.title()}", value=f"{prefix} {lang}")
            for lang in matches[:25]
        ]
    else:
        # Single language - normal autocomplete
        if current:
            matches = [lang for lang in all_languages if current.lower() in lang.lower()]
        else:
            matches = all_languages[:25]
        
        return [
            app_commands.Choice(name=lang.title(), value=lang)
            for lang in matches[:25]
        ]

@bot.tree.command(name="translate", description="Translate text to a target language")
@app_commands.describe(
    text="The text to translate",
    target_language="Target language (e.g., 'spanish', 'es', 'french')"
)
@app_commands.autocomplete(target_language=language_autocomplete)
async def translate(interaction: discord.Interaction, text: str, target_language: str):
    """Translate text to a single target language"""
    await interaction.response.defer()
    
    target_code = get_language_code(target_language)
    if not target_code:
        await interaction.followup.send(f"❌ Invalid language: `{target_language}`. Use language names (e.g., 'spanish') or codes (e.g., 'es')")
        return
    
    try:
        # Detect source language
        source_lang = detect_language(text)
        
        # Translate
        translator = GoogleTranslator(source='auto', target=target_code)
        translated = translator.translate(text)
        
        # Create embed
        embed = discord.Embed(title="🌐 Translation", color=discord.Color.blue())
        embed.add_field(name=f"Original ({source_lang})", value=text[:1024], inline=False)
        embed.add_field(name=f"Translation ({target_code})", value=translated[:1024], inline=False)
        embed.set_footer(text=f"Requested by {interaction.user.name}")
        
        await interaction.followup.send(embed=embed)
        
    except Exception as e:
        await interaction.followup.send(f"❌ Translation error: {str(e)}")

@bot.tree.command(name="multitranslate", description="Translate text to multiple languages at once")
@app_commands.describe(
    text="The text to translate",
    languages="Comma-separated list of target languages (e.g., 'spanish, french, german')"
)
async def multitranslate(interaction: discord.Interaction, text: str, languages: str):
    """Translate text to multiple languages simultaneously"""
    await interaction.response.defer()
    
    # Parse target languages
    target_langs = [lang.strip() for lang in languages.split(',')]
    target_codes = []
    
    for lang in target_langs:
        code = get_language_code(lang)
        if code:
            target_codes.append((lang, code))
        else:
            await interaction.followup.send(f"❌ Invalid language: `{lang}`")
            return
    
    if not target_codes:
        await interaction.followup.send("❌ No valid target languages specified")
        return
    
    try:
        # Detect source language
        source_lang = detect_language(text)
        
        # Create embed
        embed = discord.Embed(title="🌐 Multi-Language Translation", color=discord.Color.green())
        embed.add_field(name=f"Original ({source_lang})", value=text[:1024], inline=False)
        
        # Translate to each language (skip if same as source)
        for lang_name, lang_code in target_codes:
            # Skip if target language is same as source
            if lang_code == source_lang or lang_code == source_lang.split('-')[0]:
                continue
                
            try:
                translator = GoogleTranslator(source='auto', target=lang_code)
                translated = translator.translate(text)
                embed.add_field(name=f"{lang_name.title()} ({lang_code})", value=translated[:1024], inline=False)
            except Exception as e:
                embed.add_field(name=f"{lang_name.title()} ({lang_code})", value=f"❌ Error: {str(e)}", inline=False)
        
        embed.set_footer(text=f"Requested by {interaction.user.name}")
        await interaction.followup.send(embed=embed)
        
    except Exception as e:
        await interaction.followup.send(f"❌ Translation error: {str(e)}")

@bot.tree.command(name="autotranslate", description="Auto-translate messages in this channel")
@app_commands.describe(
    languages="Comma-separated languages for auto-translation (e.g., 'english, spanish, french')",
    enable="Enable or disable auto-translation (true/false)"
)
@app_commands.autocomplete(languages=language_autocomplete)
async def autotranslate(interaction: discord.Interaction, languages: str, enable: bool):
    """Enable/disable automatic translation in a channel"""
    channel_id = interaction.channel_id
    
    if enable:
        # Parse target languages
        target_langs = [lang.strip() for lang in languages.split(',')]
        target_codes = []
        
        for lang in target_langs:
            code = get_language_code(lang)
            if code:
                target_codes.append(code)
            else:
                await interaction.response.send_message(f"❌ Invalid language: `{lang}`")
                return
        
        if not target_codes:
            await interaction.response.send_message("❌ No valid target languages specified")
            return
        
        bot.auto_translate_channels[channel_id] = target_codes
        save_settings(bot.auto_translate_channels, bot.auto_translate_servers)
        
        lang_display = ", ".join([f"**{code}**" for code in target_codes])
        await interaction.response.send_message(
            f"✅ Auto-translation enabled for this channel\n"
            f"🌐 Target languages: {lang_display}"
        )
    else:
        if channel_id in bot.auto_translate_channels:
            del bot.auto_translate_channels[channel_id]
            save_settings(bot.auto_translate_channels, bot.auto_translate_servers)
        await interaction.response.send_message(f"✅ Auto-translation disabled for this channel")

@bot.tree.command(name="autotranslateserver", description="Enable auto-translate for entire server (Admin only)")
@app_commands.describe(
    languages="Comma-separated languages for auto-translation (e.g., 'english, spanish, french')",
    enable="Enable or disable server-wide auto-translation (true/false)"
)
@app_commands.default_permissions(administrator=True)
@app_commands.guild_only()
@app_commands.autocomplete(languages=language_autocomplete)
async def autotranslateserver(interaction: discord.Interaction, languages: str, enable: bool):
    """Enable/disable automatic translation for all channels in the server (Admin only)"""
    guild_id = interaction.guild_id
    
    if not guild_id:
        await interaction.response.send_message("❌ This command can only be used in a server")
        return
    
    if enable:
        # Parse target languages
        target_langs = [lang.strip() for lang in languages.split(',')]
        target_codes = []
        
        for lang in target_langs:
            code = get_language_code(lang)
            if code:
                target_codes.append(code)
            else:
                await interaction.response.send_message(f"❌ Invalid language: `{lang}`")
                return
        
        if not target_codes:
            await interaction.response.send_message("❌ No valid target languages specified")
            return
        
        bot.auto_translate_servers[guild_id] = target_codes
        save_settings(bot.auto_translate_channels, bot.auto_translate_servers)
        
        lang_display = ", ".join([f"**{code}**" for code in target_codes])
        await interaction.response.send_message(
            f"✅ Server-wide auto-translation enabled\n"
            f"🌐 Target languages: {lang_display}\n"
            f"ℹ️ This will translate messages in ALL channels (except channel-specific overrides)"
        )
    else:
        if guild_id in bot.auto_translate_servers:
            del bot.auto_translate_servers[guild_id]
            save_settings(bot.auto_translate_channels, bot.auto_translate_servers)
        await interaction.response.send_message(f"✅ Server-wide auto-translation disabled")

@bot.tree.command(name="addlanguage", description="Add a language to auto-translate in this channel")
@app_commands.describe(language="Language to add (e.g., 'spanish', 'es')")
@app_commands.autocomplete(language=language_autocomplete)
async def addlanguage(interaction: discord.Interaction, language: str):
    """Add a language to existing auto-translate settings"""
    channel_id = interaction.channel_id
    
    # Check if auto-translate is enabled for this channel
    if channel_id not in bot.auto_translate_channels:
        await interaction.response.send_message(
            "❌ Auto-translate is not enabled in this channel.\n"
            "Use `/autotranslate` to enable it first."
        )
        return
    
    # Get language code
    lang_code = get_language_code(language)
    if not lang_code:
        await interaction.response.send_message(f"❌ Invalid language: `{language}`")
        return
    
    # Check if language is already in the list
    current_langs = bot.auto_translate_channels[channel_id]
    if lang_code in current_langs:
        await interaction.response.send_message(f"ℹ️ **{lang_code}** is already in the auto-translate list")
        return
    
    # Add the language
    current_langs.append(lang_code)
    save_settings(bot.auto_translate_channels, bot.auto_translate_servers)
    
    lang_display = ", ".join([f"**{code}**" for code in current_langs])
    await interaction.response.send_message(
        f"✅ Added **{lang_code}** to auto-translate\n"
        f"🌐 Current languages: {lang_display}"
    )

@bot.tree.command(name="removelanguage", description="Remove a language from auto-translate in this channel")
@app_commands.describe(language="Language to remove (e.g., 'spanish', 'es')")
@app_commands.autocomplete(language=language_autocomplete)
async def removelanguage(interaction: discord.Interaction, language: str):
    """Remove a language from existing auto-translate settings"""
    channel_id = interaction.channel_id
    
    # Check if auto-translate is enabled for this channel
    if channel_id not in bot.auto_translate_channels:
        await interaction.response.send_message(
            "❌ Auto-translate is not enabled in this channel."
        )
        return
    
    # Get language code
    lang_code = get_language_code(language)
    if not lang_code:
        await interaction.response.send_message(f"❌ Invalid language: `{language}`")
        return
    
    # Check if language is in the list
    current_langs = bot.auto_translate_channels[channel_id]
    if lang_code not in current_langs:
        await interaction.response.send_message(f"ℹ️ **{lang_code}** is not in the auto-translate list")
        return
    
    # Remove the language
    current_langs.remove(lang_code)
    
    # If no languages left, disable auto-translate
    if not current_langs:
        del bot.auto_translate_channels[channel_id]
        save_settings(bot.auto_translate_channels, bot.auto_translate_servers)
        await interaction.response.send_message(
            f"✅ Removed **{lang_code}** from auto-translate\n"
            f"⚠️ No languages remaining - auto-translate has been disabled"
        )
        return
    
    save_settings(bot.auto_translate_channels, bot.auto_translate_servers)
    
    lang_display = ", ".join([f"**{code}**" for code in current_langs])
    await interaction.response.send_message(
        f"✅ Removed **{lang_code}** from auto-translate\n"
        f"🌐 Current languages: {lang_display}"
    )

@bot.tree.command(name="detectlanguage", description="Detect the language of text")
@app_commands.describe(text="The text to analyze")
async def detectlanguage(interaction: discord.Interaction, text: str):
    """Detect the language of given text"""
    try:
        detected_lang = detect_language(text)
        
        # Find full language name
        lang_name = next((name for name, code in LANGUAGE_NAMES.items() if code == detected_lang), detected_lang)
        
        embed = discord.Embed(title="🔍 Language Detection", color=discord.Color.purple())
        embed.add_field(name="Text", value=text[:1024], inline=False)
        embed.add_field(name="Detected Language", value=f"{lang_name.title()} ({detected_lang})", inline=False)
        
        await interaction.response.send_message(embed=embed)
    except Exception as e:
        await interaction.response.send_message(f"❌ Detection error: {str(e)}")

@bot.tree.command(name="languages", description="List all supported languages")
async def languages(interaction: discord.Interaction):
    """Show list of supported languages"""
    lang_list = "\n".join([f"**{name.title()}**: `{code}`" for name, code in sorted(LANGUAGE_NAMES.items())])
    
    embed = discord.Embed(title="🌍 Supported Languages", description=lang_list, color=discord.Color.gold())
    embed.set_footer(text="Use language names or codes in commands")
    
    await interaction.response.send_message(embed=embed)

@bot.tree.command(name="invite", description="Get bot invite links")
async def invite_command(interaction: discord.Interaction):
    """Display bot invite links for servers and user installation"""
    bot_id = bot.user.id
    server_invite = f"https://discord.com/api/oauth2/authorize?client_id={bot_id}&permissions=274878286848&scope=bot%20applications.commands"
    user_invite = f"https://discord.com/api/oauth2/authorize?client_id={bot_id}&scope=applications.commands"
    
    embed = discord.Embed(
        title="🌐 Invite Translation Bot",
        description="Choose how you want to add the bot:",
        color=discord.Color.blue()
    )
    
    embed.add_field(
        name="🏰 Add to Server",
        value=f"[Click here]({server_invite}) to add to a Discord server\n"
              f"*Requires admin permissions*",
        inline=False
    )
    
    embed.add_field(
        name="👤 Install for Personal Use",
        value=f"[Click here]({user_invite}) to use in DMs and group chats\n"
              f"*Works in direct messages and groups!*",
        inline=False
    )
    
    embed.set_footer(text="User install allows translation in DMs without adding to servers")
    
    await interaction.response.send_message(embed=embed)

# Auto-translate channels storage (will be loaded from file on startup)
bot.auto_translate_channels = {}

# Server-wide auto-translate storage (guild_id -> [language codes])
bot.auto_translate_servers = {}

# Store mapping of original message ID to translation message ID
bot.translation_messages = {}

@bot.event
async def on_message(message):
    # Ignore bot messages
    if message.author.bot:
        return
    
    # Skip if message is empty
    if not message.content or not message.content.strip():
        return
    
    # Skip if message is just a URL/link (common for GIFs, images, etc.)
    url_pattern = re.compile(r'^https?://\S+$', re.IGNORECASE)
    if url_pattern.match(message.content.strip()):
        print(f"⏭️ Skipping URL/GIF link: {message.content[:50]}")
        await bot.process_commands(message)
        return
    
    # Check if channel has auto-translation enabled
    target_langs = None
    
    # Priority: Channel-specific settings > Server-wide settings
    if message.channel.id in bot.auto_translate_channels:
        target_langs = bot.auto_translate_channels[message.channel.id]
    elif message.guild and message.guild.id in bot.auto_translate_servers:
        target_langs = bot.auto_translate_servers[message.guild.id]
    
    if target_langs:
        
        # Ensure target_langs is a list (for backward compatibility)
        if isinstance(target_langs, str):
            target_langs = [target_langs]
        
        print(f"🔄 Auto-translating message in channel {message.channel.id}")
        print(f"📋 Target languages: {target_langs}")
        
        try:
            # Detect source language
            source_lang = detect_language(message.content)
            
            # Find source language name
            source_lang_name = next((name for name, code in LANGUAGE_NAMES.items() if code == source_lang), source_lang)
            
            # Build translations for all target languages that differ from source
            # Translate in parallel for faster performance
            async def translate_to_language(target_lang):
                """Helper function to translate to a single language"""
                # Skip if target is same as source (handle both full codes and base codes)
                if source_lang == target_lang or source_lang.split('-')[0] == target_lang.split('-')[0]:
                    return None
                    
                try:
                    translator = GoogleTranslator(source='auto', target=target_lang)
                    translated = translator.translate(message.content)
                    
                    # Skip if translation is empty or just whitespace
                    if not translated or not translated.strip():
                        print(f"⚠️ Empty translation for {target_lang}, skipping")
                        return None
                    
                    # Find target language name
                    target_lang_name = next((name for name, code in LANGUAGE_NAMES.items() if code == target_lang), target_lang)
                    
                    return (target_lang, target_lang_name, translated)
                except Exception as e:
                    print(f"❌ Error translating to {target_lang}: {e}")
                    return None
            
            # Run all translations in parallel
            translation_tasks = [translate_to_language(lang) for lang in target_langs]
            results = await asyncio.gather(*translation_tasks)
            
            # Filter out None results
            translations = [t for t in results if t is not None]
            
            # Only send if we have translations
            if translations:
                # Build compact description with flags
                description = ""
                
                for target_code, target_name, translated_text in translations:
                    flag = LANGUAGE_FLAGS.get(target_code, '🌐')
                    description += f"{flag} **{target_code.upper()}:** {translated_text[:500]}\n"
                
                embed = discord.Embed(description=description, color=discord.Color.blue())
                embed.set_footer(text="🌐 Auto-translate")
                
                reply = await message.reply(embed=embed, mention_author=False)
                # Store the mapping so we can delete it later if original is deleted
                bot.translation_messages[message.id] = reply.id
        except Exception as e:
            # Silently fail for auto-translation to avoid spam
            print(f"Auto-translation error: {e}")
    
    await bot.process_commands(message)

@bot.event
async def on_message_delete(message):
    """Delete translation when original message is deleted"""
    if message.id in bot.translation_messages:
        translation_id = bot.translation_messages[message.id]
        try:
            # Try to fetch and delete the translation message
            translation_msg = await message.channel.fetch_message(translation_id)
            await translation_msg.delete()
            print(f"🗑️ Deleted translation for message {message.id}")
        except Exception as e:
            print(f"⚠️ Could not delete translation: {e}")
        finally:
            # Remove from tracking dictionary
            del bot.translation_messages[message.id]

# Traditional command for translate (in case slash commands don't work)
@bot.command(name='tr')
async def translate_cmd(ctx, target_lang: str, *, text: str):
    """Traditional command: !tr <language> <text>"""
    target_code = get_language_code(target_lang)
    if not target_code:
        await ctx.send(f"❌ Invalid language: `{target_lang}`")
        return
    
    try:
        source_lang = detect_language(text)
        translator = GoogleTranslator(source='auto', target=target_code)
        translated = translator.translate(text)
        
        embed = discord.Embed(title="🌐 Translation", color=discord.Color.blue())
        embed.add_field(name=f"Original ({source_lang})", value=text[:1024], inline=False)
        embed.add_field(name=f"Translation ({target_code})", value=translated[:1024], inline=False)
        
        await ctx.send(embed=embed)
    except Exception as e:
        await ctx.send(f"❌ Translation error: {str(e)}")

if __name__ == "__main__":
    if not TOKEN:
        print("❌ ERROR: DISCORD_BOT_TOKEN not found in .env file!")
        print("Please create a .env file with your Discord bot token")
    else:
        bot.run(TOKEN)
