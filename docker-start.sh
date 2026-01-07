#!/bin/bash

# Discord Translation Bot - Docker Quick Start

echo "🐋 Discord Translation Bot - Docker Setup"
echo "=========================================="
echo ""

# Check if .env file exists
if [ ! -f .env ]; then
    echo "⚠️  .env file not found!"
    echo "Creating .env from .env.example..."
    cp .env.example .env
    echo ""
    echo "📝 Please edit .env and add your Discord bot token:"
    echo "   DISCORD_BOT_TOKEN=your_bot_token_here"
    echo ""
    echo "Then run this script again."
    exit 1
fi

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "❌ Docker is not installed!"
    echo "Please install Docker: https://docs.docker.com/get-docker/"
    exit 1
fi

# Check if Docker Compose is installed
if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo "❌ Docker Compose is not installed!"
    echo "Please install Docker Compose: https://docs.docker.com/compose/install/"
    exit 1
fi

echo "✅ Docker is installed"
echo ""
echo "🔨 Building Docker image..."
docker-compose build

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build complete!"
    echo ""
    echo "🚀 Starting bot..."
    docker-compose up -d
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "✅ Bot is running!"
        echo ""
        echo "📋 Useful commands:"
        echo "  • View logs:        docker-compose logs -f"
        echo "  • Stop bot:         docker-compose down"
        echo "  • Restart bot:      docker-compose restart"
        echo "  • View status:      docker-compose ps"
        echo ""
    else
        echo ""
        echo "❌ Failed to start bot"
        echo "Check your .env file and try again"
    fi
else
    echo ""
    echo "❌ Build failed"
    echo "Check the error messages above"
fi
