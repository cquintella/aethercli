/*
 * Copyright (c) 2026 caq@intelliurb.com
 * PROPRIEDADE DA INTELLIURB - SOMENTE USO INTERNO
 */
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <cerrno>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <csignal>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "CliUtils.hpp"
#include "Completer.hpp"
#include "CommandParser.hpp"
#include "CommandExecutor.hpp"
#include "AIIntegration.hpp"
#include "MacroEngine.hpp"
#include "Auth.hpp"
#include "Logger.hpp"
#include "LocalizationManager.hpp"
#include "Common.hpp"

namespace fs = std::filesystem;
using namespace cli::util;
using namespace cli::parser;
using namespace cli::executor;
using namespace cli::ai;
using namespace cli::macro;
using namespace cli::auth;
using namespace cli::log;
using namespace cli::i18n;

enum class Context { GLOBAL, CONFIG };

struct AppState {
    Context context = Context::GLOBAL;
    std::chrono::steady_clock::time_point last_tab_time;
};

constexpr int MAX_MACRO_DEPTH = 8;

// Global pointer for signal handler to access RawMode
struct termios* global_orig_termios = nullptr;

// Usuário autenticado (ou "-" quando require_authentication está desligado);
// aparece nas linhas de log "user info: <user>: <comando>".
std::string g_current_user = "-";

// Somente chamadas async-signal-safe aqui (tcsetattr e _exit são; std::exit não).
void restore_terminal(int sig) {
    if (global_orig_termios) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, global_orig_termios);
    }
    _exit(128 + sig);
}

// Helper to temporarily restore normal terminal mode
class TerminalRestorer {
    struct termios saved_mode;
    bool saved = false;
public:
    TerminalRestorer(struct termios* orig) {
        if (!isatty(STDIN_FILENO) || !orig) return;
        if (tcgetattr(STDIN_FILENO, &saved_mode) == -1) return;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
        saved = true;
    }
    ~TerminalRestorer() {
        if (saved) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_mode);
        }
    }
};

// Helper for raw terminal mode
class RawMode {
    struct termios orig_termios;
    bool active = false;
public:
    RawMode() {
        if (!isatty(STDIN_FILENO)) return;
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
        active = true;
        global_orig_termios = &orig_termios;
        std::signal(SIGINT, restore_terminal);
        std::signal(SIGTERM, restore_terminal);
        std::signal(SIGHUP, restore_terminal);

        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        raw.c_cc[VMIN] = 0; // Non-blocking read
        raw.c_cc[VTIME] = 1; // 100ms timeout
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    ~RawMode() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
            global_orig_termios = nullptr;
        }
    }
};

