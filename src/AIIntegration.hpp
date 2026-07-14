/*
 * Copyright (c) 2026 caq@intelliurb.com
 * PROPRIEDADE DA INTELLIURB - SOMENTE USO INTERNO
 * A cópia e o uso devem ser expressamente autorizados.
 */
#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include "CliUtils.hpp"
#include "CommandParser.hpp"

namespace cli::ai {

inline std::string getEnvOrDefault(const std::string& var, const std::string& default_val) {
    const char* value = std::getenv(var.c_str());
    return value ? std::string(value) : default_val;
}

inline int getEnvPortOrDefault(const std::string& var, int default_val) {
    const char* value = std::getenv(var.c_str());
    if (!value) return default_val;
    try {
        return std::stoi(value);
    } catch (...) {
        return default_val;
    }
}

// Fala a API OpenAI-compatible (/v1/chat/completions), servida tanto pelo
// llama.cpp (llama-server) quanto pelo Ollama. Sem AI_PORT definida, sonda
// primeiro a porta padrão do Ollama (11434) e depois a do llama-server (8080).
class AIIntegration {
public:
    AIIntegration() : host(getEnvOrDefault("AI_HOST", "localhost")),
                      port(getEnvPortOrDefault("AI_PORT", 0)), // 0 = auto-probe
                      model(getEnvOrDefault("AI_MODEL", "default")),
                      api_key(getEnvOrDefault("AI_API_KEY", "")) {}

    AIIntegration(const std::string& h, int p, const std::string& m, const std::string& key = "")
        : host(h), port(p), model(m), api_key(key) {}

    std::string ask(const std::string& prompt, const std::vector<cli::parser::Command>& availableCommands, const std::string& systemInstruction) {
        std::string commandList;
        appendCommandList(availableCommands, "", commandList);

        nlohmann::json body = {
            {"model", model},
            {"messages", {
                {{"role", "system"}, {"content", systemInstruction + "\nComandos disponíveis:\n" + commandList}},
                {{"role", "user"}, {"content", prompt}}
            }},
            {"temperature", 0.2},
            {"stream", false}
        };

        std::vector<int> ports = (port > 0) ? std::vector<int>{port}
                                            : std::vector<int>{11434, 8080};
        std::string lastError;
        for (int p : ports) {
            try {
                httplib::Client cli(host, p);
                cli.set_connection_timeout(5, 0);
                cli.set_read_timeout(60, 0); // modelos locais pequenos podem demorar
                if (!api_key.empty()) {
                    cli.set_default_headers({{"Authorization", "Bearer " + api_key}});
                }

                auto res = cli.Post("/v1/chat/completions", body.dump(), "application/json");
                if (!res) {
                    lastError = "% Erro: Serviço AI não disponível em " + host + ":" + std::to_string(p);
                    continue;
                }
                if (res->status != 200) {
                    lastError = "% Erro: Serviço AI em " + host + ":" + std::to_string(p) +
                                " retornou status " + std::to_string(res->status);
                    continue;
                }

                try {
                    auto j = nlohmann::json::parse(res->body);
                    if (!j.contains("choices") || !j["choices"].is_array() || j["choices"].empty() ||
                        !j["choices"][0].contains("message") ||
                        !j["choices"][0]["message"].contains("content") ||
                        !j["choices"][0]["message"]["content"].is_string()) {
                        return "% Erro: Resposta do AI em formato inválido";
                    }
                    return cli::util::stripReasoning(j["choices"][0]["message"]["content"].get<std::string>());
                } catch (const nlohmann::json::exception& e) {
                    return "% Erro ao fazer parse da resposta do AI: " + std::string(e.what());
                }
            } catch (const std::exception& e) {
                lastError = "% Erro ao conectar ao serviço AI: " + std::string(e.what());
            }
        }
        if (ports.size() > 1) {
            return "% Erro: Nenhum serviço AI em " + host + " (portas testadas: 11434, 8080). "
                   "Defina AI_HOST/AI_PORT ou inicie o llama-server/Ollama.";
        }
        return lastError;
    }

private:
    // Lista recursivamente os comandos executáveis (folhas com activation),
    // com o caminho completo, para o system prompt.
    static void appendCommandList(const std::vector<cli::parser::Command>& cmds, const std::string& prefix, std::string& out) {
        for (const auto& c : cmds) {
            std::string path = prefix.empty() ? c.name : prefix + " " + c.name;
            if (!c.activation.empty() && c.activation.rfind("internal:", 0) != 0) {
                out += "- " + path;
                const std::string& desc = !c.description.empty() ? c.description : c.short_desc;
                if (!desc.empty()) out += ": " + desc;
                out += "\n";
            }
            appendCommandList(c.subcommands, path, out);
        }
    }

    std::string host;
    int port;
    std::string model;
    std::string api_key;
};

} // namespace cli::ai
