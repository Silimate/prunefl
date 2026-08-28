import os
import sys

from . import PRUNEFL_BIN_PATH


def prunefl():
    os.execlp(PRUNEFL_BIN_PATH, "prunefl", *sys.argv[1:])


if __name__ == "__main__":
    prunefl()
