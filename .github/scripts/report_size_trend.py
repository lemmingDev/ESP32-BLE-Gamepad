#!/usr/bin/env python3
"""Append the latest compile-sketches size report to a running CSV history
and write a comparison-to-previous-run summary.

Used by .github/workflows/report-size-trend.yml on scheduled (monthly)
builds, which have no pull request to post a size-delta comment on. The CSV history
itself is persisted across runs as a chained GitHub Actions artifact (see
the workflow) rather than a database or spreadsheet.
"""
import argparse
import csv
import datetime
import glob
import json
import os

FIELDNAMES = [
    "date", "commit", "board", "sketch",
    "flash_bytes", "flash_max", "flash_percent",
    "ram_bytes", "ram_max", "ram_percent",
]


def load_history(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def size_entry(sizes, name):
    for size in sizes:
        if size["name"] == name:
            return size
    return None


def parse_reports(reports_dir, date):
    rows = []
    for report_path in sorted(glob.glob(os.path.join(reports_dir, "*.json"))):
        with open(report_path) as f:
            report = json.load(f)

        commit = report.get("commit_hash", "")[:7]
        for board in report.get("boards", []):
            for sketch in board.get("sketches", []):
                if not sketch.get("compilation_success"):
                    continue

                flash = size_entry(sketch["sizes"], "flash")
                ram = size_entry(sketch["sizes"], "RAM for global variables")

                rows.append({
                    "date": date,
                    "commit": commit,
                    "board": board["board"],
                    "sketch": sketch["name"],
                    "flash_bytes": flash["current"]["absolute"] if flash else "",
                    "flash_max": flash["maximum"] if flash else "",
                    "flash_percent": flash["current"]["relative"] if flash else "",
                    "ram_bytes": ram["current"]["absolute"] if ram else "",
                    "ram_max": ram["maximum"] if ram else "",
                    "ram_percent": ram["current"]["relative"] if ram else "",
                })
    return rows


def write_history(path, history):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(history)


def write_summary(summary_path, history, new_rows):
    # history is chronological (each run's rows are appended in order), so
    # the last occurrence of a given key is the most recent prior entry.
    previous_by_key = {}
    for row in history:
        previous_by_key[(row["board"], row["sketch"])] = row

    lines = [
        "## Monthly size trend",
        "",
        "| Board | Sketch | Flash | Flash delta | RAM | RAM delta |",
        "|---|---|---|---|---|---|",
    ]

    for row in new_rows:
        key = (row["board"], row["sketch"])
        previous = previous_by_key.get(key)

        flash_delta = "n/a"
        ram_delta = "n/a"
        if previous and row["flash_bytes"] and previous["flash_bytes"]:
            flash_delta = f"{int(row['flash_bytes']) - int(previous['flash_bytes']):+d}"
        if previous and row["ram_bytes"] and previous["ram_bytes"]:
            ram_delta = f"{int(row['ram_bytes']) - int(previous['ram_bytes']):+d}"

        sketch_name = row["sketch"].removeprefix("examples/")
        lines.append(
            f"| {row['board']} | {sketch_name} | {row['flash_bytes']} B "
            f"({row['flash_percent']}%) | {flash_delta} | {row['ram_bytes']} B "
            f"({row['ram_percent']}%) | {ram_delta} |"
        )

    with open(summary_path, "a") as f:
        f.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reports-dir", required=True, help="Directory of compile-sketches JSON reports")
    parser.add_argument("--history-csv", required=True, help="Path to the running CSV history (read + overwritten)")
    parser.add_argument("--summary-file", required=True, help="File to append a Markdown summary to (e.g. $GITHUB_STEP_SUMMARY)")
    args = parser.parse_args()

    date = datetime.date.today().isoformat()

    history = load_history(args.history_csv)
    new_rows = parse_reports(args.reports_dir, date)

    if not new_rows:
        print("No successfully compiled sketches found in reports; nothing to record.")
        return

    write_summary(args.summary_file, history, new_rows)
    write_history(args.history_csv, history + new_rows)
    print(f"Recorded {len(new_rows)} sketch size entries for {date}.")


if __name__ == "__main__":
    main()
