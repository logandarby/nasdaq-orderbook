#!/usr/bin/env bash
# Download one trading day of Nasdaq TotalView-ITCH 5.0 into data/.
# Usage: scripts/fetch_data.sh [MMDDYYYY]
# Default is 12302019, the smallest file on the server (about 3.5 GB gzipped).
# Other days: https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/
set -euo pipefail

day="${1:-12302019}"
root="$(cd "$(dirname "$0")/.." && pwd)"
file="$day.NASDAQ_ITCH50"
url="https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/$file.gz"

mkdir -p "$root/data"
cd "$root/data"

if [[ -f "$file" ]]; then
  echo "data/$file already exists"
  exit 0
fi

# -C - resumes a partial download.
curl -fL --retry 3 -C - -o "$file.gz" "$url"

# Keep the .gz: make_fixture.py reads it directly, and it is smaller to keep.
if command -v pigz >/dev/null; then
  pigz -dk "$file.gz"
else
  gunzip -k "$file.gz"
fi
ls -lh "$file" "$file.gz"
