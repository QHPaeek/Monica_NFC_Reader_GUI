#!/bin/bash
# copy-deps.sh — 递归拷贝 MinGW DLL 依赖
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
    local basename
    basename=$(basename "$1")
    [ -n "${COPIED[$basename]}" ] && return

    local found="$MINGW_BIN/$basename"
    if [ ! -f "$found" ]; then return; fi  # 系统 DLL, 跳过

    cp "$found" "$DEST/"
    COPIED[$basename]=1
    echo "  [COPY] $basename"

    # 递归解析此 DLL 的依赖
    local dep
    for dep in $(objdump -p "$found" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'); do
        resolve_dll "$dep"
    done
}

echo "[DEPS] Resolving dependencies..."

# 1. 解析 EXE 的依赖
for dep in $(objdump -p "$EXE" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'); do
    resolve_dll "$dep"
done

# 2. 解析 Qt 插件的依赖
find "$DEST" -name "*.dll" | while read -r plug; do
    echo "$plug" | grep -q "Monica_NFC_Reader_GUI" && continue
    for dep in $(objdump -p "$plug" 2>/dev/null | grep "DLL Name:" | awk '{print $3}'); do
        resolve_dll "$dep"
    done
done

echo "[DEPS] Done. Copied ${#COPIED[@]} DLLs."
