#!/usr/bin/env bash
# Tells the website about a release the workflow has just published, so its
# download page gets rows without anybody making them by hand. The website writes
# the notes and holds the release for approval, so this publishes nothing itself.
set -euo pipefail
cd "$(dirname "$0")/../.."

sub_path="synergy/api/releases"
jira_base="https://symless.atlassian.net"
jira_project="S1"

# cmake names the build and the tag is a separate act that can disagree with it.
# A tag that does would point the website at an archive folder nobody wrote.
version="${SYNERGY_VERSION:-}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]]; then
	echo "a version shaped like 1.2.3 is required, got: ${version:-<empty>}" >&2
	exit 1
fi

tag="${SYNERGY_RELEASE_TAG:-}"
if [[ "$tag" != "v$version" ]]; then
	echo "the tag being released is $tag but the build calls itself $version, so they name different releases" >&2
	exit 1
fi

if [[ -z "${WEBSITE_API_TOKEN:-}" ]]; then
	echo "a website API token is required" >&2
	exit 1
fi

# Which editions this repository announces the build under, as one website file
# code per line. It is a repository variable because it is the whole difference
# between a release from here and one from synergy-ee, which builds this same
# tree as Enterprise. A fork that has not set it must fail rather than inherit
# another product's file codes and announce itself as that product.
declare -a file_codes=()

IFS=$',\n' read -r -d '' -a requested_codes <<<"${SYNERGY_FILE_CODES:-}" || true
for code in "${requested_codes[@]}"; do
	code="${code//[[:space:]]/}"
	if [[ -n "$code" ]]; then
		file_codes+=("$code")
	fi
done

if [[ ${#file_codes[@]} -eq 0 ]]; then
	echo "the file codes to announce this release under are required, as SYNERGY_FILE_CODES with one website file code per line (i.e. synergy-personal-v1 and synergy-business-v1)" >&2
	exit 1
fi

# A release names the files the build produced, and a release that names none is
# filled in from the website's release template instead: rows for whatever that
# template still lists, including installers this build never produced, each one
# a download that answers 404. There is no case where sending nothing is better
# than stopping, so the packages are required rather than optional.
package_dir="${SYNERGY_PACKAGE_DIR:-}"
if [[ ! -d "$package_dir" ]]; then
	echo "the directory holding the packages this release is for is required, as SYNERGY_PACKAGE_DIR, got: ${package_dir:-<empty>}" >&2
	exit 1
fi

if ! git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
	echo "the tag being released is not in this checkout, which needs the whole history and its tags" >&2
	exit 1
fi

previous="$(git describe --tags --abbrev=0 --match 'v*' "$tag^" 2>/dev/null || true)"
range="$tag"
if [[ -n "$previous" ]]; then
	range="$previous..$tag"
fi

changes="[]"
source="the issues fixed in $version"
asked_jira=false

if [[ -n "${JIRA_API_TOKEN:-}" && -n "${JIRA_USER_EMAIL:-}" ]]; then
	asked_jira=true
	jira_body="$(jq -n --arg jql "project = $jira_project AND fixVersion = \"$version\"" \
		'{jql: $jql, fields: ["summary", "description"], maxResults: 200}')"

	jira_response="$(curl -sS -X POST "$jira_base/rest/api/3/search/jql" \
		-u "$JIRA_USER_EMAIL:$JIRA_API_TOKEN" \
		-H "Content-Type: application/json" \
		--data-binary "$jira_body" \
		--max-time 60 \
		-w $'\n%{http_code}')"
	jira_status="${jira_response##*$'\n'}"
	jira_json="${jira_response%$'\n'*}"

	# A failed read is not a reason to fall back: it would quietly write worse
	# notes with nothing to say why.
	if [[ "$jira_status" != "200" ]]; then
		echo "could not read the issues fixed in $version (HTTP $jira_status): $jira_json" >&2
		exit 1
	fi

	# A description is rich text, so take every run of text in it in order.
	changes="$(jq '[.issues[]
		| .fields.summary as $summary
		| ([.fields.description? // {} | .. | objects | select(.type == "text") | .text] | join(" ")) as $body
		| if ($body | length) > 0 then "\($summary)\n\n\($body)" else $summary end]' <<<"$jira_json")"
fi

count="$(jq 'length' <<<"$changes")"

if [[ "$count" -eq 0 ]]; then
	if [[ "$asked_jira" == true ]]; then
		echo "no issues carry the fix version $version, using the commit subjects instead" >&2
	else
		echo "no credentials to read the issues fixed in $version, using the commit subjects instead" >&2
	fi
	source="the commits since ${previous:-the start of history}"

	# A squashed pull request carries its number in the subject, which says
	# nothing to anyone reading the notes it becomes.
	changes="$(git log --no-merges --format=%s "$range" |
		sed -E 's/ \(#[0-9]+\)$//' |
		jq -R -s 'split("\n") | map(select(length > 0))')"
	count="$(jq 'length' <<<"$changes")"
fi

if [[ "$count" -eq 0 ]]; then
	echo "nothing was fixed or committed for $version, so there is nothing to write notes about" >&2
	exit 1
fi

# One rpm under two names: the editions sold to companies list it against Red Hat
# and Personal lists it against the free rebuilds, so the edition in the file code
# is what picks the row. The package is named after neither, because it is neither.
el_row_for() {
	local major="$1" file_code="$2"
	case "$file_code" in
	*business* | *enterprise*) echo "rhel-$major X64" ;;
	*) echo "rocky-$major X64" ;;
	esac
}

