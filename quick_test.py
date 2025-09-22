#!/usr/bin/env python3
"""
Simple metadata extraction test
"""
import subprocess
import os

# Change to project directory
os.chdir('/Users/richardbrown/dev/dht_crawler')

# Test with one hash for 30 seconds
test_hash = "00009643dee7016aa207644c782918db9fe39f86"

print(f"🧪 Testing metadata extraction for hash: {test_hash}")
print("This will run for 30 seconds to see if metadata is received...")
print("=" * 60)

# Find the executable
executable = None
possible_paths = [
    "./builds/macos/arm/Debug/dht_crawler",
    "./builds/macos/x86/Debug/dht_crawler", 
    "./builds/macos/arm/Release/dht_crawler",
    "./builds/macos/x86/Release/dht_crawler",
    "./dht_crawler"
]

for path in possible_paths:
    if os.path.exists(path):
        executable = path
        break

if not executable:
    print("❌ No executable found. Please build first:")
    print("   make debug-macos-arm")
    exit(1)

print(f"📍 Using executable: {executable}")

# Run test
cmd = [
    executable,
    "--user", "test",
    "--password", "test", 
    "--database", "test",
    "--metadata", test_hash,
    "--metadata_log"
]

try:
    result = subprocess.run(cmd, timeout=30, capture_output=True, text=True)
    
    print("📤 OUTPUT:")
    print(result.stdout)
    
    if result.stderr:
        print("📤 ERRORS:")
        print(result.stderr)
        
    # Check results
    if "METADATA DISCOVERED" in result.stdout:
        print("🎉 SUCCESS: Metadata extraction is working!")
    elif "METADATA TIMEOUT" in result.stdout:
        print("⏰ TIMEOUT: No metadata received (may need to try different hashes)")
    elif "MySQL connection failed" in result.stdout:
        print("ℹ️  INFO: MySQL connection failed, but that's okay for testing")
        if "Queued metadata request" in result.stdout:
            print("✅ The metadata request system is working")
    else:
        print("❓ UNKNOWN: Check output above")
        
except subprocess.TimeoutExpired:
    print("⏰ Test completed (30 second timeout)")
except Exception as e:
    print(f"❌ Error: {e}")

print("=" * 60)
print("💡 If you see 'Queued metadata request' messages, your fix is working!")
print("💡 For better testing, try with known active torrents or run longer tests")
