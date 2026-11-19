import re
import sys

sys_header = r"^#include <(\w+)>"

symbols = {
    "std::min": "algorithm",
    "std::max": "algorithm",
    "std::array": "array",
    "std::numeric_limits": "limits",
    "std::isalpha": "cctype",
    "std::isnan": "cmath",
    "std::isinf": "cmath",
    "std::isfinite": "cmath",
}


def main():
    files = sys.argv[1:]

    errors = False

    for path in files:
        includes = set()
        missing_includes = set()

        with open(path, "r", encoding="utf-8") as file:
            for line in file:
                match = re.search(sys_header, line)
                if match:
                    includes.add(match.group(1))

                for text, header in symbols.items():
                    if text in line and header not in includes:
                        missing_includes.add(header)

        if missing_includes:
            errors = True
            sys.stderr.write(
                f"{path}: missing includes: {', '.join(missing_includes)}\n"
            )

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
