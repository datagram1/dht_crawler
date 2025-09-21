#!/bin/bash

# Build script for Windows x86 using Docker
set -e

echo "=== Building Windows x86 Release with Docker ==="

# Check if Docker Desktop is running
if ! docker info > /dev/null 2>&1; then
    echo "❌ Docker is not running. Please start Docker Desktop."
    exit 1
fi

# Build the Docker image
echo "Building Docker image for Windows x86..."
docker build -f Dockerfile.windows-x86 -t dht-crawler-windows-x86 .

# Create output directory
mkdir -p builds/windows/x86/Release

# Copy the built executable from Docker container
echo "Extracting built executable..."
docker create --name temp-windows-container dht-crawler-windows-x86
docker cp temp-windows-container:/app/builds/windows/x86/Release/dht_crawler.exe builds/windows/x86/Release/
docker rm temp-windows-container

# Verify the build
echo "Verifying Windows x86 build..."
if [ -f "builds/windows/x86/Release/dht_crawler.exe" ]; then
    echo "✅ Windows x86 build completed!"
    echo "Executable: builds/windows/x86/Release/dht_crawler.exe"
    ls -la builds/windows/x86/Release/dht_crawler.exe
else
    echo "❌ Build failed - executable not found"
    exit 1
fi
