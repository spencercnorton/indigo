#!/bin/sh
# Install (or update) the runner supervisor on this machine as a systemd user
# service. Run it as the user that owns the runners; that user needs Docker.
# See README.md for what it sets up and how to give it GH_TOKEN.
set -eu

repo=${INDIGO_CI_REPO:-spencercnorton/indigo}
ref=${INDIGO_CI_REF:-beta}
home=${INDIGO_CI_HOME:-$HOME/.local/share/indigo-ci}
units=$HOME/.config/systemd/user

for tool in docker git python3 systemctl; do
	command -v "$tool" >/dev/null || { echo "install.sh: $tool is missing" >&2; exit 1; }
done
docker info >/dev/null 2>&1 || { echo "install.sh: this user cannot reach Docker" >&2; exit 1; }

mkdir -p "$home" "$units"
[ -d "$home/src/.git" ] || git clone --quiet --no-checkout "https://github.com/$repo.git" "$home/src"
git -C "$home/src" fetch --quiet --prune origin "+refs/heads/$ref:refs/remotes/origin/$ref"
git -C "$home/src" checkout --quiet --force --detach "origin/$ref"
install -m 0644 "$home/src/buildtools/ci/runner/indigo-ci.service" "$units/indigo-ci.service"

env_file=$HOME/.config/indigo-ci.env
if [ ! -f "$env_file" ]; then
	umask 077
	printf 'INDIGO_CI_REPO=%s\nINDIGO_CI_REF=%s\nINDIGO_CI_POOLS=build:3,emulator:1\n' \
		"$repo" "$ref" > "$env_file"
	echo "wrote $env_file; add GH_TOKEN there or give the unit a drop-in that supplies it"
fi

# Without linger the user manager, and this service, stop at logout.
if [ "$(loginctl show-user "$USER" -p Linger --value 2>/dev/null)" != yes ]; then
	loginctl enable-linger "$USER" 2>/dev/null ||
		echo "install.sh: run 'sudo loginctl enable-linger $USER' or the runners stop at logout" >&2
fi

systemctl --user daemon-reload
systemctl --user enable indigo-ci.service
systemctl --user restart indigo-ci.service
systemctl --user --no-pager status indigo-ci.service | head -5
