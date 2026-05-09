#!/usr/bin/env bash
set -euo pipefail

out="configs/stress_extreme_48x4_dense.json"

cat > "$out" <<'HEAD'
{
  "name": "Stress Extreme 48x4 Dense",
  "description": "High-density benchmark: 4 voices x 48 steps with dense same-time, shifted, and intra-voice separation constraints.",
  "solution_length": 48,
  "num_voices": 4,
  "backtrack_method": "intelligent",
  "search_options": {
    "random_seed": 1,
    "timeout_ms": 180000
  },
  "voices": [
    { "rhythm": { "duration_values": ["1/16", "1/8", "3/16", "1/4", "3/8", "1/2"] }, "pitch": { "midi_values": [48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84] } },
    { "rhythm": { "duration_values": ["1/16", "1/8", "3/16", "1/4", "3/8", "1/2"] }, "pitch": { "midi_values": [48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84] } },
    { "rhythm": { "duration_values": ["1/16", "1/8", "3/16", "1/4", "3/8", "1/2"] }, "pitch": { "midi_values": [48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84] } },
    { "rhythm": { "duration_values": ["1/16", "1/8", "3/16", "1/4", "3/8", "1/2"] }, "pitch": { "midi_values": [48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84] } }
  ],
  "rules": [
    {
      "rule_type": "r-twelve-tone-voice1",
      "constraint_function": { "type": "builtin", "function": "all_different", "parameters": [] },
      "indices": [0,1,2,3,4,5,6,7,8,9,10,11],
      "target_voice": 0,
      "target_component": "pitch",
      "enabled": true,
      "priority": 10,
      "description": "Voice 0 local all-different first block"
    }
  ],
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

# Pairwise no-unison at same positions: 6 * 48 = 288
for v1 in 0 1 2 3; do
  for v2 in 0 1 2 3; do
    if [[ "$v1" -lt "$v2" ]]; then
      for i in $(seq 0 47); do
        append_rule "{\"id\":\"no_unison_${v1}_${v2}_${i}\",\"type\":\"basic_constraint\",\"expression\":{\"operator\":\"!=\",\"left\":\"voice[${v1}].pitch[${i}]\",\"right\":\"voice[${v2}].pitch[${i}]\"},\"mode\":\"constraint\",\"description\":\"No unison v${v1}/v${v2} at ${i}\"}"
      done
    fi
  done
done

# Pairwise shifted anti-alignment: 6 * 47 = 282
for v1 in 0 1 2 3; do
  for v2 in 0 1 2 3; do
    if [[ "$v1" -lt "$v2" ]]; then
      for i in $(seq 0 46); do
        j=$((i+1))
        append_rule "{\"id\":\"no_shift_${v1}_${v2}_${i}\",\"type\":\"basic_constraint\",\"expression\":{\"operator\":\"!=\",\"left\":\"voice[${v1}].pitch[${i}]\",\"right\":\"voice[${v2}].pitch[${j}]\"},\"mode\":\"constraint\",\"description\":\"No shifted match v${v1}[${i}] vs v${v2}[${j}]\"}"
      done
    fi
  done
done

# Intra-voice distance-2 anti-repeat: 4 * 46 = 184
for v in 0 1 2 3; do
  for i in $(seq 0 45); do
    j=$((i+2))
    append_rule "{\"id\":\"anti_repeat2_${v}_${i}\",\"type\":\"basic_constraint\",\"expression\":{\"operator\":\"!=\",\"left\":\"voice[${v}].pitch[${i}]\",\"right\":\"voice[${v}].pitch[${j}]\"},\"mode\":\"constraint\",\"description\":\"No repeat at distance 2 voice ${v}\"}"
  done
done

# Start/end anchors for all voices
append_rule '{"id":"anchor_v0_start","type":"basic_constraint","expression":{"operator":"==","left":"voice[0].pitch[0]","right":{"value":60}},"mode":"constraint","description":"Anchor v0 start"}'
append_rule '{"id":"anchor_v1_start","type":"basic_constraint","expression":{"operator":"==","left":"voice[1].pitch[0]","right":{"value":61}},"mode":"constraint","description":"Anchor v1 start"}'
append_rule '{"id":"anchor_v2_start","type":"basic_constraint","expression":{"operator":"==","left":"voice[2].pitch[0]","right":{"value":62}},"mode":"constraint","description":"Anchor v2 start"}'
append_rule '{"id":"anchor_v3_start","type":"basic_constraint","expression":{"operator":"==","left":"voice[3].pitch[0]","right":{"value":63}},"mode":"constraint","description":"Anchor v3 start"}'
append_rule '{"id":"anchor_v0_end","type":"basic_constraint","expression":{"operator":"==","left":"voice[0].pitch[47]","right":{"value":72}},"mode":"constraint","description":"Anchor v0 end"}'
append_rule '{"id":"anchor_v1_end","type":"basic_constraint","expression":{"operator":"==","left":"voice[1].pitch[47]","right":{"value":73}},"mode":"constraint","description":"Anchor v1 end"}'
append_rule '{"id":"anchor_v2_end","type":"basic_constraint","expression":{"operator":"==","left":"voice[2].pitch[47]","right":{"value":74}},"mode":"constraint","description":"Anchor v2 end"}'
append_rule '{"id":"anchor_v3_end","type":"basic_constraint","expression":{"operator":"==","left":"voice[3].pitch[47]","right":{"value":75}},"mode":"constraint","description":"Anchor v3 end"}'

cat >> "$out" <<'TAIL'
  ]
}
TAIL

# Basic sanity info
count=$(grep -c '"id"' "$out" || true)
echo "Wrote $out"
echo "dynamic_rules: $count"
