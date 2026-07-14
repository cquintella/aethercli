# aethercli - Instructions Manual (RTFM)

**aethercli** is an advanced command-line interface, inspired by Cisco IOS, designed for task automation and server management with built-in support for AI suggestions and macros.

---

## 1. Installation and Compilation

### Prerequisites
- C++17 Compiler (GCC 9+, Clang, or MSVC)
- CMake 3.14+
- Internet connection (to automatically download dependencies)

### Compiling
```bash
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make -j$(nproc)
sudo make install
```

### Running the tests
Unit tests (Catch2) build by default:
```bash
cd build && ctest --output-on-failure
```
Disable with `-DAETHERCLI_BUILD_TESTS=OFF`.

---

## 2. General Usage

### Interactive Mode
```bash
aethercli [-C config_file] [-l language_code_or_file] [--log [path]]
```

### Headless Mode (Automation)
```bash
aethercli -p "show version"
```

---

## 3. Interface and Navigation

### Navigation Commands (Cisco Style)
aethercli uses an immediate interaction system:
- **TAB (1x):** Completes the current token up to the longest common prefix of the matches; a unique match is completed in full (plus a trailing space). No possible completion rings the terminal bell.
- **TAB (2x):** Instantly shows available options for the current level.
- **Question Mark (?):** Instantly shows available options without needing to press ENTER. Inside an open double quote, `?` is inserted literally.
- **Abbreviations:** Any unambiguous prefix works as the full command (`sh ip ro` = `show ip route`). Ambiguous abbreviations are reported (`% Ambiguous command`).
- **ENTER:** Executes the command. Non-zero exit codes and signals of the underlying command are reported.
- **exit / quit / Ctrl+D (empty line):** Exits configuration mode or terminates the program. Terminal settings are restored on exit, including on SIGINT/SIGTERM/SIGHUP.
- **Ctrl+C:** In the prompt, clears the current line. While a command is running, interrupts only that command — the shell keeps running.

### Line Editing
- Arrows Left/Right, Home/End, Delete, Backspace (UTF-8 aware — accented characters are handled as one column).
- **Ctrl+A / Ctrl+E:** Start / end of line.
- **Ctrl+U / Ctrl+K:** Delete to start / end of line.
- **Ctrl+W:** Delete the word before the cursor.
- **Ctrl+L:** Clear the screen.
- Arrows Up/Down: command history. History persists across sessions in `~/.aethercli/history` (mode 0600, last 500 entries).

---

## 4. Internal Reserved Commands and Macros

### 4.1 Internal Reserved Commands
AetherCLI reserves specific `activation` strings for internal engine features:
- `internal:config_mode`: Switches context to configuration mode (`aethercli(config)#`).
- `internal:run_macro`: Executes a batch of commands from a `.macro` file.
- `internal:list_macros`: Lists available macro files in the configuration directory.
- `internal:ai`: Sends the remaining tokens as a natural-language question to the local AI (see section 7).

### 4.2 Macro System
Macros are simple text files with the `.macro` extension. Each line is executed as a standard CLI command.
- Empty lines and lines starting with `#` are ignored.
- Macros are searched in the same directory as your main JSON configuration file (absolute paths are also accepted).
- Macros may call other macros; recursion is capped at depth 8.
- **Example usage:**
  `run macro setup.macro`
  `list macros`

---

## 5. Command Configuration (`conf/commands.json`)

The configuration is **hierarchical**. This allows for organized subcommands, similar to a professional router.

### Command Object Structure
- `name`: Command name (used in the terminal). **Required.**
- `short_desc`: Short description displayed during Double-TAB.
- `description`: Longer description (used in the AI system prompt).
- `syntax`: Help text displayed for command details.
- `activation`: Operating system command or internal keyword to be executed.
- `args`: `"none"` (default) or `"append"`. With `"append"`, extra user tokens are appended to the `activation` command, each one shell-escaped. With `"none"`, extra tokens are rejected. See section 6.
- `subcommands`: (Optional) A list of other nested commands.

