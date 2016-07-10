#!/usr/bin/env bash

overall_scale=1.0

formats_scale=50.0
tokens_scale=500.0
space_scale=200.0
newline_scale=5.0

IFS=$'\n'

formats=(
  $(
    for s in {0..7}; do
      for i in 0; do
        for j in {0..7}; do
          echo "\x1b[$s;4${i};9${j}m"
        done
      done
    done
  )
)

tokens=(
  '8.167/100:a'
  '1.492/100:b'
  '2.782/100:c'
  '4.253/100:d'
  '12.702/100:e'
  '2.228/100:f'
  '2.015/100:g'
  '6.094/100:h'
  '6.966/100:i'
  '0.153/100:j'
  '0.772/100:k'
  '4.025/100:l'
  '2.406/100:m'
  '6.749/100:n'
  '7.507/100:o'
  '1.929/100:p'
  '0.095/100:q'
  '5.987/100:r'
  '6.327/100:s'
  '9.056/100:t'
  '2.758/100:u'
  '0.978/100:v'
  '2.361/100:w'
  '0.150/100:x'
  '1.974/100:y'
  '0.074/100:z'
)

apply_scale() {
  local scale=$1
  shift
  for val in "$@"; do
    # NOTE: Leave this inside the loop to avoid divide-by-zero when empty.
    relative=$(bc <<< "scale = 20; $overall_scale*$scale/$#")
    printf '%s:%s\n' "$relative" "$val"
  done
}

explicit_scale() {
  local scale=$1
  shift
  for pair in "$@"; do
    local relative=$(echo "$pair" | cut -d: -f1)
    local val=$(echo "$pair" | cut -d: -f2)
    relative=$(bc <<< "scale = 20; $relative*$overall_scale*$scale")
    printf '%s:%s\n' "$relative" "$val"
  done
}

formats=($(apply_scale "$formats_scale" "${formats[@]}"))
tokens=($(explicit_scale "$tokens_scale" "${tokens[@]}"))

all_events=(
  "${formats[@]}" "${tokens[@]}"
  "$(apply_scale "$newline_scale" '\n')"
  "$(apply_scale "$space_scale" ' ')"
)

process_input() {
  while :; do
    read -d '' -N 1 -s char
    # [Ctrl]+D: quit.
    if [ "$char" = $'\004' ]; then
      break
    fi
    # [Esc]: kill formats.
    if [ "$char" = $'\e' ]; then
      printf '%s\n' "${formats[@]}" | sed 's/.*:/0:/'
      # Try to restore terminal color.
      echo $'\e[0;40;37m' 1>&2
    fi
    # [Backspace]: cause mayhem. (This kills the default action, which is the
    # only way to completely empty the action timer, assuming all other actions
    # have been removed. The program currently can't handle zero actions.)
    if [ "$char" = $'\x7f' ]; then
      echo $'\e[0;40;37m' 1>&2
      char='check_for_updates'
    fi
    # Default: kill char.
    if [ -z "$char" ]; then
      char='\n'
    fi
    echo "0:$char"
  done
}

cd "$(dirname "$0")" || exit 1

make exponential-printer || exit 1

{
  printf '%s\n' "${all_events[@]}"
  process_input
} | ./exponential-printer
