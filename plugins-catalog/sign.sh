#!/usr/bin/env bash
# Пересобирает index.json из plugins/*.plugin и подписывает его ключом Mogram.
# Ключ: ~/.config/mogram/update-key.pem (или MOGRAM_UPDATE_KEY). Запуск из корня репозитория.
set -euo pipefail
key=${MOGRAM_UPDATE_KEY:-$HOME/.config/mogram/update-key.pem}
[[ -f "$key" ]] || { echo "Нет ключа $key" >&2; exit 1; }
# Модерация: плагин с зашифрованной вставкой, exec/eval или загрузкой кода в каталог не попадает.
python3 lint.py plugins/*.plugin || { echo "Проверка не пройдена, каталог не подписан" >&2; exit 1; }
python3 - <<'PY' > index.json.tmp
import ast, glob, hashlib, json, os, re, sys
out = []
seen = {}
for path in sorted(glob.glob("plugins/*.plugin")):
    data = open(path, "rb").read()
    src = data.decode("utf-8")
    def field(name):
        m = re.search(r'^__%s__\s*=\s*(.+)$' % name, src, re.M)
        if not m:
            return ""
        try:
            return ast.literal_eval(m.group(1))
        except (ValueError, SyntaxError, TypeError):
            return ""
    pid = field("id")
    if not pid:
        sys.exit("У %s нет корректного __id__, каталог не собран" % path)
    if pid in seen:
        sys.exit("Дубликат id %r: %s и %s" % (pid, seen[pid], path))
    seen[pid] = path
    out.append({
        "id": pid, "name": field("name"),
        "description": field("description"), "details": field("details"), "author": field("author"),
        "version": field("version"), "icon": field("icon"), "permissions": field("permissions") or [],
        "file": os.path.basename(path),
        "sha256": hashlib.sha256(data).hexdigest(), "size": len(data),
    })
print(json.dumps({"plugins": out}, ensure_ascii=False, indent=2))
PY
mv index.json.tmp index.json
# Файлы плагинов лежат в корне рядом с index.json: клиент берёт их по имени.
cp plugins/*.plugin .
msg=$(mktemp)
trap 'rm -f "$msg"' EXIT
{ printf 'mogram-plugins\n'; cat index.json; } > "$msg"
openssl pkeyutl -sign -rawin -inkey "$key" -in "$msg" | base64 -w0 > index.json.sig
echo "Готово: $(grep -c '"id"' index.json) плагинов"
