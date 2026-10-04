#!/usr/bin/env sh
set -eu

if [ "$#" -ne 0 ]; then
	printf 'Usage: %s\n' "$0" >&2
	exit 2
fi

script_dir=$(
	unset CDPATH
	cd -- "$(dirname -- "$0")" && pwd
)

# Compare numeric releases and release candidates without requiring GNU awk.
version_tag_is_greater() {
	awk -v left="$1" -v right="$2" '
    function parse(tag, out, fields, cnt) {
      cnt = split(tag, fields, /[.-]/)
      out[1] = fields[1] + 0
      out[2] = fields[2] + 0
      out[3] = fields[3] + 0
      out[4] = cnt == 3 ? -1 : substr(fields[4], 3) + 0
    }
    BEGIN {
      parse(left, l)
      parse(right, r)
      for (i = 1; i <= 3; ++i) {
        if (l[i] != r[i]) {
          exit (l[i] > r[i]) ? 0 : 1
        }
      }
      if (l[4] == -1 && r[4] != -1) {
        exit 0
      }
      if (l[4] != -1 && r[4] == -1) {
        exit 1
      }
      exit (l[4] > r[4]) ? 0 : 1
    }
  '
}

# Keep vDS's nearest merged tag, commit distance, short hash, and dirty suffix.
git_version() {
	if ! command -v git >/dev/null 2>&1 ||
		! git -C "$script_dir" rev-parse --verify HEAD >/dev/null 2>&1; then
		printf '%s\n' unknown
		return
	fi

	dirty=""
	if ! git -C "$script_dir" diff-index --quiet HEAD -- 2>/dev/null; then
		dirty="+"
	fi

	best_tag=""
	best_distance=""
	for tag in $(git -C "$script_dir" tag --merged HEAD --list); do
		if ! printf '%s' "$tag" |
			grep -Eq '^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*(-rc[0-9][0-9]*)?$'; then
			continue
		fi
		commit=$(git -C "$script_dir" rev-list -n 1 "refs/tags/$tag" 2>/dev/null) ||
			continue
		distance=$(git -C "$script_dir" rev-list --count "$commit..HEAD" 2>/dev/null) ||
			continue
		if [ -z "$best_tag" ] ||
			[ "$distance" -lt "$best_distance" ] ||
			{ [ "$distance" -eq "$best_distance" ] &&
				version_tag_is_greater "$tag" "$best_tag"; }; then
			best_tag=$tag
			best_distance=$distance
		fi
	done

	if [ -z "$best_tag" ]; then
		printf '%s\n' unknown
	elif [ "$best_distance" -eq 0 ]; then
		printf '%s%s\n' "$best_tag" "$dirty"
	else
		sha=$(git -C "$script_dir" rev-parse --short=7 HEAD)
		printf '%s-%s-g%s%s\n' "$best_tag" "$best_distance" "$sha" "$dirty"
	fi
}

git_version
