# DHT Crawler Error Logging System

## Overview

The DHT Crawler now includes a comprehensive error logging system that captures crashes, exceptions, and errors in the MySQL database. This system helps with debugging, monitoring, and maintaining the application.

## Database Schema

### Log Table Structure

The `log` table is automatically created when the application starts and contains the following fields:

```sql
CREATE TABLE IF NOT EXISTS log (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
    function_name VARCHAR(255) NOT NULL,
    caller_function VARCHAR(255) NULL,
    error_code INT NULL,
    error_message TEXT NULL,
    stack_trace TEXT NULL,
    severity ENUM('DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL') DEFAULT 'ERROR' NULL,
    thread_id VARCHAR(50) NULL,
    process_id INT NULL,
    additional_data TEXT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
    INDEX idx_timestamp (timestamp),
    INDEX idx_function_name (function_name),
    INDEX idx_caller_function (caller_function),
    INDEX idx_error_code (error_code),
    INDEX idx_severity (severity),
    INDEX idx_thread_id (thread_id),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

### Field Descriptions

- **id**: Auto-incrementing primary key
- **timestamp**: When the error occurred (automatically set)
- **function_name**: The function where the error occurred (required)
- **caller_function**: The function that called the function where the error occurred
- **error_code**: Numeric error code (0 for success, negative for exceptions, positive for system errors)
- **error_message**: Human-readable error description
- **stack_trace**: Stack trace information (if available)
- **severity**: Error severity level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
- **thread_id**: Thread identifier where the error occurred
- **process_id**: Process identifier
- **additional_data**: Additional context information (JSON or key-value pairs)
- **created_at**: Record creation timestamp

## Usage

### C++ API

The error logging system provides two main methods in the `MySQLConnection` class:

#### logError()
```cpp
bool logError(const std::string& function_name, 
              const std::string& caller_function = "",
              int error_code = 0,
              const std::string& error_message = "",
              const std::string& stack_trace = "",
              const std::string& severity = "ERROR",
              const std::string& additional_data = "");
```

#### logException()
```cpp
bool logException(const std::string& function_name,
                  const std::string& caller_function,
                  const std::exception& e,
                  const std::string& additional_data = "");
```

### Example Usage

```cpp
try {
    // Some operation that might fail
    if (!mysql->storeTorrent(torrent)) {
        mysql->logError("processTorrent", "main", -1, "Failed to store torrent", "", "ERROR", 
                       "info_hash=" + torrent.info_hash);
    }
} catch (const std::exception& e) {
    mysql->logException("processTorrent", "main", e, "torrent_id=" + torrent.info_hash);
}
```

## Implemented Error Logging

The following critical functions now include error logging:

### MySQLConnection Class
- `connect()` - Database connection errors
- `storeTorrent()` - Torrent storage errors
- `markTorrentTimedOut()` - Timeout marking errors

### DHTTorrentCrawler Class
- `initialize()` - Initialization errors
- `handleMetadataReceived()` - Metadata processing errors

### Main Function
- Global exception handling with database logging

## Testing the Logging System

### 1. Compile and Run Test
```bash
make test-logging
./test_logging
```

### 2. Check Logs in Database
```bash
mysql -h 192.168.10.100 -u keynetworks -pK3yn3tw0rk5 Torrents < check_logs.sql
```

### 3. Manual Database Query
```sql
-- Connect to database
mysql -h 192.168.10.100 -u keynetworks -pK3yn3tw0rk5 Torrents

-- View recent logs
SELECT * FROM log ORDER BY timestamp DESC LIMIT 10;

-- Count errors by severity
SELECT severity, COUNT(*) FROM log GROUP BY severity;
```

## Error Severity Levels

- **DEBUG**: Debugging information
- **INFO**: General information messages
- **WARNING**: Warning conditions that don't stop execution
- **ERROR**: Error conditions that may affect functionality
- **CRITICAL**: Critical errors that may cause application failure

## Database Connection Details

The logging system uses the same database connection as the main application:

- **Host**: 192.168.10.100
- **Port**: 3306
- **Database**: Torrents
- **Username**: keynetworks
- **Password**: K3yn3tw0rk5

## Monitoring and Maintenance

### Regular Monitoring Queries

```sql
-- Check for critical errors in the last hour
SELECT COUNT(*) FROM log 
WHERE severity = 'CRITICAL' 
AND timestamp >= DATE_SUB(NOW(), INTERVAL 1 HOUR);

-- Most common error functions
SELECT function_name, COUNT(*) as error_count 
FROM log 
WHERE timestamp >= DATE_SUB(NOW(), INTERVAL 24 HOUR)
GROUP BY function_name 
ORDER BY error_count DESC 
LIMIT 10;

-- Error trends over time
SELECT DATE(timestamp) as date, severity, COUNT(*) as count
FROM log 
WHERE timestamp >= DATE_SUB(NOW(), INTERVAL 7 DAY)
GROUP BY DATE(timestamp), severity
ORDER BY date DESC, severity;
```

### Log Cleanup

Consider implementing log rotation to prevent the log table from growing too large:

```sql
-- Delete logs older than 30 days
DELETE FROM log WHERE timestamp < DATE_SUB(NOW(), INTERVAL 30 DAY);

-- Archive old logs to a separate table
CREATE TABLE log_archive LIKE log;
INSERT INTO log_archive SELECT * FROM log WHERE timestamp < DATE_SUB(NOW(), INTERVAL 30 DAY);
DELETE FROM log WHERE timestamp < DATE_SUB(NOW(), INTERVAL 30 DAY);
```

## Benefits

1. **Debugging**: Easy identification of where and why errors occur
2. **Monitoring**: Track error patterns and frequency
3. **Maintenance**: Proactive identification of issues
4. **Performance**: Monitor function call patterns and error rates
5. **Reliability**: Better understanding of application behavior under different conditions

## Future Enhancements

Potential improvements to consider:

1. **Log Levels**: Configurable logging levels
2. **Remote Logging**: Send logs to external systems
3. **Alerting**: Automatic notifications for critical errors
4. **Metrics**: Integration with monitoring systems
5. **Log Analysis**: Automated error pattern detection
