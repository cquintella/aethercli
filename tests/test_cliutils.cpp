/*
 * Copyright (c) 2026 caq@intelliurb.com
 * PROPRIEDADE DA INTELLIURB - SOMENTE USO INTERNO
 */
#include <catch2/catch_test_macros.hpp>

#include "CliUtils.hpp"

using namespace cli::util;

TEST_CASE("split: tokens simples separados por espaço") {
    REQUIRE(split("show ip route") == std::vector<std::string>{"show", "ip", "route"});
}

TEST_CASE("split: espaços múltiplos e bordas são ignorados") {
    REQUIRE(split("  show   version  ") == std::vector<std::string>{"show", "version"});
}

TEST_CASE("split: string vazia e só espaços") {
    REQUIRE(split("").empty());
    REQUIRE(split("     ").empty());
}

TEST_CASE("split: aspas duplas agrupam token com espaços") {
    REQUIRE(split("play music \"Dream On\"") == std::vector<std::string>{"play", "music", "Dream On"});
}

TEST_CASE("split: aspas sem fechamento agrupam até o fim") {
    REQUIRE(split("echo \"a b c") == std::vector<std::string>{"echo", "a b c"});
}

TEST_CASE("escape_shell_arg: envolve em aspas duplas") {
    REQUIRE(escape_shell_arg("abc") == "\"abc\"");
}

TEST_CASE("escape_shell_arg: escapa metacaracteres do sh dentro de aspas") {
    REQUIRE(escape_shell_arg("a$b") == "\"a\\$b\"");
    REQUIRE(escape_shell_arg("a`b") == "\"a\\`b\"");
    REQUIRE(escape_shell_arg("a\\b") == "\"a\\\\b\"");
    REQUIRE(escape_shell_arg("a\"b") == "\"a\\\"b\"");
}

TEST_CASE("escape_shell_arg: ponto-e-vírgula e pipe ficam literais") {
    REQUIRE(escape_shell_arg("; rm -rf /") == "\"; rm -rf /\"");
    REQUIRE(escape_shell_arg("a|b") == "\"a|b\"");
}

TEST_CASE("stripReasoning: remove bloco <think> e espaços das bordas") {
    REQUIRE(stripReasoning("<think>hmm, o usuário quer...</think>\n\nshow version") == "show version");
    REQUIRE(stripReasoning("  \n show version \n ") == "show version");
    REQUIRE(stripReasoning("show version") == "show version");
    REQUIRE(stripReasoning("<think>só pensamento</think>") == "");
    REQUIRE(stripReasoning("<think>sem fechamento... show version") == "<think>sem fechamento... show version");
}

TEST_CASE("extractCommandSuggestion: primeira linha útil") {
    REQUIRE(extractCommandSuggestion("show ip route") == "show ip route");
    REQUIRE(extractCommandSuggestion("\n\n  show version  \n") == "show version");
    REQUIRE(extractCommandSuggestion("") == "");
}

TEST_CASE("extractCommandSuggestion: ignora cercas de código e crases") {
    REQUIRE(extractCommandSuggestion("```\nshow version\n```") == "show version");
    REQUIRE(extractCommandSuggestion("```bash\nshow files /tmp\n```\nexplicação...") == "show files /tmp");
    REQUIRE(extractCommandSuggestion("`show version`") == "show version");
}

TEST_CASE("extractCommandSuggestion: remove prompt decorativo '$ '") {
    REQUIRE(extractCommandSuggestion("$ show version") == "show version");
    REQUIRE(extractCommandSuggestion("# show version") == "show version");
}

TEST_CASE("extractCommandSuggestion: resposta '%' passa adiante para o chamador decidir") {
    REQUIRE(extractCommandSuggestion("% Nenhum comando disponível para isso.") == "% Nenhum comando disponível para isso.");
}

TEST_CASE("utf8: navegação por codepoint em texto acentuado") {
    std::string s = "café"; // 'é' ocupa 2 bytes: c a f \xc3 \xa9
    REQUIRE(s.size() == 5);
    REQUIRE(displayWidth(s) == 4);

    REQUIRE(utf8Next(s, 3) == 5); // pula o 'é' inteiro
    REQUIRE(utf8Prev(s, 5) == 3); // volta para o início do 'é'
    REQUIRE(utf8Next(s, 0) == 1);
    REQUIRE(utf8Prev(s, 1) == 0);
    REQUIRE(utf8Prev(s, 0) == 0);
    REQUIRE(utf8Next(s, 5) == 5);
}

TEST_CASE("utf8: displayWidth parcial conta colunas até o limite") {
    std::string s = "café";
    REQUIRE(displayWidth(s, 3) == 3);
    REQUIRE(displayWidth(s, 5) == 4);
    REQUIRE(displayWidth(s, 4) == 4); // corte no meio do 'é' não conta a continuação
}