bool executeCommand(const std::string& full_input, const std::vector<Command>& tree,
                    LocalizationManager& lang, Context& ctx, const std::string& configDir,
                    int macro_depth = 0) {
    if (full_input.length() > 4096) {
        std::cout << "%% Error: Command line exceeds maximum allowed length (4096 characters)." << std::endl;
        return true;
    }

    auto tokens = split(full_input);
    if (tokens.empty()) return true;

    if (tokens[0] == "exit" || tokens[0] == "quit") {
        if (ctx == Context::CONFIG) {
            ctx = Context::GLOBAL;
            std::cout << "\nReturning to GLOBAL mode." << std::endl;
            return true;
        } else {
            std::cout << "\nExiting..." << std::endl;
            return false;
        }
    }

    auto res = cli::completer::resolve(tree, tokens);
    if (res.ambiguous) {
        std::cout << "\n% Ambiguous command: \"" << res.ambiguous_token << "\"" << std::endl;
        return true;
    }
    if (!res.cmd) {
        std::cout << "\n" << lang.get("unknown_cmd") << full_input << std::endl;
        return true;
    }

    const Command& cmd = *res.cmd;
    const size_t depth = res.depth;

    if (cmd.activation.empty()) {
        if (!cmd.subcommands.empty()) {
            if (depth == tokens.size()) {
                std::cout << "\n% Incomplete command." << std::endl;
                std::cout << "\nAvailable subcommands:" << std::endl;
                for (const auto& c : cmd.subcommands) {
                    std::cout << "  " << std::left << std::setw(15) << c.name << " - " << c.short_desc << std::endl;
                }
            } else {
                std::cout << "\n" << lang.get("unknown_cmd") << full_input << std::endl;
            }
        } else {
            std::cout << "\n% Command '" << cmd.name << "' has no action configured." << std::endl;
        }
        return true;
    }

    std::cout << "\n" << lang.get("executing") << full_input << std::endl;

    if (cmd.activation == "internal:config_mode") {
        ctx = Context::CONFIG;
        std::cout << lang.get("config_mode_msg") << std::endl;
        return true;
    }

    if (cmd.activation == "internal:run_macro") {
        if (tokens.size() <= depth) {
            std::cout << "\n%% Error: Missing macro filename." << std::endl;
            return true;
        }
        if (macro_depth >= MAX_MACRO_DEPTH) {
            std::cout << "\n%% Error: Macro recursion limit (" << MAX_MACRO_DEPTH << ") reached." << std::endl;
            return true;
        }
        std::string macroFile = tokens[depth];
        if (macroFile[0] != '/') macroFile = configDir + "/" + macroFile;

        std::cout << "\n% Loading macro: " << macroFile << std::endl;
        auto macroCommands = MacroEngine::loadMacro(macroFile);
        if (!macroCommands) {
            std::cout << "%% Error: Cannot open macro file: " << macroFile << std::endl;
            return true;
        }
        for (const auto& mCmd : *macroCommands) {
            executeCommand(mCmd, tree, lang, ctx, configDir, macro_depth + 1);
        }
        return true;
    }

    if (cmd.activation == "internal:list_macros") {
        std::cout << "\nAvailable macros in " << configDir << ":" << std::endl;
        bool found = false;
        try {
            for (const auto& entry : fs::directory_iterator(configDir)) {
                if (entry.path().extension() == ".macro") {
                    std::cout << "  " << entry.path().filename().string() << std::endl;
                    found = true;
                }
            }
        } catch (const fs::filesystem_error&) {
            std::cout << "%% Error: Cannot read directory: " << configDir << std::endl;
            return true;
        }
        if (!found) std::cout << "  (No macro files found)" << std::endl;
        return true;
    }

    if (cmd.activation == "internal:ai") {
        std::string question;
        for (size_t i = depth; i < tokens.size(); ++i) {
            if (!question.empty()) question += " ";
            question += tokens[i];
        }
        if (question.empty()) {
            std::cout << "%% Error: Usage: " << (cmd.syntax.empty() ? "ai <question>" : cmd.syntax) << std::endl;
            return true;
        }

        static const std::string default_ai_prompt =
            "You are an assistant embedded in aethercli, a Cisco IOS-style management shell. "
            "Reply with EXACTLY ONE command line chosen from the available commands below that "
            "best answers the user's request, and nothing else. If no command applies, reply "
            "with one short sentence starting with '%'.";

        AIIntegration ai;
        std::cout << "% Consulting AI..." << std::endl;
        std::string answer = ai.ask(question, tree, lang.getOr("ai_system_prompt", default_ai_prompt));
        std::cout << answer << std::endl;

        // Primeira linha útil da resposta (sem cercas de código/crases) é a
        // candidata a comando.
        std::string suggestion = extractCommandSuggestion(answer);
        if (suggestion.empty() || suggestion[0] == '%') return true;

        auto sres = cli::completer::resolve(tree, split(suggestion));
        bool executable = sres.cmd && !sres.cmd->activation.empty() &&
                          sres.cmd->activation.rfind("internal:", 0) != 0;
        if (!executable) return true;

        if (global_orig_termios) { // loop interativo: pedir confirmação
            TerminalRestorer restorer(global_orig_termios);
            std::cout << "Execute '" << suggestion << "'? (y/n) " << std::flush;
            std::string reply;
            if (std::getline(std::cin, reply) &&
                (reply == "y" || reply == "Y" || reply == "yes" || reply == "s" || reply == "sim")) {
                return executeCommand(suggestion, tree, lang, ctx, configDir, macro_depth);
            }
        } else {
            std::cout << "% (suggestion not executed: no interactive confirmation available)" << std::endl;
        }
        return true;
    }

    if (cmd.activation.rfind("internal:", 0) == 0) {
        std::cout << "%% Error: Unrecognized internal activation: " << cmd.activation << std::endl;
        return true;
    }

    // Comando shell. Tokens extras viram argumentos apenas se o comando
    // declarar "args": "append" — evita injeção de flags em comandos mapeados.
    std::vector<std::string> extra(tokens.begin() + depth, tokens.end());
    if (!extra.empty() && !cmd.allow_args) {
        std::cout << "% Command '" << cmd.name << "' does not accept arguments"
                  << " (set \"args\": \"append\" in the config to allow)." << std::endl;
        return true;
    }

    // Demarcador de Fim de Opções
    bool has_eoo = (cmd.activation.length() >= 3 && cmd.activation.substr(cmd.activation.length() - 3) == " --");
    if (!has_eoo) {
        for (const auto& a : extra) {
            if (!a.empty() && a[0] == '-') {
                std::cout << "%% Error: Security violation. Extra arguments starting with '-' are forbidden unless the command activation ends with ' --'." << std::endl;
                return true;
            }
        }
    }

    Logger::info(g_current_user + ": " + full_input);
    try {
        std::string final_cmd = cmd.activation;
        for (const auto& a : extra) {
            final_cmd += " " + escape_shell_arg(a);
        }
        ExecResult r;
        {
            TerminalRestorer restorer(global_orig_termios);
            r = CommandExecutor::executeShellCommand(final_cmd);
        }
        if (r.signaled) {
            std::cout << "% Command terminated by signal " << r.term_signal << "." << std::endl;
        } else if (r.exit_code == 127) {
            std::cout << "% Command not found or could not be executed (127)." << std::endl;
        } else if (r.exit_code != 0) {
            std::cout << "% Command exited with status " << r.exit_code << "." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        Logger::error(e.what());
    }
    return true;
}

void showOptions(const std::vector<Command>& tree, const std::string& current_input) {
    auto tokens = split(current_input);
    bool endsWithSpace = !current_input.empty() && current_input.back() == ' ';
    std::string last_token = (tokens.empty() || endsWithSpace) ? "" : tokens.back();
    if (!last_token.empty()) tokens.pop_back();

    const Command* parent = nullptr;
    const std::vector<Command>* current_level = &tree;
    if (!tokens.empty()) {
        auto res = cli::completer::resolve(tree, tokens);
        if (res.ambiguous) {
            std::cout << "\n% Ambiguous command: \"" << res.ambiguous_token << "\"" << std::endl;
            return;
        }
        if (!res.cmd || res.depth != tokens.size()) {
            std::cout << "\n% Unrecognized command." << std::endl;
            return;
        }
        parent = res.cmd;
        current_level = &parent->subcommands;
    }

    if (parent && (endsWithSpace || parent->subcommands.empty())) {
        std::cout << "\nDetails for '" << parent->name << "':" << std::endl;
        std::cout << "  Description: " << parent->short_desc << std::endl;
        if (!parent->syntax.empty()) {
            std::cout << "  Syntax:      " << parent->syntax << std::endl;
        }

        if (parent->subcommands.empty()) {
            if (parent->allow_args) {
                std::cout << "  (Accepts additional arguments)" << std::endl;
            }
            return;
        } else if (endsWithSpace) {
            std::cout << "\nSubcommands:" << std::endl;
            for (const auto& c : parent->subcommands) {
                std::cout << "  " << std::left << std::setw(15) << c.name << " - " << c.short_desc << std::endl;
            }
            return;
        }
    }

    auto matches = cli::completer::prefixMatches(*current_level, last_token);
    std::cout << "\nAvailable options:" << std::endl;
    if (matches.empty()) {
        if (parent && parent->subcommands.empty()) {
            std::cout << "  (No more options for this command)" << std::endl;
        } else {
            std::cout << "  (No matches found)" << std::endl;
        }
        return;
    }
    for (const auto* c : matches) {
        std::cout << "  " << std::left << std::setw(15) << c->name << " - " << c->short_desc << std::endl;
    }
}

void runInteractive(const std::vector<Command>& commands, const std::string& langSource, const std::string& configDir, const std::string& motd) {
    LocalizationManager lang;
    lang.loadLanguage(langSource, configDir);

    if (!motd.empty()) {
        std::cout << motd << std::endl;
    } else {
        std::cout << lang.get("welcome") << std::endl;
    }

    AppState state;
    std::string buffer;
    size_t cursor_pos = 0;
    const size_t MAX_CHARS = 320;
    int last_printed_rows = 0;

    // Histórico persistente em ~/.aethercli/history
    std::vector<std::string> history;
    int history_index = -1;
    fs::path histFile;
    const char* home = std::getenv("HOME");
    if (home) {
        try {
            fs::path dir = fs::path(home) / ".aethercli";
            fs::create_directories(dir);
            histFile = dir / "history";
            std::ifstream in(histFile);
            std::string hline;
            while (std::getline(in, hline)) {
                if (!hline.empty()) history.push_back(hline);
            }
            const size_t MAX_HISTORY = 500;
            if (history.size() > MAX_HISTORY) {
                history.erase(history.begin(), history.end() - (long)MAX_HISTORY);
            }
        } catch (...) {
            histFile.clear();
        }
    }
    auto appendHistory = [&](const std::string& line) {
        if (histFile.empty()) return;
        std::ofstream out(histFile, std::ios::app);
        if (out) out << line << "\n";
        ::chmod(histFile.c_str(), 0600); // comandos podem conter dados sensíveis
    };

    RawMode raw;

    auto redraw = [&](const std::string& prompt) {
        struct winsize w {};
        int cols = 80;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) cols = w.ws_col;

        if (last_printed_rows > 0) {
            std::cout << "\r\033[" << last_printed_rows << "A";
        } else {
            std::cout << "\r";
        }

        std::cout << "\033[J";

        std::string full_text = prompt + buffer;
        std::cout << full_text;

        // Largura em colunas (codepoints), não em bytes — senão acentos
        // desalinham o cursor.
        int total_width = (int)displayWidth(full_text);
        last_printed_rows = total_width / cols;

        int cur_width = (int)(displayWidth(prompt) + displayWidth(buffer, cursor_pos));
        int target_row = cur_width / cols;
        int target_col = cur_width % cols;

        int move_up = last_printed_rows - target_row;
        if (move_up > 0) {
            std::cout << "\033[" << move_up << "A";
        }

        std::cout << "\r";
        if (target_col > 0) {
            std::cout << "\033[" << target_col << "C";
        }

        std::cout << std::flush;
    };

    while (true) {
        std::string prompt = (state.context == Context::CONFIG) ? "aethercli(config)# " : "aethercli# ";
        redraw(prompt);

        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            if (n == 0 && isatty(STDIN_FILENO)) { // timeout do VTIME
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            break; // EOF (stdin redirecionado) ou erro de leitura
        }

        if (c == 3) { // Ctrl+C
            buffer.clear();
            cursor_pos = 0;
            history_index = -1;
            last_printed_rows = 0;
            std::cout << "^C" << std::endl;
            continue;
        }

        if (c == '\n' || c == '\r') {
            std::cout << std::endl;
            last_printed_rows = 0;
            if (!buffer.empty()) {
                if (history.empty() || history.back() != buffer) {
                    history.push_back(buffer);
                    appendHistory(buffer);
                }
                history_index = -1;
                bool keep_running = true;
                try {
                    keep_running = executeCommand(buffer, commands, lang, state.context, configDir);
                } catch (const std::exception& e) {
                    std::cout << "%% Internal error: " << e.what() << std::endl;
                }
                buffer.clear();
                cursor_pos = 0;
                if (!keep_running) break;
            }
        } else if (c == 4) { // Ctrl+D
            if (buffer.empty()) {
                std::cout << std::endl;
                break;
            }
            if (cursor_pos < buffer.size()) {
                buffer.erase(cursor_pos, utf8Next(buffer, cursor_pos) - cursor_pos);
            }
        } else if (c == 1) { // Ctrl+A
            cursor_pos = 0;
        } else if (c == 5) { // Ctrl+E
            cursor_pos = buffer.size();
        } else if (c == 11) { // Ctrl+K
            buffer.erase(cursor_pos);
        } else if (c == 21) { // Ctrl+U
            buffer.erase(0, cursor_pos);
            cursor_pos = 0;
        } else if (c == 23) { // Ctrl+W: apaga a palavra antes do cursor
            size_t p = cursor_pos;
            while (p > 0 && buffer[p - 1] == ' ') --p;
            while (p > 0 && buffer[p - 1] != ' ') --p;
            buffer.erase(p, cursor_pos - p);
            cursor_pos = p;
        } else if (c == 12) { // Ctrl+L
            std::cout << "\033[2J\033[H";
            last_printed_rows = 0;
        } else if (c == 127 || c == 8) { // Backspace
            if (cursor_pos > 0) {
                size_t p = utf8Prev(buffer, cursor_pos);
                buffer.erase(p, cursor_pos - p);
                cursor_pos = p;
            }
        } else if (c == 27) { // Escape sequence
            char s0;
            if (read(STDIN_FILENO, &s0, 1) <= 0) continue; // ESC solto
            if (s0 == '[') {
                // Consome a sequência CSI inteira até o byte final (0x40-0x7E),
                // senão teclas como Delete ([3~) vazam '~' para o buffer.
                std::string params;
                char fin = 0;
                char sc;
                while (read(STDIN_FILENO, &sc, 1) > 0) {
                    if ((unsigned char)sc >= 0x40 && (unsigned char)sc <= 0x7E) {
                        fin = sc;
                        break;
                    }
                    params += sc;
                    if (params.size() > 16) break; // sequência anômala
                }

                if (fin == 'A') { // Up
                    if (!history.empty()) {
                        if (history_index == -1) history_index = (int)history.size() - 1;
                        else if (history_index > 0) history_index--;
                        buffer = history[history_index];
                        if (buffer.length() > MAX_CHARS) buffer.resize(MAX_CHARS);
                        cursor_pos = buffer.length();
                    }
                } else if (fin == 'B') { // Down
                    if (history_index != -1) {
                        if (history_index < (int)history.size() - 1) {
                            history_index++;
                            buffer = history[history_index];
                            if (buffer.length() > MAX_CHARS) buffer.resize(MAX_CHARS);
                        } else {
                            history_index = -1;
                            buffer.clear();
                        }
                        cursor_pos = buffer.length();
                    }
                } else if (fin == 'C') { // Right
                    cursor_pos = utf8Next(buffer, cursor_pos);
                } else if (fin == 'D') { // Left
                    cursor_pos = utf8Prev(buffer, cursor_pos);
                } else if (fin == 'H') { // Home
                    cursor_pos = 0;
                } else if (fin == 'F') { // End
                    cursor_pos = buffer.size();
                } else if (fin == '~') {
                    if (params == "3") { // Delete
                        if (cursor_pos < buffer.size()) {
                            buffer.erase(cursor_pos, utf8Next(buffer, cursor_pos) - cursor_pos);
                        }
                    } else if (params == "1" || params == "7") { // Home
                        cursor_pos = 0;
                    } else if (params == "4" || params == "8") { // End
                        cursor_pos = buffer.size();
                    }
                }
                // Demais sequências: consumidas e ignoradas.
            } else if (s0 == 'O') { // SS3 (Home/End em alguns terminais)
                char s1;
                if (read(STDIN_FILENO, &s1, 1) > 0) {
                    if (s1 == 'H') cursor_pos = 0;
                    else if (s1 == 'F') cursor_pos = buffer.size();
                }
            }
            // ESC + outra tecla (Alt+x): ignorado.
        } else if (c == '\t') { // Tab
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_tab_time).count();
            state.last_tab_time = now;

            if (diff < 500) { // Double tab: lista as opções
                std::cout << std::endl;
                showOptions(commands, buffer);
                last_printed_rows = 0;
            } else {
                // Completa a partir do texto ANTES do cursor (não do buffer todo).
                std::string before = buffer.substr(0, cursor_pos);
                auto tokens = split(before);
                bool endsWithSpace = !before.empty() && before.back() == ' ';
                std::string last_token = (tokens.empty() || endsWithSpace) ? "" : tokens.back();
                if (!last_token.empty()) tokens.pop_back();

                const std::vector<Command>* level = &commands;
                bool valid_prefix = true;
                if (!tokens.empty()) {
                    auto res = cli::completer::resolve(commands, tokens);
                    if (res.cmd && res.depth == tokens.size()) {
                        level = &res.cmd->subcommands;
                    } else {
                        valid_prefix = false;
                    }
                }

                std::string completion;
                if (valid_prefix) {
                    auto matches = cli::completer::prefixMatches(*level, last_token);
                    if (!matches.empty()) {
                        // Completa até o maior prefixo comum; match único ganha
                        // espaço final.
                        std::string lcp = cli::completer::longestCommonPrefix(matches);
                        completion = lcp.substr(last_token.length());
                        if (matches.size() == 1) completion += " ";
                    }
                }

                if (!completion.empty() && buffer.length() + completion.length() <= MAX_CHARS) {
                    buffer.insert(cursor_pos, completion);
                    cursor_pos += completion.length();
                } else {
                    std::cout << '\a' << std::flush; // nada a completar: feedback sonoro
                }
            }
        } else if (c == '?' && std::count(buffer.begin(), buffer.end(), '\"') % 2 == 0) {
            // '?' dentro de aspas abertas é literal; fora delas, ajuda imediata.
            std::cout << std::endl;
            showOptions(commands, buffer);
            last_printed_rows = 0;
        } else if (isprint((unsigned char)c) || (c & 0x80)) {
            if (buffer.length() < MAX_CHARS) {
                buffer.insert(cursor_pos, 1, c);
                cursor_pos++;
            }
        }
    }
}

