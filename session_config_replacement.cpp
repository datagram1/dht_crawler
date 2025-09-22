// AUTOMATED METADATA FIX - REPLACEMENT SESSION CONFIGURATION
// Copy this block to replace the session configuration in your DHTTorrentCrawler constructor

        // *** FIXED SESSION CONFIGURATION FOR BEP 9 METADATA EXCHANGE ***
        lt::session_params params;
        lt::settings_pack& settings = params.settings;
        
        // *** CRITICAL: Enable metadata extension (BEP 9) ***
        settings.set_bool(lt::settings_pack::enable_extensions, true);
        
        // DHT configuration
        settings.set_bool(lt::settings_pack::enable_dht, true);
        settings.set_int(lt::settings_pack::dht_announce_interval, 15);
        settings.set_int(lt::settings_pack::dht_bootstrap_nodes, 0);
        
        // *** FIXED: Extended alert mask for metadata ***
        settings.set_int(lt::settings_pack::alert_mask, 
            lt::alert_category::dht | 
            lt::alert_category::peer | 
            lt::alert_category::status |
            lt::alert_category::connect |
            lt::alert_category::error |
            lt::alert_category::torrent);
        
        // Port binding for peer connections
        settings.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881");
        
        // *** FIXED: Longer timeouts for metadata exchange ***
        settings.set_int(lt::settings_pack::handshake_timeout, 30);      // Increased from 10s
        settings.set_int(lt::settings_pack::peer_timeout, 180);         // Increased from 120s
        
        // *** CRITICAL: Enable peer exchange and local peer discovery ***
        settings.set_bool(lt::settings_pack::enable_lsd, true);
        settings.set_bool(lt::settings_pack::enable_peer_exchange, true);
        
        // Connection limits
        settings.set_int(lt::settings_pack::connections_limit, 200);
        settings.set_int(lt::settings_pack::active_limit, 1000);
        
        // *** CRITICAL: Enable TCP for metadata exchange ***
        settings.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        settings.set_bool(lt::settings_pack::enable_incoming_utp, true);
        settings.set_bool(lt::settings_pack::enable_outgoing_tcp, true);   // *** CRITICAL ***
        settings.set_bool(lt::settings_pack::enable_incoming_tcp, true);   // *** CRITICAL ***
        
        // Port forwarding
        settings.set_bool(lt::settings_pack::enable_upnp, true);
        settings.set_bool(lt::settings_pack::enable_natpmp, true);
        
        m_session = std::make_unique<lt::session>(params);
