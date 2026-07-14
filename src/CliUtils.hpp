/*
 * Copyright (c) 2026 caq@intelliurb.com
 * PROPRIEDADE DA INTELLIURB - SOMENTE USO INTERNO
 * A cópia e o uso devem ser expressamente autorizados.
 */
#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace cli::util {

// Divide a linha em tokens separados por espaço. Aspas duplas agrupam um
// token com espaços e são removidas do resultado.
inline std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_quotes = false;
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (c == '\"') {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

// Escapa um argumento para uso dentro de aspas duplas no /bin/sh.
// Dentro de "..." o sh só interpreta: " \ $ `
inline std::string escape_shell_arg(const std::string& arg) {
    std::string escaped = "\"";
    for (char c : arg) {
        if (c == '\"' || c == '\\' || c == '$' || c == '`') {
            escaped += '\\';
        }
        escaped += c;
    }
    escaped += "\"";
    return escaped;
}

// ---- Helpers para respostas de LLM ---------------------------------------

// Remove um bloco <think>...</think> do início da resposta (modelos Qwen em
// modo thinking) e espaços/linhas em branco nas bordas.
inline std::string stripReasoning(std::string s) {
    size_t start = s.find("<think>");
    if (start != std::string::npos) {
        size_t end = s.find("</think>", start);
        if (end != std::string::npos) {
            s.erase(start, end + 8 - start);
        }
    }
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Extrai a primeira linha útil de uma resposta de LLM como candidata a
// comando: pula linhas vazias e cercas de código (```), e remove crases,
// "$ " e "# " decorativos que modelos pequenos costumam adicionar.
inline std::string extractCommandSuggestion(const std::string& answer) {
    size_t pos = 0;
    while (pos < answer.size()) {
        size_t nl = answer.find('\n', pos);
        std::string line = answer.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? answer.size() : nl + 1;

        size_t b = line.find_first_not_of(" \t\r");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r");
        line = line.substr(b, e - b + 1);

        if (line.rfind("```", 0) == 0) continue; // cerca de código

        while (!line.empty() && line.front() == '`') line.erase(0, 1);
        while (!line.empty() && line.back() == '`') line.pop_back();
        if (line.rfind("$ ", 0) == 0 || line.rfind("# ", 0) == 0) line.erase(0, 2);

        b = line.find_first_not_of(" \t");
        if (b == std::string::npos) continue;
        e = line.find_last_not_of(" \t");
        return line.substr(b, e - b + 1);
    }
    return "";
}

// ---- Helpers UTF-8 -------------------------------------------------------
// O buffer de edição guarda bytes; o cursor precisa andar por codepoint para
// não parar no meio de um caractere acentuado.

inline bool isUtf8Continuation(unsigned char b) { return (b & 0xC0) == 0x80; }

// Posição do início do codepoint anterior a `pos`.
inline size_t utf8Prev(const std::string& s, size_t pos) {
    if (pos == 0) return 0;
    do { --pos; } while (pos > 0 && isUtf8Continuation((unsigned char)s[pos]));
    return pos;
}

// Posição logo após o codepoint que começa em/contém `pos`.
inline size_t utf8Next(const std::string& s, size_t pos) {
    if (pos >= s.size()) return s.size();
    ++pos;
    while (pos < s.size() && isUtf8Continuation((unsigned char)s[pos])) ++pos;
    return pos;
}

// Largura de exibição aproximada: 1 coluna por codepoint (caracteres
// full-width CJK não são tratados).
inline size_t displayWidth(const std::string& s, size_t end = std::string::npos) {
    size_t n = std::min(end, s.size());
    size_t w = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!isUtf8Continuation((unsigned char)s[i])) ++w;
    }
    return w;
}

} // namespace cli::util
