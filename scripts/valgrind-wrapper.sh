#!/bin/sh
# Wrapper to run odbcserver under Valgrind for memory leak detection.
# Linux only — Valgrind is not available on macOS ARM.
#
# Usage:
#   Set ODBC_SERVER_WRAPPER to this script's path before starting
#   the Erlang ODBC application, or symlink this in place of
#   priv/bin/odbcserver.
#
# Example:
#   export ODBC_SERVER_WRAPPER=/path/to/scripts/valgrind-wrapper.sh
#   rebar3 ct --suite=test/odbc_data_type_SUITE
#
# The wrapper passes through all arguments and stdin/stdout to
# the real odbcserver binary.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASEDIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ODBCSERVER="$BASEDIR/priv/bin/odbcserver"

SUPPRESSIONS=""
if [ -f "$SCRIPT_DIR/valgrind.supp" ]; then
    SUPPRESSIONS="--suppressions=$SCRIPT_DIR/valgrind.supp"
fi

exec valgrind \
    --leak-check=full \
    --show-reachable=yes \
    --track-origins=yes \
    --error-exitcode=42 \
    --log-file="$BASEDIR/valgrind-%p.log" \
    $SUPPRESSIONS \
    "$ODBCSERVER" "$@"
