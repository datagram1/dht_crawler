#!/bin/bash

# Build script for Linux x86 using Docker
set -e

echo "=== Building Linux x86 Release with Docker ==="

# Build the Docker image
echo "Building Docker image for Linux x86..."
docker build -f Dockerfile.linux-x86 -t dht-crawler-linux-x86 .

# Create output directory
mkdir -p builds/linux/x86/Release

# Copy the built executable from Docker container
echo "Extracting built executable..."
docker create --name temp-container dht-crawler-linux-x86
docker cp temp-container:/app/builds/linux/x86/Release/dht_crawler builds/linux/x86/Release/
docker rm temp-container

# Make executable
chmod +x builds/linux/x86/Release/dht_crawler

# Verify the build
echo "Verifying Linux x86 build..."
file builds/linux/x86/Release/dht_crawler
echo ""
echo "Dependencies:"
ldd builds/linux/x86/Release/dht_crawler 2>/dev/null || echo "ldd not available on macOS - this is normal"

echo ""
echo "✅ Linux x86 build completed!"
echo "Executable: builds/linux/x86/Release/dht_crawler"
