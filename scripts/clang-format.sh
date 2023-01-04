#!/bin/sh
set -eu

if ! (clang-format --version >/dev/null); then
  echo "clang-format not available. Install it with 'pre-commit'"
  exit 1
fi


# determine changed files
FILES=$(git diff --diff-filter=AM --name-only HEAD~1| tr '\n' ' ' | sort -u)

if [ -z "$FILES" ]; then
  echo "nothing was modified"
  exit 0
fi

STYLEDIFF=/tmp/clang-format.diff
true > $STYLEDIFF

for f in $FILES; do
	if ! [ -f "$f" ]; then
		echo "$f was removed." >>/tmp/ctest-important.log
		continue
	fi

	# echo "Checking $f"
	case "$f" in

	*frmts/grib/degrib/*)
	  continue
	  ;;

	*.cpp|*.c|*.h|*.cxx|*.hxx|*.c++|*.h++|*.cc|*.hh|*.C|*.H)
		;;

	*)
		continue
		;;
	esac

	m="$f.prepare"
	cp "$f" "$m"
	clang-format -i "$f"
	if diff -u "$m" "$f" >>$STYLEDIFF; then
		rm "$m"
	else
		echo "File $f is not styled properly."
	fi
done

if [ -s "$STYLEDIFF" ]; then
	echo
	echo "Required code formatting updates:"
	cat "$STYLEDIFF"

	cat <<EOF
Tips to prevent and resolve:
* Run pre-commit to install clang-format for C++ code style
EOF
	exit 1
fi
