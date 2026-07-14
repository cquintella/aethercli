/*
 * Copyright (c) 2026 caq@intelliurb.com
 */
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <optional>

namespace cli::macro {

class MacroEngine {
public:
    // std::nullopt = arquivo não pôde ser aberto (distinto de macro vazia).
    static std::optional<std::vector<std::string>> loadMacro(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) return std::nullopt;

        std::vector<std::string> commands;
        std::string line;
        while (std::getline(file, line)) {
            // Ignora linhas vazias ou comentários
            if (line.empty() || line[0] == '#') continue;
            commands.push_back(line);
        }
        return commands;
    }
};

} // namespace cli::macro
