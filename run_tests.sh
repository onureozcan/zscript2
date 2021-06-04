#!/bin/bash

binary=./cmake-build-debug-mingw/zero.exe
binary_linux=./cmake-build-debug-remote-host/zero


test() {
  echo "=============================================================="
  echo "testing $1 ... ($binary)"

  local test_file_path="test_files/$1.ze"
  local expected_content_path="test_expected/$1.txt"
  local expected_content="$(cat $expected_content_path  | tr -d '\r')"

  $binary "$test_file_path" --interpret 2>/dev/null | tr -d '\r' > tmp.txt
  local result="$(cat tmp.txt)"

  if [ "$expected_content" = "$result" ]; then
      echo " Passed (interpreted mode)"
  else
      echo " FAILED (interpreted mode)"
      diff "$expected_content_path" tmp.txt
  fi

  $binary "$test_file_path" 2>/dev/null | tr -d '\r' > tmp.txt
  local result="$(cat tmp.txt)"

  if [ "$expected_content" = "$result" ]; then
      echo " Passed (jit mode)"
  else
      echo " FAILED (jit mode)"
      diff "$expected_content_path" tmp.txt
  fi

  rm tmp.txt
}

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  binary=$binary_linux
fi

test "hello"
test "basic_printing"
test "expressions"
test "control_flow"
test "functions"
test "recursive"
test "type_parameters"
test "named_functions"
