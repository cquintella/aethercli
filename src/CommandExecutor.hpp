/*
 * Copyright (c) 2026 caq@intelliurb.com
 * PROPRIEDADE DA INTELLIURB - SOMENTE USO INTERNO
 * A cópia e o uso devem ser expressamente autorizados.
 */
#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

namespace cli::executor {

struct ExecResult {
    int exit_code = 0;      // válido quando !signaled (127 = execvp falhou)
    bool signaled = false;
    int term_signal = 0;
};

class CommandExecutor {
public:
    static ExecResult execute(const std::vector<std::string>& args) {
        if (args.empty()) {
            throw std::runtime_error("Empty command arguments!");
        }

        // Ctrl+C deve interromper apenas o comando filho, não o shell: ignora
        // SIGINT/SIGQUIT no pai antes do fork (herdado como ignore pelo filho,
        // que restaura o default antes do exec).
        struct sigaction ign {}, old_int, old_quit;
        ign.sa_handler = SIG_IGN;
        sigaction(SIGINT, &ign, &old_int);
        sigaction(SIGQUIT, &ign, &old_quit);

        pid_t pid = fork();
        if (pid == -1) {
            sigaction(SIGINT, &old_int, nullptr);
            sigaction(SIGQUIT, &old_quit, nullptr);
            throw std::runtime_error("fork() failed!");
        }

        if (pid == 0) {
            std::signal(SIGINT, SIG_DFL);
            std::signal(SIGQUIT, SIG_DFL);
            // No filho, apenas executa. A saída vai para o stdout/stderr do pai.
            std::vector<char*> argv;
            for (const auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            _exit(127);
        }

        int status = 0;
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
        sigaction(SIGINT, &old_int, nullptr);
        sigaction(SIGQUIT, &old_quit, nullptr);

        ExecResult result;
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.signaled = true;
            result.term_signal = WTERMSIG(status);
        }
        return result;
    }

    static ExecResult executeShellCommand(const std::string& cmd) {
        return execute({"/bin/sh", "-c", cmd});
    }
};

} // namespace cli::executor
