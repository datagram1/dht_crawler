/*
 * Fixed Session Configuration for BEP 9 Metadata Extension
 * Add this to your DHTTorrentCrawler constructor in dht_crawler.cpp
 */

// *** FIXED SESSION CONFIGURATION FOR METADATA EXCHANGE ***
// Replace your current session configuration with this:

// Initialize libtorrent session with proper BEP 9 support
lt::session_params params;
lt::settings_pack& settings = params.settings;

// *** CRITICAL: Enable metadata extension (BEP 9) ***
settings.set_bool(lt::settings_pack::enable_extensions, true);
settings.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
settings.set_bool(lt::settings_pack::enable_incoming_tcp, true);

// DHT configuration
settings.set_bool(lt::settings_pack::enable_dht, true);
settings.set_int(lt::settings_pack::dht_announce_interval, 15);
settings.set_int(lt::settings_pack::dht_bootstrap_nodes, 0);

// *** FIXED: Extended alert mask to include metadata alerts ***
settings.set_int(lt::settings_pack::alert_mask, 
    lt::alert_category::dht | 
    lt::alert_category::peer | 
    lt::alert_category::status |
    lt::alert_category::connect |
    lt::alert_category::error |
    lt::alert_category::torrent |          // *** NEW: For torrent status ***
    lt::alert_category::piece_progress |   // *** NEW: For metadata progress ***
    lt::alert_category::port_mapping);     // *** NEW: For port mapping ***

// Port binding for peer connections
settings.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881");

// *** FIXED: More generous timeout settings for metadata exchange ***
settings.set_int(lt::settings_pack::handshake_timeout, 30);     // Increased from 10 to 30 seconds
settings.set_int(lt::settings_pack::peer_timeout, 180);        // Increased from 120 to 180 seconds
settings.set_int(lt::settings_pack::inactivity_timeout, 90);   // *** NEW: 90 seconds for inactive peers ***

// *** CRITICAL: Enable peer exchange and local peer discovery ***
settings.set_bool(lt::settings_pack::enable_lsd, true);
settings.set_bool(lt::settings_pack::enable_peer_exchange, true);  // *** NEW: Enable PEX ***

// Connection limits
settings.set_int(lt::settings_pack::connections_limit, 200);
settings.set_int(lt::settings_pack::active_limit, 1000);

// *** FIXED: Proper UTP/TCP configuration for metadata exchange ***
settings.set_bool(lt::settings_pack::enable_outgoing_utp, true);
settings.set_bool(lt::settings_pack::enable_incoming_utp, true);
settings.set_bool(lt::settings_pack::enable_outgoing_tcp, true);   // *** CRITICAL: Enable TCP ***
settings.set_bool(lt::settings_pack::enable_incoming_tcp, true);   // *** CRITICAL: Enable TCP ***

// Port forwarding
settings.set_bool(lt::settings_pack::enable_upnp, true);
settings.set_bool(lt::settings_pack::enable_natpmp, true);

// *** NEW: Metadata-specific settings ***
settings.set_int(lt::settings_pack::max_out_request_queue, 1000);      // Large request queue
settings.set_int(lt::settings_pack::max_allowed_in_request_queue, 500); // Allow incoming requests
settings.set_int(lt::settings_pack::request_timeout, 30);              // 30 second request timeout
settings.set_int(lt::settings_pack::piece_timeout, 60);                // 60 second piece timeout

// *** CRITICAL: Enable extensions for metadata ***
settings.set_bool(lt::settings_pack::enable_extensions, true);
