#!/usr/bin/env bash
set -euo pipefail

# Compare exact heuristic ordering (top_k=0) against approximate shortlist mode.
# Usage:
#   scripts/benchmark_heuristic_topk.sh
#   scripts/benchmark_heuristic_topk.sh configs/heuristic_priority_direction_8x1.json configs/stress_hard_32x3_dense.json

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BIN="./bin/test-max-wrapper"
TOPK_EXACT=0
TOPK_APPROX=3

DEFAULT_CONFIGS=(
  "configs/heuristic_priority_direction_8x1.json"
  "configs/stress_hard_32x3_dense.json"
)

if [[ $# -gt 0 ]]; then
  CONFIGS=("$@")
else
  CONFIGS=("${DEFAULT_CONFIGS[@]}")
fi

if [[ ! -x "$BIN" ]]; then
  echo "Building $BIN ..."
  make -j2 bin/test-max-wrapper
fi

make_temp_config() {
  local in_cfg="$1"
  local out_cfg="$2"
  local topk="$3"

  python3 - "$in_cfg" "$out_cfg" "$topk" <<'PY'
import json
import sys

src, dst, topk_raw = sys.argv[1], sys.argv[2], sys.argv[3]
topk = int(topk_raw)

with open(src, 'r', encoding='utf-8') as f:
    cfg = json.load(f)

search = cfg.get('search_options')
if not isinstance(search, dict):
    search = {}
    cfg['search_options'] = search

search['heuristic_top_k'] = topk
search['heuristic_trace'] = False

with open(dst, 'w', encoding='utf-8') as f:
    json.dump(cfg, f, indent=2)
    f.write('\n')
PY
}

extract_signature() {
  local log_file="$1"

  local json_line
  json_line="$(grep -E '^\{.*\}$' "$log_file" | tail -n 1 || true)"

  python3 - "$json_line" <<'PY'
import json
import sys

line = sys.argv[1]
if not line:
    print('-')
    raise SystemExit(0)

try:
    payload = json.loads(line)
except json.JSONDecodeError:
    print('-')
    raise SystemExit(0)

voices = payload.get('voice_solutions')
if not isinstance(voices, list) or not voices:
    print('-')
    raise SystemExit(0)

v0 = voices[0]
if not isinstance(v0, list):
    print('-')
    raise SystemExit(0)

snippet = v0[:8]
print(','.join(str(x) for x in snippet) if snippet else '-')
PY
}

run_case() {
  local config="$1"
  local topk="$2"

  local tmp_cfg
  local log_file
  tmp_cfg="$(mktemp "/tmp/heur_topk_cfg_${topk}_XXXXXX")"
  log_file="$(mktemp "/tmp/heur_topk_log_${topk}_XXXXXX")"

  make_temp_config "$config" "$tmp_cfg" "$topk"
  /usr/bin/time -p "$BIN" "$tmp_cfg" > "$log_file" 2>&1 || true

  local status found elapsed real_s signature
  status="$(grep -E '^status=' "$log_file" | tail -n 1 | cut -d'=' -f2- || true)"
  found="$(grep -E '^found_solution=' "$log_file" | tail -n 1 | cut -d'=' -f2- || true)"
  elapsed="$(grep -E '^elapsed_ms=' "$log_file" | tail -n 1 | cut -d'=' -f2- || true)"
  real_s="$(grep -E '^real ' "$log_file" | tail -n 1 | awk '{print $2}' || true)"
  signature="$(extract_signature "$log_file")"

  [[ -n "$status" ]] || status="-"
  [[ -n "$found" ]] || found="-"
  [[ -n "$elapsed" ]] || elapsed="-"
  [[ -n "$real_s" ]] || real_s="-"

  printf '%s|%s|%s|%s|%s|%s\n' "$topk" "$status" "$found" "$elapsed" "$real_s" "$signature"

  rm -f "$tmp_cfg" "$log_file"
}

printf 'mode | config | status | found | elapsed_ms | real_s | voice0_prefix\n'
printf '%s\n' '-----|--------|--------|-------|------------|--------|-------------------------------'

for cfg in "${CONFIGS[@]}"; do
  if [[ ! -f "$cfg" ]]; then
    printf 'exact(top_k=%d) | %s | missing | - | - | - | -\n' "$TOPK_EXACT" "$cfg"
    printf 'approx(top_k=%d) | %s | missing | - | - | - | -\n' "$TOPK_APPROX" "$cfg"
    continue
  fi

  exact="$(run_case "$cfg" "$TOPK_EXACT")"
  approx="$(run_case "$cfg" "$TOPK_APPROX")"

  IFS='|' read -r e_topk e_status e_found e_elapsed e_real e_sig <<< "$exact"
  IFS='|' read -r a_topk a_status a_found a_elapsed a_real a_sig <<< "$approx"

  printf 'exact(top_k=%s) | %s | %s | %s | %s | %s | %s\n' "$e_topk" "$cfg" "$e_status" "$e_found" "$e_elapsed" "$e_real" "$e_sig"
  printf 'approx(top_k=%s) | %s | %s | %s | %s | %s | %s\n' "$a_topk" "$cfg" "$a_status" "$a_found" "$a_elapsed" "$a_real" "$a_sig"

  if [[ "$e_sig" != "$a_sig" && "$e_sig" != "-" && "$a_sig" != "-" ]]; then
    printf 'delta | %s | signature_changed | - | - | - | %s -> %s\n' "$cfg" "$e_sig" "$a_sig"
  fi

  printf '%s\n' '-----|--------|--------|-------|------------|--------|-------------------------------'
done
