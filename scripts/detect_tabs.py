import sys


def main():
    files = sys.argv[1:]

    errors = False

    for path in files:
        with open(path, "r", encoding="utf-8") as file:
            try:
                for i, line in enumerate(file):
                    if "\t" in line:
                        errors = True
                        sys.stderr.write(f"{path}:{i + 1}: {line}")
            except UnicodeDecodeError as e:
                errors = True
                sys.stderr.write(str(e))

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
