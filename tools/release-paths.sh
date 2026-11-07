# Shared by `ci/check-version-bump.sh` and `ci/tag-release.sh`.

# Paths whose changes are "release-relevant", i.e. they can change baldr's
# behavior or how it's built, and so warrant a version bump and count towards
# "is there something new to release?".
RELEASE_RELEVANT_PATHS=(
    'baldr'
    'cmake'
    'deps'
    'libutl'
    'res'
    'CMakeLists.txt'
    'conanfile.txt'
    'dependencies.cmake'
    'settings.cmake'
)

# Version of baldr at the given ref; the only place that knows where the
# version lives (`version.txt`, `VERSION` before the rename).
version_at() {
    local file
    for file in version.txt VERSION; do
        if git -C "${WORKSPACE}" cat-file -e "$1:${file}" 2>/dev/null; then
            git -C "${WORKSPACE}" show "$1:${file}" | tr -d '[:space:]'
            return 0
        fi
    done
    return 1
}
