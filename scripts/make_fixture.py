#!/usr/bin/env python3
"""Cut the first N messages of an ITCH day into a small test fixture.

Nasdaq's sample files frame each message with a 2-byte big-endian length,
so the cut always falls on a message boundary. Reads .gz files directly.

Usage: scripts/make_fixture.py data/12302019.NASDAQ_ITCH50.gz \
           tests/data/fixture.itch [--messages 200000]
"""

import argparse
import gzip
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", help="ITCH file, plain or .gz")
    parser.add_argument("dest", help="output fixture path")
    parser.add_argument("--messages", type=int, default=200_000)
    args = parser.parse_args()

    opener = gzip.open if args.source.endswith(".gz") else open
    written = 0
    with opener(args.source, "rb") as src, open(args.dest, "wb") as dst:
        while written < args.messages:
            header = src.read(2)
            if len(header) < 2:
                break
            body = src.read(int.from_bytes(header, "big"))
            dst.write(header + body)
            written += 1

    print(f"wrote {written} messages to {args.dest}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
