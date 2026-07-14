#!/bin/bash
# Script para gerar um novo usuário e adicioná-lo ao arquivo de usuários
# Formato: username:nome:e-mail:cripto

if [ "$#" -ne 4 ]; then
    echo "Uso: $0 <arquivo_de_usuarios> <username> <nome> <e-mail>"
    exit 1
fi

FILE=$1
USER=$2
NOME=$3
EMAIL=$4

if [ ${#USER} -lt 1 ] || [ ${#USER} -gt 32 ]; then
    echo "Erro: O username deve ter entre 1 e 32 caracteres (padrão Unix)."
    exit 1
fi

# Validar caracteres padrão do Unix para username
if ! [[ "$USER" =~ ^[a-z_][a-z0-9_-]*$ ]]; then
    echo "Erro: O username contém caracteres inválidos (deve iniciar com letra minúscula ou sublinhado e conter apenas letras minúsculas, números, sublinhados ou traços)."
    exit 1
fi

echo -n "Senha: "
read -s PASS
echo

if [ ${#PASS} -lt 8 ] || [ ${#PASS} -gt 255 ]; then
    echo "Erro: A senha deve ter entre 8 e 255 caracteres."
    exit 1
fi

echo -n "Confirme a senha: "
read -s PASS2
echo

if [ "$PASS" != "$PASS2" ]; then
    echo "Erro: As senhas não conferem."
    exit 1
fi

# Gera o hash SHA256 (incluindo o pepper se configurado no ambiente)
# O formato de hash esperado no C++ é hexadecimal contínuo (64 caracteres)
HASH=$(echo -n "${PASS}${AETHERCLI_PEPPER}" | sha256sum | awk '{print $1}')

# Dá >> no arquivo
echo "$USER:$NOME:$EMAIL:$HASH" >> "$FILE"

echo "Usuário '$USER' adicionado com sucesso em '$FILE'."
