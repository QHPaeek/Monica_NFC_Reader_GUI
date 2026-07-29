#!/bin/bash
# copy-deps.sh — 递归拷贝 MinGW DLL 依赖 (修复 subshell 问题)
set -e

EXE="$1"
DEST="$2"
MINGW_BIN="/c/msys64/mingw64/bin"

if [ ! -f "$EXE" ] || [ -z "$DEST" ]; then
    echo "Usage: $0 <exe_path> <dest_dir>"
    exit 1
fi

declare -A COPIED

resolve_dll() {
    local basename name
    basename=$(basename "$1" 2>/dev/null)
    [ -z "$basename" ] && return
    name=$(echo "$basename" | tr '[:upper:]' '[:lower:]')
    [ -n "${COPIED[$name]}" ] && return

    local found="$MINGW_BIN/$basename"
    if [ ! -f "$found" ]; then return; fi  # 系统 DLL

    cp "$found" "$DEST/"
    COPIED[$name]=1
    echo "  [COPY] $basename"

    # 递归
    local dep
    for dep in $(objdump -p "$found" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'); do
        resolve_dll "$dep"
    done
}

echo "[DEPS] Resolving dependencies..."

# 1. EXE 的依赖
for dep in $(objdump -p "$EXE" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'); do
    resolve_dll "$dep"
done

# 2. 已有 DLL 和插件的依赖 (避免 subshell)
LIST_FILE=$(mktemp)
find "$DEST" -name "*.dll" > "$LIST_FILE"
while IFS= read -r plug; do
    basename "$plug" | grep -qi "monica" && continue
    for dep in $(objdump -p "$plug" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'); do
        resolve_dll "$dep"
    done
done < "$LIST_FILE"
rm -f "$LIST_FILE"

# 3. 第二轮：新拷贝的 DLL 可能还有依赖
find "$DEST" -name "*.dll" > "$LIST_FILE"
while IFS= read -r plug; do
    for dep in $(objdump -p "$plug" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'); do
        resolve_dll "$dep"
    done
done < "$LIST_FILE"
rm -f "$LIST_FILE"

echo "[DEPS] Done. Copied ${#COPIED[@]} DLLs."
