#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_FILE="$DIR/config.json"
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

if [ "$#" -ne 1 ]; then
    get_msg "script_rm_cmd_usage" "Usage: rm command <command_path>"
    echo "  Example: rm command /show/ip"
    exit 1
fi

CMD_PATH="$1"

if [[ "$CMD_PATH" == "/configure" || "$CMD_PATH" == "/configure/"* ]]; then
    get_msg "script_cmd_restricted" "Error: The /configure tree is restricted and cannot be modified."
    exit 1
fi

if [[ "$CMD_PATH" == "/" || "$CMD_PATH" == "" ]]; then
    get_msg "script_cmd_root_rm" "Error: Cannot remove the root."
    exit 1
fi

TMP_FILE=$(mktemp)

jq --arg path "$CMD_PATH" '
  ($path | sub("^/"; "") | split("/")) as $parts |
  def rm_cmd($parts_left; $node):
    if ($parts_left | length) == 1 then
      if ($node | type) == "array" then
        map(select(.name != $parts_left[0]))
      else
        .subcommands = (.subcommands | map(select(.name != $parts_left[0])))
      end
    else
      if ($node | type) == "array" then
        map(if .name == $parts_left[0] then rm_cmd($parts_left[1:]; .) else . end)
      else
        .subcommands = (.subcommands | map(if .name == $parts_left[0] then rm_cmd($parts_left[1:]; .) else . end))
      end
    end;
  .commands = (.commands | rm_cmd($parts; .))
' "$CONFIG_FILE" > "$TMP_FILE"

if [ $? -eq 0 ]; then
    cat "$TMP_FILE" > "$CONFIG_FILE"
    get_msg "script_cmd_removed" "Command removed successfully. Run 'reload conf' to apply."
else
    get_msg "script_cmd_error" "Error: Failed to modify config file."
fi
rm -f "$TMP_FILE"
