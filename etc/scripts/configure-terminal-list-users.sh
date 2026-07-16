#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LANG_FILE="$DIR/lang_${AETHERCLI_LANG:-en}.json"

get_msg() {
    local key="$1"
    local default_msg="$2"
    if [ -f "$LANG_FILE" ]; then
        local val=$(jq -r ".$key // \"$default_msg\"" "$LANG_FILE" 2>/dev/null)
        if [ "$val" != "null" ]; then echo "$val"; return; fi
    fi
    echo "$default_msg"
}

USERS_FILE="$DIR/users.json"

if [ ! -f "$USERS_FILE" ]; then
    get_msg "script_users_none" "No users configured (users.json not found)."
    exit 0
fi

get_msg "script_users_list" "Configured users:"
jq -r '.users[] | .username' "$USERS_FILE" | while read -r user; do
    echo " - $user"
done
