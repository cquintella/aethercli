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

if [ "$#" -lt 3 ]; then
    get_msg "script_add_cmd_usage" "Usage: add command <parent_path> <name> <short_desc> [activation] [syntax]"
    echo "  Use / for root level. Use quotes if arguments contain spaces."
    exit 1
fi

PARENT_PATH="$1"
NAME="$2"
SHORT_DESC="$3"
ACTIVATION="${4:-}"
SYNTAX="${5:-}"

if [[ "$PARENT_PATH" == "/configure" || "$PARENT_PATH" == "/configure/"* ]]; then
    get_msg "script_cmd_restricted" "Error: The /configure tree is restricted and cannot be modified."
    exit 1
fi

NEW_CMD=$(jq -n \
  --arg name "$NAME" \
  --arg short_desc "$SHORT_DESC" \
  --arg activation "$ACTIVATION" \
  --arg syntax "$SYNTAX" \
  '{
    name: $name,
    short_desc: $short_desc
  } + (if $activation != "" then {activation: $activation} else {} end)
    + (if $syntax != "" then {syntax: $syntax} else {} end)')

TMP_FILE=$(mktemp)

jq --arg path "$PARENT_PATH" --argjson new_cmd "$NEW_CMD" '
  ($path | sub("^/"; "") | split("/")) as $parts |
  def insert_cmd($parts_left; $node):
    if ($parts_left | length) == 0 or ($parts_left | length == 1 and $parts_left[0] == "") then
      if ($node | type) == "array" then
        $node + [$new_cmd]
      else
        $node | .subcommands = ((.subcommands // []) + [$new_cmd])
      end
    else
      if ($node | type) == "array" then
        map(if .name == $parts_left[0] then insert_cmd($parts_left[1:]; .) else . end)
      else
        .subcommands = (.subcommands | map(if .name == $parts_left[0] then insert_cmd($parts_left[1:]; .) else . end))
      end
    end;
  if $path == "/" or $path == "" then
    .commands = (.commands + [$new_cmd])
  else
    .commands = (.commands | insert_cmd($parts; .))
  end
' "$CONFIG_FILE" > "$TMP_FILE"

if [ $? -eq 0 ]; then
    cat "$TMP_FILE" > "$CONFIG_FILE"
    get_msg "script_cmd_added" "Command added successfully. Run 'reload conf' to apply."
else
    get_msg "script_cmd_error" "Error: Failed to modify config file."
fi
rm -f "$TMP_FILE"
