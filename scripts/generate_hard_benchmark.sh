#!/usr/bin/env bash
set -euo pipefail

out="configs/stress_hard_32x3_dense.json"

cat > "$out" <<'HEAD'
{
  "name": "Stress Hard 32x3 Dense",
  "description": "Hard-but-measurable benchmark: 3 voices x 32 with dense anti-alignment constraints.",
  "solution_length": 32,
  "num_voices": 3,
  "backtrack_method": "intelligent",
  "search_options": {
    "random_seed": 1,
    "timeout_ms": 180000
  },
  "voices": [
    { "rhythm": { "duration_values": ["1/16", "1/8", "3/16", "1/4", "3/8"] }, "pitch": { "midi_values": [52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78] } },
    { "rhythm": { "duration_values": ["1/16", "1/8", "3/16", "1/4", "3/8"] }, "pitch": { "midi_values": [52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78] } },
    { "rhythm": { "duration_values": ["1/16", "1/8", "3/16", "1/4", "3/8"] }, "pitch": { "midi_values": [52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78] } }
  ],
  "rules": [],
  "dynamic_rules": [
HEAD

first=1
append_rule() {
  local rule="$1"
  if [[ $first -eq 0 ]]; then
    printf ',\n' >> "$out"
  fi
  first=0
  printf '    %s' "$rule" >> "$out"
}

# Same-position no-unison: 3 pairs * 32 = 96
for v1 in 0 1 2; do
  for v2 in 0 1 2; do
    if [[ "$v1" -lt "$v2" ]]; then
      for i in $(seq 0 31); do
        append_rule "{\"id\":\"no_unison_${v1}_${v2}_${i}\",\"type\":\"basic_constraint\",\"expression\":{\"operator\":\"!=\",\"left\":\"voice[${v1}].pitch[${i}]\",\"right\":\"voice[${v2}].pitch[${i}]\"},\"mode\":\"constraint\",\"description\":\"No unison v${v1}/v${v2} at ${i}\"}"
      done
    fi
  done
done

# Shifted anti-alignment: 3 pairs * 31 = 93
for v1 in 0 1 2; do
  for v2 in 0 1 2; do
    if [[ "$v1" -lt "$v2" ]]; then
      for i in $(seq 0 30); do
        j=$((i+1))
        append_rule "{\"id\":\"no_shift_${v1}_${v2}_${i}\",\"type\":\"basic_constraint\",\"expression\":{\"operator\":\"!=\",\"left\":\"voice[${v1}].pitch[${i}]\",\"right\":\"voice[${v2}].pitch[${j}]\"},\"mode\":\"constraint\",\"description\":\"No shifted match v${v1}[${i}] vs v${v2}[${j}]\"}"
      done
    fi
  done
done

# Intra-voice distance-2 anti-repeat: 3 voices * 30 = 90
for v in 0 1 2; do
  for i in $(seq 0 29); do
    j=$((i+2))
    append_rule "{\"id\":\"anti_repeat2_${v}_${i}\",\"type\":\"basic_constraint\",\"expression\":{\"operator\":\"!=\",\"left\":\"voice[${v}].pitch[${i}]\",\"right\":\"voice[${v}].pitch[${j}]\"},\"mode\":\"constraint\",\"description\":\"No repeat at distance 2 voice ${v}\"}"
  done
done

append_rule '{"id":"anchor_v0_start","type":"basic_constraint","expression":{"operator":"==","left":"voice[0].pitch[0]","right":{"value":60}},"mode":"constraint","description":"Anchor v0 start"}'
append_rule '{"id":"anchor_v1_start","type":"basic_constraint","expression":{"operator":"==","left":"voice[1].pitch[0]","right":{"value":61}},"mode":"constraint","description":"Anchor v1 start"}'
append_rule '{"id":"anchor_v2_start","type":"basic_constraint","expression":{"operator":"==","left":"voice[2].pitch[0]","right":{"value":62}},"mode":"constraint","description":"Anchor v2 start"}'
append_rule '{"id":"anchor_v0_end","type":"basic_constraint","expression":{"operator":"==","left":"voice[0].pitch[31]","right":{"value":70}},"mode":"constraint","description":"Anchor v0 end"}'
append_rule '{"id":"anchor_v1_end","type":"basic_constraint","expression":{"operator":"==","left":"voice[1].pitch[31]","right":{"value":71}},"mode":"constraint","description":"Anchor v1 end"}'
append_rule '{"id":"anchor_v2_end","type":"basic_constraint","expression":{"operator":"==","left":"voice[2].pitch[31]","right":{"value":72}},"mode":"constraint","description":"Anchor v2 end"}'

cat >> "$out" <<'TAIL'
  ]
}
TAIL

echo "Wrote $out"
echo "dynamic_rules: $(grep -c '"id"' "$out" || true)"
