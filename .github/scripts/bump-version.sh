#!/bin/bash

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

set -e

# Creates a new commit with the patch version being the current date, if there
# are changes since the last version change.
function main() {
  readonly repo="$(realpath "${0%/*}"/../..)"
  cd "${repo}"

  readonly old_version="$(cat VERSION)"
  if [[ "${old_version}" != *.*.???????? ]]; then
    echo >&2 "Unexpected version string: ${old_version}"
    return 1
  fi

  readonly old_commit="$(git log --format=%H -1 -- VERSION)"
  if git diff --quiet "${old_commit}" -- . :^docs/ :^tests/ :^README.md; then
    echo >&2 "No change since commit ${old_commit}"
    return 0
  fi

  readonly old_version_patch="${old_version##*.}"
  readonly new_version_patch="$(date +%Y%m%d)"
  if ((new_version_patch <= old_version_patch)); then
    echo >&2 "Unexpected date: ${new_version_patch} <= ${old_version_patch}"
    return 1
  fi

  readonly old_version_major_minor="${old_version%${old_version_patch}}"
  readonly new_version="${old_version_major_minor}${new_version_patch}"
  echo "${new_version}" >VERSION
  git add -- VERSION
  git commit --no-verify --message "build: bump version to ${new_version}"
}

main "$@"
