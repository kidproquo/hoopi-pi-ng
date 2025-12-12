#!/bin/bash
# HoopiPi Service Deployment Script
# Installs and manages the systemd service for HoopiPi Web API

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_SERVICE_FILE="hoopi-pi-engine.service"
ENGINE_SERVICE_NAME="hoopi-pi-engine"
WEB_SERVICE_FILE="hoopi-pi-web.service"
WEB_SERVICE_NAME="hoopi-pi-web"

show_usage() {
    echo "Usage: $0 {install|uninstall|start|stop|restart|status|logs}"
    echo ""
    echo "Commands:"
    echo "  install    - Install and enable the systemd service"
    echo "  uninstall  - Stop and remove the systemd service"
    echo "  start      - Start the service"
    echo "  stop       - Stop the service"
    echo "  restart    - Restart the service"
    echo "  status     - Show service status"
    echo "  logs       - Show service logs (follow mode)"
    exit 1
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo "Error: This command requires root privileges"
        echo "Please run with sudo: sudo $0 $1"
        exit 1
    fi
}

install_service() {
    check_root "install"

    echo "Installing HoopiPi Services..."

    # Check if builds exist
    if [ ! -f "$SCRIPT_DIR/build/standalone/HoopiPi" ]; then
        echo "Error: HoopiPi engine not built. Run ./deploy-build.sh first."
        exit 1
    fi

    if [ ! -f "$SCRIPT_DIR/build/api-server/HoopiPiAPI" ]; then
        echo "Error: API server not built. Run ./deploy-build.sh first."
        exit 1
    fi

    # Copy service files
    cp "$SCRIPT_DIR/$ENGINE_SERVICE_FILE" /etc/systemd/system/
    cp "$SCRIPT_DIR/$WEB_SERVICE_FILE" /etc/systemd/system/

    # Reload systemd
    systemctl daemon-reload

    # Enable services
    systemctl enable $ENGINE_SERVICE_NAME
    systemctl enable $WEB_SERVICE_NAME

    echo "✓ Services installed and enabled"
    echo ""
    echo "To start the services:"
    echo "  sudo systemctl start $ENGINE_SERVICE_NAME"
    echo "  sudo systemctl start $WEB_SERVICE_NAME"
    echo ""
    echo "Or start both at once:"
    echo "  sudo $0 start"
    echo ""
    echo "To check status:"
    echo "  sudo $0 status"
}

uninstall_service() {
    check_root "uninstall"

    echo "Uninstalling HoopiPi Services..."

    # Stop services if running
    systemctl stop $WEB_SERVICE_NAME 2>/dev/null || true
    systemctl stop $ENGINE_SERVICE_NAME 2>/dev/null || true

    # Disable services
    systemctl disable $WEB_SERVICE_NAME 2>/dev/null || true
    systemctl disable $ENGINE_SERVICE_NAME 2>/dev/null || true

    # Remove service files
    rm -f /etc/systemd/system/$WEB_SERVICE_FILE
    rm -f /etc/systemd/system/$ENGINE_SERVICE_FILE

    # Reload systemd
    systemctl daemon-reload

    echo "✓ Services uninstalled"
}

start_service() {
    check_root "start"
    systemctl start $ENGINE_SERVICE_NAME
    systemctl start $WEB_SERVICE_NAME
    echo "✓ Services started"
    echo ""
    systemctl status $ENGINE_SERVICE_NAME --no-pager
    echo ""
    systemctl status $WEB_SERVICE_NAME --no-pager
}

stop_service() {
    check_root "stop"
    systemctl stop $WEB_SERVICE_NAME
    systemctl stop $ENGINE_SERVICE_NAME
    echo "✓ Services stopped"
}

restart_service() {
    check_root "restart"
    systemctl restart $ENGINE_SERVICE_NAME
    systemctl restart $WEB_SERVICE_NAME
    echo "✓ Services restarted"
    echo ""
    systemctl status $ENGINE_SERVICE_NAME --no-pager
    echo ""
    systemctl status $WEB_SERVICE_NAME --no-pager
}

status_service() {
    echo "=== HoopiPi Engine Status ==="
    systemctl status $ENGINE_SERVICE_NAME --no-pager
    echo ""
    echo "=== HoopiPi Web API Status ==="
    systemctl status $WEB_SERVICE_NAME --no-pager
}

logs_service() {
    echo "Following logs for both services (Ctrl+C to stop)..."
    echo ""
    journalctl -u $ENGINE_SERVICE_NAME -u $WEB_SERVICE_NAME -f
}

# Main script
case "${1:-}" in
    install)
        install_service
        ;;
    uninstall)
        uninstall_service
        ;;
    start)
        start_service
        ;;
    stop)
        stop_service
        ;;
    restart)
        restart_service
        ;;
    status)
        status_service
        ;;
    logs)
        logs_service
        ;;
    *)
        show_usage
        ;;
esac
