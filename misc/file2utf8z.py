#!/usr/bin/env python3
"""
dumps a file as a null-terminated "extern const char[]" array

usage: python3 ./file2utf8z.py <variable name>
"""
import sys

raw = sys.stdin.read().strip().encode("utf8")

print(f"extern const char {sys.argv[1]}[] = {{", end="")
for i, char in enumerate(raw):
  if i % 8 == 0:
    print("\n\t", end="")
  print(f"{hex(char)}", end=", ")
print("\n\t0x00\n};\n")
