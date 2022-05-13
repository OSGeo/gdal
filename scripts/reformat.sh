#!/bin/sh
set -eu

if ! (cmake-format --version >/dev/null); then
  echo "cmake-format not available. Install it with 'pip install cmake-format'"
  exit 1
fi

# GNU prefix command for mac os support (gsed)
GP=
# shellcheck disable=SC2039
case "${OSTYPE:-}" in
  darwin*)
    GP=g
    ;;
esac

# determine changed files
MODIFIED=$(git status --porcelain| ${GP}sed -ne "s/^ *[MA]  *//p" | sort -u)

if [ -z "$MODIFIED" ]; then
  echo "nothing was modified"
  exit 0
fi

FORMAT_FIX_DIFF=format_fix.diff
rm -f "$FORMAT_FIX_DIFF"
touch "$FORMAT_FIX_DIFF"

for f in $MODIFIED; do

  case "$f" in

    *CMakeLists.txt)
      ;;

    *gdal.cmake)
      ;;

    *cmake/helpers/CheckDependentLibraries.cmake)
      ;;

    *)
      continue
      ;;
  esac

  # Disable cmake-format (https://github.com/OSGeo/gdal/pull/5326#issuecomment-1042617407)
  # m=$f.prepare
  # cp "$f" "$m"
  # cmake-format -i "$f"
  # diff -u "$m" "$f" >> "$FORMAT_FIX_DIFF" || /bin/true
  # rm -f "$m"
done

ret_code=0
if [ -s "$FORMAT_FIX_DIFF" ]; then
  ret_code=1
  # review changes
  if tty -s; then
    if ! (colordiff --version >/dev/null); then
      cat "$FORMAT_FIX_DIFF"
    else
      colordiff < "$FORMAT_FIX_DIFF" | less -r
    fi
  else
    cat "$FORMAT_FIX_DIFF"
  fi
fi
rm -f "$FORMAT_FIX_DIFF"
exit $ret_code
