#!/usr/bin/env python3
"""
dumps the version as a null-terminated "extern const char[]" array
"""
import datetime
import sys

version = datetime.datetime.now().strftime("%Y.%m.%d")
if len(sys.argv) == 2:
  version = sys.argv[1]
raw = version.encode("utf8")

print(f"extern const char VERSION[] = {{", end="")
for i, char in enumerate(raw):
  if i % 8 == 0:
    print("\n\t", end="")
  print(f"{hex(char)}", end=", ")
print("\n\t0x00\n};\n")
