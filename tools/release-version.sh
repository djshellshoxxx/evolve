#!/usr/bin/env bash
set -euo pipefail
version=$(sed -n 's/^project(MUTAGEN VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
suffix=${RELEASE_SUFFIX:-}
requested=false
if [[ ${GITHUB_REF:-} == refs/tags/v* ]]; then
    version=${GITHUB_REF#refs/tags/v}
    suffix=""
    requested=true
elif [[ -z $suffix && ${GITHUB_EVENT_NAME:-} == push && ${GITHUB_REF:-} == refs/heads/main ]]; then
    message=$(git show -s --format=%s "${GITHUB_SHA:-HEAD}")
    if [[ $message =~ \[release:([[:alnum:].-]+)\] ]]; then
        suffix=${BASH_REMATCH[1]}
        requested=true
    elif [[ $message == *'[release:'* ]]; then
        echo 'Invalid release marker; use [release:beta.4], for example.' >&2
        exit 1
    fi
fi
if [[ -n $suffix ]]; then
    if [[ ! $suffix =~ ^[[:alnum:]]+([.-][[:alnum:]]+)*$ ]]; then
        echo 'Invalid pre-release suffix.' >&2
        exit 1
    fi
    version=$version-$suffix
fi
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[[:alnum:]]+([.-][[:alnum:]]+)*)?$ ]]; then
    echo 'Invalid release version.' >&2
    exit 1
fi
if [[ ${GITHUB_EVENT_NAME:-} == workflow_dispatch && ${RELEASE_PUBLISH:-} == true ]]; then requested=true; fi
case ${1:-} in
    '') printf '%s\n' "$version" ;;
    --requested) printf '%s\n' "$requested" ;;
    *) echo 'Unknown release-version option.' >&2; exit 1 ;;
esac
