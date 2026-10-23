#!/usr/bin/env bash
# 列出 docs/spec.md 每條規則對應的自動化測試數量（spec §11、tasks T23）。
# 測試名稱要包含規則編號，例如 TEST(EnergyTest, E3_CatchUpAfterLongGap)；
# 一個測試可以對應多條規則，例如 N6_W5_ConnectionLossAbortsBothWithin5s。
# 用法：scripts/check_spec_coverage.sh [--missing]   加上 --missing 只列出沒有自動化測試的規則
set -euo pipefail
cd "$(dirname "$0")/.."

ids=$(grep -oE '^\s*- \*\*[A-Z]+[0-9]+[a-z]?\*\*' docs/spec.md | grep -oE '[A-Z]+[0-9]+[a-z]?')
names=$(grep -rhoE 'TEST(_F)?\([A-Za-z0-9_]+, *[A-Za-z0-9_]+' tests | sed -E 's/.*, *//')

for id in $ids; do
    count=$(grep -cE "^([A-Z]+[0-9]+[a-z]?_)*${id}_" <<<"$names" || true)
    if [[ "${1:-}" != "--missing" || "$count" -eq 0 ]]; then
        printf '%-5s %s\n' "$id" "$count"
    fi
done
