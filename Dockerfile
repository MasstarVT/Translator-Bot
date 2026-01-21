FROM ubuntu:22.04 as builder

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libcurl4-openssl-dev \
    libssl-dev \
    zlib1g-dev \
    libsqlite3-dev \
    libopus-dev \
    libsodium-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source files
COPY CMakeLists.txt .
COPY include/ include/
COPY src/ src/

# Build the bot
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libcurl4 \
    libssl3 \
    zlib1g \
    libsqlite3-0 \
    libopus0 \
    libsodium23 \
    ffmpeg \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install yt-dlp
RUN pip3 install --no-cache-dir yt-dlp

# Create non-root user for security (UID 1000 matches common host user)
RUN groupadd -g 1000 botuser && useradd -u 1000 -g botuser -m botuser

# Create app directory and data directory
WORKDIR /app
RUN mkdir -p /app/data && chown -R botuser:botuser /app

# Copy the built executable and DPP library
COPY --from=builder /build/build/bin/discord-bot .
COPY --from=builder /build/build/_deps/dpp-build/library/libdpp.so* /usr/local/lib/

# Update library cache
RUN ldconfig

# Set ownership and permissions
RUN chown -R botuser:botuser /app && \
    chmod 755 /app/discord-bot

# Switch to non-root user
USER botuser

# Set environment variables
ENV DATABASE_PATH=/app/data/bot.db

# Volume for persistent data
VOLUME ["/app/data"]

# Run the bot
CMD ["./discord-bot"]
