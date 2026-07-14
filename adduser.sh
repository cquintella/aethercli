#!/bin/bash
# Cria/atualiza um usuário delegando ao `aethercli --adduser`, que grava
# PBKDF2-HMAC-SHA256 (salt aleatório de 16 bytes + AETHERCLI_PEPPER opcional)
# em users.json. A validação de username/senha é feita pelo próprio binário.
# Uso: adduser.sh <users.json ou diretório> <username>

set -e

if [ "$#" -ne 2 ]; then
    echo "Uso: $0 <users.json (ou diretório de configuração)> <username>"
    exit 1
fi

DEST=$1
USERNAME=$2

if [ -d "$DEST" ]; then
    DIR=$DEST
else
    if [ "$(basename "$DEST")" != "users.json" ]; then
        echo "Erro: o arquivo de usuários deve se chamar users.json (recebido: $DEST)."
        exit 1
    fi
    DIR=$(dirname "$DEST")
fi

BIN=${AETHERCLI_BIN:-aethercli}
if ! command -v "$BIN" >/dev/null 2>&1; then
    echo "Erro: binário '$BIN' não encontrado no PATH (defina AETHERCLI_BIN)."
    exit 1
fi

# --adduser grava em <dir do -C>/users.json; o config em si não é lido nesse modo.
exec "$BIN" -C "$DIR/config.json" --adduser "$USERNAME"
