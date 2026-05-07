#!/bin/sh

DAEMON="aesdsocket"
DAEMON_PATH="/usr/bin/aesdsocket"

case "$1" in
    start)
        echo "Starting $DAEMON"
        # Start as a daemon (-d passed to aesdsocket)
        start-stop-daemon -S -n $DAEMON -a $DAEMON_PATH -- -d
        ;;
    stop)
        echo "Stopping $DAEMON"
        start-stop-daemon -K -n $DAEMON
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0