# Two entries below read as typos and are not: a catalog slug keeps the dot the
# file name replaces (ubuntu-24.04 against ubuntu-24-04), and the Ubuntu 22.04
# arm64 deb is also what fills Raspberry Pi OS.
package_rows_for() {
	case "$1" in
	*_windows_x64.msi) echo "windows-10 X64" ;;
	*_windows_arm64.msi) echo "windows-10 Arm64" ;;
	*_mac_x64.dmg) echo "macos-12.0 X64" ;;
	*_mac_arm64.dmg) echo "macos-12.0 Arm64" ;;
	*_debian-12_x86_64.deb) echo "debian-12 X64" ;;
	*_debian-12_aarch64.deb) echo "debian-12 Arm64" ;;
	*_debian-13_x86_64.deb) echo "debian-13 X64" ;;
	*_debian-13_aarch64.deb) echo "debian-13 Arm64" ;;
	*_ubuntu-22-04_x86_64.deb) echo "ubuntu-22.04 X64" ;;
	*_ubuntu-22-04_aarch64.deb) printf '%s\n' "ubuntu-22.04 Arm64" "raspberry-pi-os Arm64" ;;
	*_ubuntu-24-04_x86_64.deb) echo "ubuntu-24.04 X64" ;;
	*_ubuntu-24-04_aarch64.deb) echo "ubuntu-24.04 Arm64" ;;
	*_ubuntu-26-04_x86_64.deb) echo "ubuntu-26.04 X64" ;;
	*_ubuntu-26-04_aarch64.deb) echo "ubuntu-26.04 Arm64" ;;
	*_fedora-42_x86_64.rpm) echo "fedora-42 X64" ;;
	*_fedora-42_aarch64.rpm) echo "fedora-42 Arm64" ;;
	*_fedora-43_x86_64.rpm) echo "fedora-43 X64" ;;
	*_fedora-43_aarch64.rpm) echo "fedora-43 Arm64" ;;
	*_fedora-44_x86_64.rpm) echo "fedora-44 X64" ;;
	*_fedora-44_aarch64.rpm) echo "fedora-44 Arm64" ;;
	*_el-8_x86_64.rpm) el_row_for 8 "$2" ;;
	*_el-9_x86_64.rpm) el_row_for 9 "$2" ;;
	*_opensuse-tumbleweed_x86_64.rpm) echo "opensuse-tumbleweed X64" ;;
	*_opensuse-tumbleweed_aarch64.rpm) echo "opensuse-tumbleweed Arm64" ;;
	*_arch-linux_x86_64.pkg.tar.zst) echo "arch-linux X64" ;;
	*_manjaro_x86_64.pkg.tar.zst) echo "manjaro X64" ;;
	*_linux_x86_64.flatpak) echo "flatpak X64" ;;
	*_linux_aarch64.flatpak) echo "flatpak Arm64" ;;
	*) return 1 ;;
	esac
}

package_list() {
	local dir="$1" file_code="$2" out="[]" name rows row os arch
	while IFS= read -r name; do
		# The download page has no button for the portable archive, and listing it
		# against the msi's row would hide one of the two behind the other.
		case "$name" in
		*-portable.7z) continue ;;
		esac
		if ! rows="$(package_rows_for "$name" "$file_code")"; then
			echo "built a package this script cannot name an operating system for: $name" >&2
			exit 1
		fi
		while IFS= read -r row; do
			os="${row%% *}"
			arch="${row##* }"
			out="$(jq --arg fileName "$name" --arg os "$os" --arg arch "$arch" \
				'. + [{fileName: $fileName, os: $os, arch: $arch}]' <<<"$out")"
		done <<<"$rows"
	done < <(find "$dir" -maxdepth 1 -type f -printf '%f\n' | sort)
	echo "$out"
}

base="${SYNERGY_WEBSITE_URL_BASE:-https://symless.com}"
url="${base%/}/$sub_path"

echo "Telling the website about $version, built from $count changes taken from $source"

for file_code in "${file_codes[@]}"; do
	# Read per edition, because the same rpm is filed under a different row for
	# each.
	packages="$(package_list "$package_dir" "$file_code")"
	package_count="$(jq 'length' <<<"$packages")"

	if [[ "$package_count" -eq 0 ]]; then
		echo "the build produced no packages for $file_code, so there is nothing to release" >&2
		exit 1
	fi

	echo "Naming the $package_count packages the build produced for $file_code"

	payload="$(jq -n --arg fileCode "$file_code" --arg version "$version" \
		--argjson changes "$changes" --argjson packages "$packages" \
		'{fileCode: $fileCode, version: $version, changes: $changes, packages: $packages}')"

	response="$(curl -sS -X POST "$url" \
		-H "Authorization: Bearer $WEBSITE_API_TOKEN" \
		-H "Content-Type: application/json" \
		--data-binary "$payload" \
		--max-time 300 \
		-w $'\n%{http_code}')"
	status="${response##*$'\n'}"
	body="${response%$'\n'*}"

	case "$status" in
	200) echo "The website is holding $version of $file_code for review: $body" ;;
	# This can create a release per file code, so a run that got one in before
	# failing has to be safe to repeat; re-running the job is the recovery.
	409) echo "$file_code already has a release for $version, so it was left alone: $body" >&2 ;;
	*)
		echo "the website refused $file_code $version (HTTP $status): $body" >&2
		exit 1
		;;
	esac
done
