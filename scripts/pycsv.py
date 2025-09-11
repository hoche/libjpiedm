#!/usr/bin/env python3

# Silly little script for helping me debug. Strips out columns from the CSV file.

import sys
import os
import csv

remove_list = [
    "E1",
    "E2",
    "E3",
    "E4",
    "E5",
    "E6",
    "C1",
    "C2",
    "C3",
    "C4",
    "C5",
    "C6"
]

def remove_columns(reader, writer, columns_to_remove):
    # Keep only the columns not in the removal list
    fieldnames = [col for col in reader.fieldnames if col not in columns_to_remove]

    writer = csv.DictWriter(sys.stdout, fieldnames=fieldnames, lineterminator="\n")
    writer.writeheader()

    for row in reader:
        filtered_row = {col: row[col] for col in fieldnames}
        writer.writerow(filtered_row)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: script.py [file]", file=sys.stderr)
        sys.exit(1)

    maybe_file = sys.argv[1]

    # If the last argument looks like a file, open it; otherwise read stdin
    try:
        infile = open(maybe_file, newline="", encoding="utf-8")
    except FileNotFoundError:
        infile = sys.stdin
        columns = sys.argv[1:]

    reader = csv.DictReader(infile)
    remove_columns(reader, sys.stdout, remove_list)

    if infile is not sys.stdin:
        infile.close()