### Configuration Example
```json
{
  "motd": "Welcome message here\n",
  "commands": [
    {
      "name": "show",
      "short_desc": "Visualization commands",
      "subcommands": [
        {
          "name": "disk",
          "short_desc": "Disk usage",
          "activation": "df -h",
          "syntax": "show disk [path]",
          "args": "append"
        }
      ]
    }
  ]
}
```

---

## 6. Security and Auditing

### Execution Model
- aethercli only executes what is mapped in the `activation` field. Shell metacharacters in user input (e.g., `; rm -rf /`) are neutralized: extra tokens are passed as quoted, escaped arguments.
- By default a command accepts **no arguments** (`"args": "none"`). Opt in per command with `"args": "append"`.
- **Flag Injection Prevention:** If a command accepts arguments, the user cannot pass arguments starting with `-` (e.g., `-exec`) to prevent flag injection, **unless** the `activation` string explicitly ends with the end-of-options demarcator ` --` (e.g., `ls --`).
- **Input Hard Limit:** All inputs (interactive, headless, or macros) are strictly limited to a maximum of 4096 characters to prevent buffer exhaustion.
- The `-C` flag lets the invoking user load their own configuration. aethercli is **not** a privilege boundary: whoever can write the config file controls what runs. In restricted-shell deployments, invoke aethercli through a wrapper with a fixed `-C` and a root-owned config.

### Execution Logs
By default, **logging is disabled** to protect sensitive data.
- To enable: use the `--log` flag.
- Default location: `~/.aethercli/log/aethercli.log`.
- **Warning:** In production, avoid keeping logs enabled if commands handle passwords or private data. Note that the interactive history (`~/.aethercli/history`) also records typed commands.

### Directory Protection
- `make install` installs the configuration with explicit permissions: files 0644, directories 0755, owned by the installing user (typically root). Only the owner can edit `commands.json`.

---

## 7. AI Integration (llama.cpp / Ollama)

aethercli consults a local AI to suggest commands based on natural language. It speaks the OpenAI-compatible API (`/v1/chat/completions`), which is served both by **llama.cpp** (`llama-server`) and by **Ollama** — no configuration flag needed to pick between them.

- **Usage:** `ai how do I check the routing table`
- **Workflow:** the AI receives the list of executable commands and must answer with a single command line. Code fences, backticks, and `<think>` blocks in the reply are stripped. If the suggestion resolves to a real command, aethercli asks for confirmation (`Execute '...'? (y/n)`) before running it. In headless mode suggestions are printed but never executed.
- **Starting a backend with llama.cpp:**
  ```bash
  llama-server -m /opt/llm/Qwen3-VL-2B-Instruct-Q4_K_M.gguf --port 8080
  ```
- **Configuration (environment variables):**
  - `AI_HOST` (default `localhost`)
  - `AI_PORT` (default: probes `11434` (Ollama) then `8080` (llama-server))
  - `AI_MODEL` (default `default`; llama-server ignores it and uses the loaded model, Ollama requires a valid model name, e.g. `llama3`)
  - `AI_API_KEY` (optional; sent as `Authorization: Bearer`)
- **System prompt:** override via the `ai_system_prompt` key in the language file (e.g., `lang_en.json`).
- Tip: `?` triggers inline help at the prompt, so phrase AI questions without it (or quote it).

---

## 8. Authentication and User Management

aethercli supports an optional local authentication system.

### Enabling Authentication
In your JSON configuration file, set:
```json
{
  "require_authentication": true,
  "passwd-file": "/etc/aethercli/users.txt",
  "number_auth_fail": 3
}
```
If `passwd-file` is omitted, the system defaults to `users.json` in the configuration directory.

### User Database Format
aethercli supports standard Unix-style flat text files for user authentication. The format is `username:name:email:sha256_hash` (one user per line). Usernames must adhere to POSIX standards (1-32 characters). Passwords must be between 8 and 255 characters.

### Adding Users
To add or update a user securely, use the provided `adduser.sh` script or the CLI built-in:
```bash
./adduser.sh /etc/aethercli/users.txt admin "Admin" admin@local
# Or via CLI
aethercli --adduser admin
```
The hashing mechanism supports the `AETHERCLI_PEPPER` environment variable to salt/pepper the hashes dynamically.

---
*aethercli v0.4.0 - Security, Hierarchy, Macros, Authentication, and AI Implemented*