void setupLogger(bool enableLog, std::string customPath) {
    if (!enableLog) { Logger::setEnabled(false); return; }
    std::string finalPath = customPath;
    if (finalPath.empty()) {
        const char* home = std::getenv("HOME");
        if (!home) {
            Logger::setEnabled(false);
            return;
        }
        fs::path logDir = fs::path(home) / ".aethercli" / "log";
        try {
            fs::create_directories(logDir);
            finalPath = (logDir / "aethercli.log").string();
        } catch (...) {
            Logger::setEnabled(false);
            return;
        }
    }
    Logger::setLogPath(finalPath);
    Logger::setEnabled(true);
}

int main(int argc, char* argv[]) {
    std::string langSource = "en", configFile = AETHERCLI_CONFIG, target = "";
    bool headless = false, enableLog = false;
    std::string logPath = "", adduserName = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "aethercli v" << cli::VERSION << " - advanced management cli shell" << std::endl;
            std::cout << "Usage: aethercli [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -h, --help       Show this help message" << std::endl;
            std::cout << "  -v, --version    Show version information" << std::endl;
            std::cout << "  -C, --conf       Path to configuration file" << std::endl;
            std::cout << "  -l, --lang_file  Language code or .json file (default: en)" << std::endl;
            std::cout << "  -p               Headless mode: execute a single command" << std::endl;
            std::cout << "  --log [path]     Enable logging (optional path)" << std::endl;
            std::cout << "  --adduser <name> Add/update a user in users.json (require_authentication)" << std::endl;
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "aethercli v" << cli::VERSION << std::endl;
            return 0;
        } else if ((arg == "-C" || arg == "--conf") && i + 1 < argc) {
            configFile = argv[++i];
        } else if ((arg == "-l" || arg == "--lang_file") && i + 1 < argc) {
            langSource = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            headless = true;
            target = argv[++i];
        } else if (arg == "--adduser" && i + 1 < argc) {
            adduserName = argv[++i];
        } else if (arg == "--log") {
            enableLog = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') logPath = argv[++i];
        } else {
            std::cerr << "Warning: ignoring unknown option: " << arg << std::endl;
        }
    }

    setupLogger(enableLog, logPath);

    if (!adduserName.empty()) {
        try {
            std::string configDir = fs::absolute(fs::path(configFile)).parent_path().string();
            return Auth::addUser(configDir + "/users.json", adduserName) ? 0 : 1;
        } catch (const std::exception& e) {
            std::cerr << "Fatal: " << e.what() << std::endl;
            return 1;
        }
    }

    Config config;
    if (!fs::exists(configFile)) {
        // Config ausente não é fatal: avisa e abre o shell com árvore vazia,
        // para o usuário poder digitar comandos (exit/?) e ver o aviso.
        std::cerr << "% Warning: configuration file not found: " << configFile << std::endl;
        std::cerr << "% Starting with an empty command set. Use -C <file> to load a configuration." << std::endl;
    } else {
        try {
            config = CommandParser::parseConfig(configFile);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }

    try {
        std::string configDir = fs::absolute(fs::path(configFile)).parent_path().string();

        // Login antes de qualquer execução (interativa ou headless): fail closed.
        if (config.require_authentication) {
            std::string usersFile = config.passwd_file;
            if (usersFile.empty()) {
                usersFile = configDir + "/users.json";
            } else if (usersFile[0] != '/') {
                usersFile = configDir + "/" + usersFile;
            }
            AuthError err = AuthError::None;
            auto users = Auth::loadUsers(usersFile, err);
            if (err != AuthError::None) {
                Logger::error(std::string(errorEntry(err).tag) + ": " + usersFile);
                std::cerr << "Fatal: " << errorEntry(err).message << ": " << usersFile
                          << " (use --adduser <name> to create users)" << std::endl;
                return 1;
            }
            const std::string pepper = Auth::pepperFromEnv();
            int fails = 0;
            while (true) {
                std::cout << "Username: " << std::flush;
                std::string username;
                if (!Auth::readLineFd(username)) {
                    std::cout << std::endl;
                    return 1; // EOF no stdin: sem como autenticar
                }
                std::string password = Auth::readHiddenLine("Password: ");
                if (Auth::verify(users, username, password, pepper)) {
                    g_current_user = username;
                    Logger::info(g_current_user + ": logged in");
                    break;
                }
                ++fails;
                Logger::error(std::string(errorEntry(AuthError::BadCredentials).tag) +
                              ": user '" + username + "' (" + std::to_string(fails) + "/" +
                              std::to_string(config.number_auth_fail) + ")");
                std::cout << errorEntry(AuthError::BadCredentials).message << std::endl;
                if (fails >= config.number_auth_fail) {
                    Logger::error(errorEntry(AuthError::TooManyAttempts).tag);
                    std::cerr << errorEntry(AuthError::TooManyAttempts).message << std::endl;
                    return 1;
                }
            }
        }

        if (headless) {
            LocalizationManager lang;
            lang.loadLanguage(langSource, configDir);
            Context dummy = Context::GLOBAL;
            executeCommand(target, config.commands, lang, dummy, configDir);
        } else {
            runInteractive(config.commands, langSource, configDir, config.motd);
        }
    } catch (const std::exception& e) {
        // O unwinding até aqui já executou o destrutor do RawMode e restaurou
        // o terminal; sem este catch o std::terminate deixaria o tty em raw mode.
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
