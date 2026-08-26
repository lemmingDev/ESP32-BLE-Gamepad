#!/usr/bin/env python3
"""Render an arduino/compile-sketches JSON size report as a Markdown table
and append it to the job's GitHub Actions step summary.

Used by .github/workflows/main.yml right after the "Compile sketches" step,
once per board in the build matrix, so memory usage (and its delta versus
the previous commit, when available) is visible straight on the workflow
run page instead of only in the weekly PR delta comment.
"""
import argparse
import glob
import json
import os

SIZE_NAMES = [
    ("flash", "Flash"),
    ("RAM for global variables", "RAM"),
]


def size_entry(sizes, name):
    for size in sizes:
        if size["name"] == name:
            return size
    return None


def fmt_bytes(n):
    return f"{n:,}"


def fmt_current(size):
    if size is None:
        return "n/a"
    current = size["current"]
    return f"{fmt_bytes(current['absolute'])} B ({current['relative']:.1f}%)"


def fmt_delta(size):
    if size is None or "delta" not in size:
        return ""
    absolute = size["delta"]["absolute"]
    relative = size["delta"]["relative"]
    if absolute > 0:
        return f"+{fmt_bytes(absolute)} B (+{relative:.1f}%) :small_red_triangle:"
    if absolute < 0:
        return f"{fmt_bytes(absolute)} B ({relative:.1f}%) :small_red_triangle_down:"
    return "no change"


def render_board(board):
    lines = [f"## Memory usage — `{board['board']}`", ""]

    sketches = board.get("sketches", [])
    if not sketches:
        lines.append("_No sketches compiled._")
        return lines

    header = ["Sketch"]
    for _, label in SIZE_NAMES:
        header += [label, f"Δ {label}"]
    lines.append(f"| {' | '.join(header)} |")
    lines.append(f"|{'---|' * len(header)}")

    for sketch in sketches:
        name = sketch["name"].removeprefix("examples/")
        if not sketch.get("compilation_success"):
            row = [name] + [":x: failed", ""] * len(SIZE_NAMES)
            lines.append(f"| {' | '.join(row)} |")
            continue

        row = [name]
        for size_name, _ in SIZE_NAMES:
            size = size_entry(sketch.get("sizes", []), size_name)
            row += [fmt_current(size), fmt_delta(size)]
        lines.append(f"| {' | '.join(row)} |")

    return lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--reports-dir", required=True)
    parser.add_argument("--summary-file", required=True)
    args = parser.parse_args()

    report_paths = sorted(glob.glob(os.path.join(args.reports_dir, "*.json")))
    if not report_paths:
        return

    lines = []
    for report_path in report_paths:
        with open(report_path) as f:
            report = json.load(f)

        for board in report.get("boards", []):
            lines += render_board(board)
            lines.append("")

    with open(args.summary_file, "a") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
