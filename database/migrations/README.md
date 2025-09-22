# Database Migrations

This directory contains database migration scripts for the magnetico upgrade of the DHT crawler.

## Overview

The migration process enhances the existing database schema with new tables and features inspired by the Tribler project, while maintaining backward compatibility with existing data.

## Migration Files

### 001_create_enhanced_tables.sql
Creates the new enhanced tables:
- `TorrentMetadata` - Enhanced torrent metadata with validation
- `TrackerState` - Tracker state tracking
- `PeerState` - Peer state management
- `DHTNodeState` - DHT node quality tracking
- `CrawlSession` - Crawl session management
- `MetadataRequest` - Metadata request tracking

### 002_migrate_existing_data.sql
Migrates existing data from old tables to new enhanced tables:
- Migrates torrent data from `torrents` to `TorrentMetadata`
- Migrates peer data from `discovered_peers` to `PeerState`
- Migrates file data from `torrent_files` to `TorrentMetadata.file_list` JSON
- Creates a migration session record

### 003_rollback_migration.sql
Rolls back the migration by dropping the enhanced tables.

## Usage

### Running Migrations

```bash
# Create enhanced tables only
./run_migrations.sh --create-tables

# Migrate existing data only
./run_migrations.sh --migrate-data

# Run all migrations
./run_migrations.sh --all

# Rollback migration
./run_migrations.sh --rollback
```

### Checking Migration Status

```bash
# Check migration status
mysql -h 192.168.10.100 -u keynetworks -pK3yn3tw0rk5 Torrents < check_migration_status.sql
```

## Database Configuration

The migration scripts use the following database configuration:
- Host: 192.168.10.100
- Port: 3306
- User: keynetworks
- Password: K3yn3tw0rk5
- Database: Torrents

## Migration Process

1. **Create Enhanced Tables**: Creates new tables with enhanced schema
2. **Migrate Existing Data**: Transfers data from old tables to new tables
3. **Verify Migration**: Check that all data has been migrated correctly

## Rollback Process

The rollback process removes the enhanced tables but preserves the original tables. If you need to restore data from enhanced tables to original tables, you would need to create a separate restoration script.

## Notes

- The migration preserves all existing data
- Original tables are not modified during migration
- Enhanced tables provide additional features and better performance
- The migration can be run multiple times safely (idempotent)

## Troubleshooting

### Common Issues

1. **Connection Failed**: Check database credentials and network connectivity
2. **Permission Denied**: Ensure the user has CREATE, INSERT, UPDATE, DELETE permissions
3. **Data Type Mismatch**: Check that existing data is compatible with new schema

### Recovery

If migration fails:
1. Check the error message
2. Fix the issue
3. Re-run the migration (it's idempotent)
4. If rollback is needed, use the rollback script

## Support

For issues with migrations, check:
1. Database logs
2. Migration script output
3. Data compatibility
4. Database permissions
