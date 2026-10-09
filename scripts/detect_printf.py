import sys


def main():
    search_target = sys.argv[1]
    files = sys.argv[2:]

    errors = False

    for path in files:
        with open(path, "r", encoding="utf-8") as file:
            for i, line in enumerate(file):
                if search_target not in line:
                    continue

                if "/* ok */" in line or "/*ok*/" in line or "// ok" in line:
                    continue

                line = line.strip()

                if (
                    line.startswith("//")
                    or line.startswith("/*")
                    or line.startswith("*")
                    or line.startswith("@")
                ):
                    continue

                errors = True
                sys.stderr.write(f"{path}:{i + 1}: {line}\n")

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
