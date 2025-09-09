#!/usr/bin/env python3
import argparse
import os
from pathlib import Path


if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--unused")
  ap.add_argument("result")
  
  ns = ap.parse_args()
  if not os.path.isfile(ns.unused):
    print("No unused, toposort test.")
    exit(0)
    
  unused = Path(ns.unused).absolute()

  unused_set = set()
  with open(unused, encoding="utf8") as f:
    for unused_file in f:
      if unused_file.strip() == "":
        continue
      unused_set.add(unused.parent / unused_file.strip())

  final_set = set()
  with open(ns.result, encoding="utf8") as f:
    for result_file in f:
      if result_file.strip() == "":
        continue
      final_set.add(result_file)

  assert len(unused_set.union(final_set)) == len(final_set) + len(unused_set), "trimming failed"
