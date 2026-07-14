/*
 * Copyright (c) 2026 caq@intelliurb.com
 * PROPRIEDADE DA INTELLIURB - SOMENTE USO INTERNO
 */
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace cli::parser {

struct Command {
    std::string name;
    std::string abbrev;
    std::string short_desc;
    std::string description;
    std::string activation;
    std::string syntax;
    // Se true ("args": "append"), tokens extras do usuário são anexados ao
    // comando shell (escapados). Default false: comando não aceita argumentos.
    bool allow_args = false;
    std::vector<Command> subcommands;
};

struct Config {
    std::vector<Command> commands;
    std::string motd;
    // Autenticação opcional: se true, exige login contra <configDir>/users.json.
    bool require_authentication = false;
    int number_auth_fail = 3;
    std::string passwd_file;
};

class CommandParser {
public:
    static Config parseConfig(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Critical Error: Configuration file not found: " + filePath);
        }

        nlohmann::json j;
        try {
            file >> j;
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error("Critical Error: Malformed JSON in " + filePath + " at byte " + std::to_string(e.byte) + ": " + e.what());
        }

        Config config;
        
        if (j.contains("motd") && j["motd"].is_string()) {
            config.motd = j["motd"];
        }

        if (j.contains("require_authentication")) {
            const auto& v = j["require_authentication"];
            if (v.is_boolean()) config.require_authentication = v.get<bool>();
            else if (v.is_string() && (v == "true" || v == "false")) config.require_authentication = (v == "true");
            else throw std::runtime_error("Critical Error: 'require_authentication' must be \"true\" or \"false\" in " + filePath);
        }

        if (j.contains("number_auth_fail")) {
            const auto& v = j["number_auth_fail"];
            int n = 0;
            if (v.is_number_integer()) n = v.get<int>();
            else if (v.is_string()) {
                try { n = std::stoi(v.get<std::string>()); } catch (...) { n = 0; }
            }
            if (n < 1) throw std::runtime_error("Critical Error: 'number_auth_fail' must be an integer >= 1 in " + filePath);
            config.number_auth_fail = n;
        }

        if (j.contains("passwd-file")) {
            const auto& v = j["passwd-file"];
            if (!v.is_string()) throw std::runtime_error("Critical Error: 'passwd-file' must be a string in " + filePath);
            config.passwd_file = v.get<std::string>();
        }

        if (!j.contains("commands")) {
            throw std::runtime_error("Critical Error: Missing mandatory top-level field 'commands' in " + filePath);
        }

        if (!j["commands"].is_array()) {
            throw std::runtime_error("Critical Error: Field 'commands' must be an array in " + filePath);
        }

        config.commands = parseCommands(j["commands"], "root");
        return config;
    }

private:
    static std::vector<Command> parseCommands(const nlohmann::json& j, const std::string& context) {
        std::vector<Command> commands;
        
        for (size_t i = 0; i < j.size(); ++i) {
            const auto& element = j[i];
            std::string current_context = context + ".commands[" + std::to_string(i) + "]";

            if (!element.is_object()) {
                throw std::runtime_error("Critical Error: Array element at " + current_context + " is not an object.");
            }

            // Validar campos obrigatórios
            if (!element.contains("name") || !element["name"].is_string() || element["name"].get<std::string>().empty()) {
                throw std::runtime_error("Critical Error: Missing or invalid mandatory field 'name' at " + current_context);
            }

            Command cmd;
            cmd.name = element["name"];
            
            // Campos opcionais com validação de tipo
            if (element.contains("abbrev")) {
                if (!element["abbrev"].is_string()) throw std::runtime_error("Error: 'abbrev' must be a string at " + current_context);
                cmd.abbrev = element["abbrev"];
            }

            if (element.contains("short_desc")) {
                if (!element["short_desc"].is_string()) throw std::runtime_error("Error: 'short_desc' must be a string at " + current_context);
                cmd.short_desc = element["short_desc"];
            }

            if (element.contains("description")) {
                if (!element["description"].is_string()) throw std::runtime_error("Error: 'description' must be a string at " + current_context);
                cmd.description = element["description"];
            }

            if (element.contains("activation")) {
                if (!element["activation"].is_string()) throw std::runtime_error("Error: 'activation' must be a string at " + current_context);
                cmd.activation = element["activation"];
            }

            if (element.contains("syntax")) {
                if (!element["syntax"].is_string()) throw std::runtime_error("Error: 'syntax' must be a string at " + current_context);
                cmd.syntax = element["syntax"];
            }

            if (element.contains("args")) {
                if (!element["args"].is_string()) throw std::runtime_error("Error: 'args' must be a string at " + current_context);
                std::string args_policy = element["args"];
                if (args_policy == "append") cmd.allow_args = true;
                else if (args_policy == "none") cmd.allow_args = false;
                else throw std::runtime_error("Error: 'args' must be \"none\" or \"append\" at " + current_context);
            }

            if (element.contains("subcommands")) {
                if (!element["subcommands"].is_array()) {
                    throw std::runtime_error("Critical Error: 'subcommands' must be an array at " + current_context);
                }
                cmd.subcommands = parseCommands(element["subcommands"], current_context + "(" + cmd.name + ")");
            }

            commands.push_back(cmd);
        }
        return commands;
    }
};

} // namespace cli::parser
