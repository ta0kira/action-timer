#!/usr/bin/env bash

overall_scale=0.1
formats_scale=50.0
tokens_scale=250.0
newline_scale=5.0

IFS=$'\n'

formats=($(
  for s in {0..7}; do
    for i in {1..7}; do
      eval echo "$'\x1b[$s;49;9${i}m'"
    done
  done
))

tokens=($(
  for s in {a..z}{a..z}' '; do
    echo "$s"
done))

apply_scale() {
  local scale=$1
  shift
  scale=$(bc <<< "scale = 8; $overall_scale*$scale/$#")
  for val in "$@"; do
    printf '%s:%s\n' "$scale" "$val"
  done
}

formats=($(apply_scale "$formats_scale" "${formats[@]}"))
tokens=($(apply_scale "$tokens_scale" "${tokens[@]}"))

cd "$(dirname "$0")" || exit 1

[ exponential -nt exponential.cpp ] || \
  c++ -Wall -pedantic -std=c++11 -O2 -I../include exponential.cpp -o exponential -pthread || exit 1

./exponential "${formats[@]}" "${tokens[@]}" "$(bc <<< "scale = 8; $overall_scale*$newline_scale"):"$'\n'
