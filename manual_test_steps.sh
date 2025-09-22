#!/bin/bash

echo "🔍 Quick DHT Test - Manual Steps"
echo "Run these commands to identify the issue:"
echo ""

echo "1. Test basic DHT bootstrap (run for 1 minute):"
echo "   cd ~/dev/dht_crawler"
echo "   timeout 60 ./builds/macos/arm/Release/dht_crawler --user test --password test --database test --metadata 00009643dee7016aa207644c782918db9fe39f86 --debug --verbose 2>&1 | grep -E '(DHT|Bootstrap|Alert|Peer|Error)'"
echo ""

echo "2. Test network connectivity:"
echo "   ping -c 1 router.utorrent.com"
echo "   ping -c 1 router.bittorrent.com"
echo "   ping -c 1 dht.transmissionbt.com"
echo ""

echo "3. Check if port 6881 is accessible:"
echo "   netstat -an | grep 6881"
echo ""

echo "4. Test with a known popular torrent (Ubuntu ISO):"
echo "   # Ubuntu 20.04 LTS hash: b846072e47d3560b8e86cf37d7c9d87e8781b6e3"
echo "   ./builds/macos/arm/Release/dht_crawler --metadata b846072e47d3560b8e86cf37d7c9d87e8781b6e3 --debug --verbose"
echo ""

echo "Expected outputs to look for:"
echo "   ✅ 'DHT Bootstrap completed'"
echo "   ✅ 'Alert type: X - [message]'"
echo "   ✅ 'Listening on: 0.0.0.0:6881'"
echo "   ✅ 'peers()' or 'peer_address'"
echo ""

echo "Common issues:"
echo "   ❌ No DHT Bootstrap = Network/firewall issue"
echo "   ❌ No alerts = libtorrent session not working"
echo "   ❌ 'Total queries sent: 0' = DHT queries not being sent"
