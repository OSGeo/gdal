#!/usr/bin/env python3
import re
import sys


def fix_block_headers(text, width=74):
    """
    Detects GDAL-style comment blocks and re-centers the internal text.
    'width' is the total length of the /******/ line.
    """
    # Regex explanation:
    # 1. Matches a line of at least 70 asterisks starting with /
    # 2. Captures the middle line starting with /* and ending with */
    # 3. Matches the closing line of asterisks
    pattern = r"(/\*{70,}/)\s*\n/\*\s*(.*?)\s*\*/\s*\n(/\*{70,}/)"

    def replace_header(match):
        content = match.group(2).strip()

        # Calculate padding for centering
        # We subtract 4 for the '/*' and '*/' markers
        usable_width = width - 4
        centered_content = content.center(usable_width)

        new_middle = f"/*{centered_content}*/"

        # Ensure the borders match the requested width if they don't already
        new_border = "/" + "*" * (width - 2) + "/"

        return f"{new_border}\n{new_middle}\n{new_border}"

    return re.sub(pattern, replace_header, text, flags=re.MULTILINE)


def main(filenames):
    for filename in filenames:
        with open(filename, "r", encoding="utf-8") as f:
            original = f.read()

        fixed = fix_block_headers(original)

        if original != fixed:
            with open(filename, "w", encoding="utf-8") as f:
                f.write(fixed)
            print(f"Fixed GDAL headers in: {filename}")


if __name__ == "__main__":
    main(sys.argv[1:])
