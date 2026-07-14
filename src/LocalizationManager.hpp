/*
 * Copyright (c) 2026 caq@intelliurb.com
 */
#pragma once
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace cli::i18n {

class LocalizationManager {
public:
    void loadLanguage(const std::string& langSource, const std::string& configDir) {
        if (langSource.empty()) return;

        std::string path;
        if (langSource[0] == '/') {
            path = langSource;
        } else if (langSource.size() > 5 && langSource.substr(langSource.size() - 5) == ".json") {
            path = configDir + "/" + langSource;
        } else {
            path = configDir + "/lang_" + langSource + ".json";
        }

        std::ifstream file(path);
        if (!file.is_open()) return;

        // Arquivo de idioma malformado não pode derrubar o shell.
        try {
            nlohmann::json j;
            file >> j;
            translations = j.get<std::map<std::string, std::string>>();
        } catch (const std::exception& e) {
            std::cerr << "Warning: ignoring malformed language file " << path
                      << " (" << e.what() << ")" << std::endl;
        }
    }

    std::string get(const std::string& key) const {
        auto it = translations.find(key);
        if (it != translations.end()) return it->second;
        return key; // Retorna a própria chave se não encontrar tradução
    }

    std::string getOr(const std::string& key, const std::string& fallback) const {
        auto it = translations.find(key);
        return it != translations.end() ? it->second : fallback;
    }

private:
    std::map<std::string, std::string> translations;
};

} // namespace cli::i18n
