import csv
import json
import re
import sys
import time
import unicodedata
import urllib.parse
import urllib.request
from difflib import SequenceMatcher
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "data" / "songs.csv"
CACHE = ROOT / "work" / "musicbrainz-release-cache.json"
REPORT = ROOT / "data" / "release-date-report.csv"
CACHE.parent.mkdir(exist_ok=True)


def normalize(value):
    value = unicodedata.normalize("NFKC", value).casefold()
    return "".join(character for character in value if character.isalnum())


def similarity(left, right):
    left = normalize(left)
    right = normalize(right)
    if left == right:
        return 1.0
    if left and (left in right or right in left):
        return 0.93
    return SequenceMatcher(None, left, right).ratio()


def artists(recording):
    values = []
    for credit in recording.get("artist-credit", []):
        if isinstance(credit, dict) and credit.get("name"):
            values.append(credit["name"])
    return " ".join(values)


def query_musicbrainz(title, performers):
    query = f'recording:"{title}"'
    url = (
        "https://musicbrainz.org/ws/2/recording/"
        f"?query={urllib.parse.quote(query)}&fmt=json&limit=25"
    )
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "MusicRank/1.0 (local-song-catalog)"},
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        payload = json.load(response)

    expected_artists = [part for part in performers.split("、") if part]
    candidates = []
    for recording in payload.get("recordings", []):
        date = recording.get("first-release-date", "")
        if not re.fullmatch(r"\d{4}(?:-\d{2})?(?:-\d{2})?", date):
            continue
        actual_artists = artists(recording)
        title_match = similarity(title, recording.get("title", ""))
        artist_match = max(
            (similarity(expected, actual_artists) for expected in expected_artists),
            default=0.0,
        )
        score = title_match * 0.72 + artist_match * 0.28
        candidates.append(
            {
                "date": date,
                "score": score,
                "track": recording.get("title", ""),
                "artist": actual_artists,
                "title_score": title_match,
                "artist_score": artist_match,
                "url": f"https://musicbrainz.org/recording/{recording.get('id', '')}",
            }
        )
    candidates.sort(key=lambda item: (-item["score"], item["date"]))
    candidates = [
        item
        for item in candidates
        if item["title_score"] >= 0.93
        and item["artist_score"] >= 0.82
        and re.fullmatch(r"\d{4}-\d{2}-\d{2}", item["date"])
    ]
    if not candidates or candidates[0]["score"] < 0.90:
        return None
    best_score = candidates[0]["score"]
    strong = [
        item
        for item in candidates
        if item["score"] >= max(0.90, best_score - 0.02)
    ]
    strong.sort(key=lambda item: item["date"])
    return strong[0]


def main():
    with CATALOG.open("r", encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        rows = list(reader)
        fieldnames = reader.fieldnames
    cache = json.loads(CACHE.read_text("utf-8")) if CACHE.exists() else {}

    for index, row in enumerate(rows, 1):
        key = f"{row['title']}\n{row['performers']}"
        if key not in cache:
            try:
                cache[key] = query_musicbrainz(row["title"], row["performers"])
            except Exception as error:
                print(f"[{index:03}/104] ERROR {row['title']}: {error}")
                cache[key] = {"retry": True}
            CACHE.write_text(
                json.dumps(cache, ensure_ascii=False, indent=2), "utf-8"
            )
            time.sleep(1.1)
        result = cache[key]
        if result and not result.get("retry"):
            print(
                f"[{index:03}/104] {row['title']} -> "
                f"{result['date']} ({result['score']:.3f})"
            )
        else:
            print(f"[{index:03}/104] {row['title']} -> unresolved")

    report = []
    matched = 0
    for row in rows:
        key = f"{row['title']}\n{row['performers']}"
        result = cache.get(key)
        if result and not result.get("retry"):
            row["release_date"] = result["date"]
            matched += 1
        report.append(
            {
                "title": row["title"],
                "performers": row["performers"],
                "release_date": row["release_date"],
                "status": "matched" if result and not result.get("retry") else "unresolved",
                "confidence": f"{result['score']:.3f}" if result and not result.get("retry") else "",
                "matched_track": result["track"] if result and not result.get("retry") else "",
                "matched_artist": result["artist"] if result and not result.get("retry") else "",
                "source_url": result["url"] if result and not result.get("retry") else "",
            }
        )

    with CATALOG.open("w", encoding="utf-8-sig", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    with REPORT.open("w", encoding="utf-8-sig", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=report[0].keys())
        writer.writeheader()
        writer.writerows(report)
    print(f"matched={matched}; unresolved={len(rows) - matched}")


if __name__ == "__main__":
    main()
