#!/usr/bin/env python3
"""
DHT Metadata Extraction Diagnostic Tool
Identifies why peer discovery is failing
"""
import subprocess
import sys
import time
import os

def run_diagnostic():
    print("🔍 DHT Metadata Extraction Diagnostic")
    print("=" * 60)
    
    # Test 1: Check if DHT is working at all
    print("1. Testing DHT bootstrap and basic connectivity...")
    print("   Running 2-minute test with verbose DHT logging...")
    
    os.chdir('/Users/richardbrown/dev/dht_crawler')
    
    # Single hash test with maximum debugging
    test_hash = "00009643dee7016aa207644c782918db9fe39f86"
    
    cmd = [
        "./builds/macos/arm/Release/dht_crawler",
        "--user", "test",
        "--password", "test", 
        "--database", "test",
        "--metadata", test_hash,
        "--debug",
        "--verbose"  # Full verbose mode
    ]
    
    print(f"Command: {' '.join(cmd)}")
    print("=" * 60)
    
    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        # Monitor for specific issues
        dht_bootstrap = False
        peers_found = False
        metadata_requests = False
        alerts_seen = False
        
        start_time = time.time()
        
        while time.time() - start_time < 120:  # 2 minute test
            try:
                line = process.stdout.readline()
                if not line:
                    if process.poll() is not None:
                        break
                    time.sleep(0.1)
                    continue
                
                print(line.strip())
                
                # Check for key indicators
                if "DHT Bootstrap completed" in line:
                    dht_bootstrap = True
                    print("✅ DHT Bootstrap detected!")
                    
                elif "peers" in line.lower() and any(x in line for x in ["found", "reply", "discovered"]):
                    peers_found = True
                    print("✅ Peer discovery detected!")
                    
                elif "metadata request" in line.lower() or "queued metadata" in line.lower():
                    metadata_requests = True
                    print("✅ Metadata requests detected!")
                    
                elif "alert" in line.lower() or "processing" in line and "alerts" in line:
                    alerts_seen = True
                    
                # Look for specific error patterns
                if "failed" in line.lower() or "error" in line.lower():
                    print(f"⚠️  Issue detected: {line.strip()}")
                    
            except Exception as e:
                print(f"Error reading output: {e}")
                break
        
        # Kill process
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
        
        # Analyze results
        print("=" * 60)
        print("📊 DIAGNOSTIC RESULTS:")
        print(f"   DHT Bootstrap: {'✅ Working' if dht_bootstrap else '❌ Failed'}")
        print(f"   Peer Discovery: {'✅ Working' if peers_found else '❌ Failed'}")
        print(f"   Metadata Requests: {'✅ Working' if metadata_requests else '❌ Failed'}")
        print(f"   Alert Processing: {'✅ Working' if alerts_seen else '❌ Failed'}")
        
        print("\n🔧 RECOMMENDED FIXES:")
        
        if not dht_bootstrap:
            print("❌ DHT Bootstrap Issue:")
            print("   - Check internet connectivity")
            print("   - Verify DHT routers are reachable")
            print("   - Check firewall/NAT settings")
            print("   - Try manual DHT router addition")
            
        if not peers_found:
            print("❌ Peer Discovery Issue:")
            print("   - DHT may not be working properly")
            print("   - Try using trackers in addition to DHT")
            print("   - Check if hashes are for active torrents")
            print("   - Verify port 6881 is accessible")
            
        if not metadata_requests:
            print("❌ Metadata Request Issue:")
            print("   - Session configuration problem")
            print("   - Magnet link parsing issue")
            print("   - Review enhanced_metadata_manager.hpp")
            
        if not alerts_seen:
            print("❌ Alert Processing Issue:")
            print("   - libtorrent session not working")
            print("   - Alert mask configuration problem")
            print("   - Build/library issue")
            
        # Additional network test
        print("\n🌐 NETWORK CONNECTIVITY TEST:")
        test_network_connectivity()
        
    except Exception as e:
        print(f"❌ Diagnostic failed: {e}")

def test_network_connectivity():
    """Test basic network connectivity to DHT routers"""
    routers = [
        "router.utorrent.com",
        "router.bittorrent.com", 
        "dht.transmissionbt.com"
    ]
    
    for router in routers:
        try:
            result = subprocess.run(
                ["ping", "-c", "1", "-W", "3000", router], 
                capture_output=True, 
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                print(f"   ✅ {router}: Reachable")
            else:
                print(f"   ❌ {router}: Unreachable")
        except:
            print(f"   ❓ {router}: Test failed")

if __name__ == "__main__":
    run_diagnostic()
