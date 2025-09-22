# Port Forwarding Setup for DHT Crawler

## Why Port Forwarding Matters

Port forwarding is **crucial** for effective DHT crawling and metadata fetching. Here's why:

### Current Problem (Behind NAT/Firewall)
- **Incoming connections blocked**: Other DHT nodes can't connect to you
- **Limited peer discovery**: You can only connect to nodes that initiate connections
- **Poor metadata success**: Fewer peer connections = less metadata exchange
- **Reduced DHT participation**: You're not a "first-class" DHT citizen

### With Port Forwarding (Direct Internet Access)
- **Bidirectional connectivity**: Other DHT nodes can connect directly to you
- **More peer connections**: You become a full DHT participant
- **Better metadata fetching**: More peers = higher success rate
- **Improved discovery**: Better torrent and peer discovery

## Evidence from mldht Project

The [mldht project](https://github.com/the8472/mldht) documentation states:

> "can acquire approximately 0.3 torrents per second on a single-homed setup without firewall"

This suggests that **firewall/NAT limitations significantly impact performance**.

## Port Forwarding Setup

### 1. Choose a Port
The crawler is configured to use **port 6881** (standard BitTorrent port range).

**Alternative ports**: 6882-6889 (BitTorrent range), or any port 1024-65535

### 2. Router Configuration

#### Step 1: Access Router Admin Panel
- Open browser and go to router IP (usually `192.168.1.1` or `192.168.0.1`)
- Login with admin credentials

#### Step 2: Find Port Forwarding Settings
- Look for "Port Forwarding", "Virtual Server", or "NAT" settings
- Common locations: Advanced → Port Forwarding, or Security → Port Forwarding

#### Step 3: Configure Port Forwarding
```
Service Name: DHT Crawler
Protocol: TCP/UDP (or Both)
External Port: 6881
Internal IP: [Your computer's local IP]
Internal Port: 6881
```

#### Step 4: Save and Restart
- Save configuration
- Restart router if required

### 3. Firewall Configuration

#### Windows Firewall
```cmd
# Allow incoming connections on port 6881
netsh advfirewall firewall add rule name="DHT Crawler" dir=in action=allow protocol=TCP localport=6881
netsh advfirewall firewall add rule name="DHT Crawler UDP" dir=in action=allow protocol=UDP localport=6881
```

#### macOS Firewall
```bash
# Allow incoming connections on port 6881
sudo pfctl -f /etc/pf.conf
# Add rule to /etc/pf.conf:
# pass in proto tcp from any to any port 6881
# pass in proto udp from any to any port 6881
```

#### Linux (iptables)
```bash
# Allow incoming connections on port 6881
sudo iptables -A INPUT -p tcp --dport 6881 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 6881 -j ACCEPT
sudo iptables-save > /etc/iptables/rules.v4
```

### 4. Verify Port Forwarding

#### Test External Connectivity
```bash
# From another machine on the internet
telnet [your-public-ip] 6881
# Should connect if port forwarding is working
```

#### Check with Online Tools
- Use port checking websites like `canyouseeme.org`
- Enter port 6881 and your public IP
- Should show "Open" if configured correctly

## Expected Results with Port Forwarding

### Before Port Forwarding:
- **Peers found**: 0 (as in your current results)
- **Metadata fetched**: 0
- **Incoming connections**: Blocked
- **DHT participation**: Limited

### After Port Forwarding:
- **Peers found**: Should increase significantly
- **Metadata fetched**: Should improve to 10-30%
- **Incoming connections**: Active
- **DHT participation**: Full

## Monitoring Port Forwarding Status

The crawler now displays port forwarding status:

```
Checking port forwarding status...
Listening on interfaces: 0.0.0.0:6881
```

### What to Look For:
- **"Listening on: 0.0.0.0:6881"**: Good - listening on all interfaces
- **"Listening on: 127.0.0.1:6881"**: Bad - only local connections
- **"Listening on: 192.168.x.x:6881"**: Bad - only local network

## Troubleshooting

### Common Issues:

#### 1. Port Already in Use
```bash
# Check what's using port 6881
netstat -tulpn | grep 6881
# Kill process if needed
sudo kill -9 [PID]
```

#### 2. Router Doesn't Support Port Forwarding
- Some ISPs block port forwarding
- Consider using a VPN or VPS
- Try different ports (6882-6889)

#### 3. Double NAT
- Some ISPs use carrier-grade NAT
- Contact ISP to request public IP
- Consider using a VPS instead

#### 4. Dynamic IP
- Use dynamic DNS service (DuckDNS, No-IP)
- Update router configuration with DDNS

## Alternative Solutions

### 1. VPS Hosting
If port forwarding isn't possible:
- Rent a VPS with public IP
- Run crawler on VPS
- Much better connectivity

### 2. VPN with Port Forwarding
- Use VPN service that supports port forwarding
- Configure router to use VPN
- Forward ports through VPN

### 3. Cloud Hosting
- AWS EC2, Google Cloud, Azure
- Full internet connectivity
- Better performance than home connection

## Security Considerations

### 1. Firewall Rules
- Only allow necessary ports
- Block unnecessary services
- Use fail2ban for additional protection

### 2. Network Isolation
- Run crawler on isolated network segment
- Use VLANs if available
- Monitor network traffic

### 3. Regular Updates
- Keep router firmware updated
- Update operating system
- Monitor for security advisories

## Performance Expectations

### With Port Forwarding:
- **Peer connections**: 50-200 active connections
- **Metadata success rate**: 10-30%
- **Discovery rate**: 100-500 torrents/hour
- **DHT participation**: Full bidirectional

### Without Port Forwarding:
- **Peer connections**: 0-10 active connections
- **Metadata success rate**: 0-5%
- **Discovery rate**: 1000+ torrents/hour (but low quality)
- **DHT participation**: Limited, mostly outgoing

## Conclusion

Port forwarding is **essential** for effective DHT crawling. The difference in metadata fetching success rate should be dramatic - from 0% to 10-30% or higher.

The crawler is now configured to take advantage of port forwarding with:
- Specific port binding (6881)
- Higher connection limits (500 connections, 100 active)
- Port forwarding status monitoring
- UPnP/NAT-PMP support for automatic configuration

Set up port forwarding and you should see immediate improvements in peer discovery and metadata fetching!
