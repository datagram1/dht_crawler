#!/bin/bash

# Database Migration Runner Script
# This script runs the database migrations for the magnetico upgrade

set -e

# Configuration
DB_HOST="192.168.10.100"
DB_PORT="3306"
DB_USER="keynetworks"
DB_PASSWORD="K3yn3tw0rk5"
DB_NAME="Torrents"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if MySQL is available
check_mysql() {
    print_status "Checking MySQL connection..."
    if ! mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" -e "SELECT 1;" "$DB_NAME" >/dev/null 2>&1; then
        print_error "Cannot connect to MySQL database"
        exit 1
    fi
    print_status "MySQL connection successful"
}

# Function to run a migration file
run_migration() {
    local migration_file="$1"
    local migration_name=$(basename "$migration_file" .sql)
    
    print_status "Running migration: $migration_name"
    
    if [ ! -f "$migration_file" ]; then
        print_error "Migration file not found: $migration_file"
        exit 1
    fi
    
    # Run the migration
    if mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" < "$migration_file"; then
        print_status "Migration $migration_name completed successfully"
    else
        print_error "Migration $migration_name failed"
        exit 1
    fi
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --create-tables    Create enhanced tables"
    echo "  --migrate-data     Migrate existing data"
    echo "  --rollback         Rollback migration"
    echo "  --all              Run all migrations"
    echo "  --help             Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --create-tables"
    echo "  $0 --migrate-data"
    echo "  $0 --all"
    echo "  $0 --rollback"
}

# Main function
main() {
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    
    # Check if no arguments provided
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi
    
    # Check MySQL connection
    check_mysql
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --create-tables)
                print_status "Creating enhanced tables..."
                run_migration "$script_dir/001_create_enhanced_tables.sql"
                shift
                ;;
            --migrate-data)
                print_status "Migrating existing data..."
                run_migration "$script_dir/002_migrate_existing_data.sql"
                shift
                ;;
            --rollback)
                print_warning "Rolling back migration..."
                read -p "Are you sure you want to rollback? This will delete the enhanced tables. (y/N): " -n 1 -r
                echo
                if [[ $REPLY =~ ^[Yy]$ ]]; then
                    run_migration "$script_dir/003_rollback_migration.sql"
                else
                    print_status "Rollback cancelled"
                fi
                shift
                ;;
            --all)
                print_status "Running all migrations..."
                run_migration "$script_dir/001_create_enhanced_tables.sql"
                run_migration "$script_dir/002_migrate_existing_data.sql"
                print_status "All migrations completed successfully"
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
}

# Run main function with all arguments
main "$@"
