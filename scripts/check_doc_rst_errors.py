import re
import sys

directive_missing_space = re.compile("[ ^][.][.][a-z]+::?")
example_misplaced_title = re.compile("[.][.] example::.*[A-Za-z]")


def main():
    files = sys.argv[1:]

    errors = False

    def log_error(file, line, desc):
        nonlocal errors
        sys.stderr.write(f"{file}:{line}: {desc}\n")
        errors = True

    for path in files:
        contents = open(path, "r", encoding="utf-8").read()

        for i, line in enumerate(contents.split("\n")):
            if "\t" in line:
                log_error(path, i + 1, "tab character detected")
            if "\r" in line and sys.platform() != "Windows":
                log_error(path, i + 1, "Windows linebreak detected")
            match = re.search(directive_missing_space, line)
            if match:
                log_error(path, i + 1, f"Detected malformed directive {match.group()}")
            match = re.search(example_misplaced_title, line)
            if match:
                log_error(path, i + 1, "Example title should be specified with :title:")

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
