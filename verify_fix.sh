#!/bin/bash
"""
Quick verification script for DHT metadata extraction fix
"""

echo "🔍 DHT Metadata Extraction Fix Verification"
echo "=" * 50

# Check if the fix is in place
echo "1. Checking if upload_mode fix is applied..."
if grep -q "seed_mode" src/enhanced_metadata_manager.hpp; then
    echo "✅ Fix found: seed_mode is being used"
    if grep -q "upload_mode" src/enhanced_metadata_manager.hpp; then
        echo "⚠️  Warning: upload_mode still present (should be commented out)"
    else
        echo "✅ upload_mode properly removed/commented"
    fi
else
    echo "❌ Fix not found: seed_mode not detected"
    echo "   Make sure you have this line in enhanced_metadata_manager.hpp:"
    echo "   params.flags |= lt::torrent_flags::seed_mode;"
fi

echo ""
echo "2. Checking build configuration..."
if [ -f "builds/macos/arm/Debug/dht_crawler" ]; then
    echo "✅ Debug build found"
elif [ -f "builds/macos/x86/Debug/dht_crawler" ]; then
    echo "✅ Debug build found (x86)"
else
    echo "❌ No debug build found. Run: make debug-macos-arm"
fi

echo ""
echo "3. Checking test hashes file..."
if [ -f "test_info_hashes.txt" ]; then
    hash_count=$(wc -l < test_info_hashes.txt)
    echo "✅ Test hashes file found with $hash_count hashes"
else
    echo "❌ Test hashes file not found"
fi

echo ""
echo "4. Quick test command:"
echo "   ./test_metadata_fix.py"
echo "   or manually:"
echo "   ./builds/macos/arm/Debug/dht_crawler --user test --password test --database test --metadata \$(head -3 test_info_hashes.txt | tr '\n' ',') --debug --metadata_log"
