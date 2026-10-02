# Shared list of paths whose changes are "release-relevant", i.e. they can
# change baldr's behavior or how it's built, and so warrant a version bump
# and count towards "is there something new to release?".

RELEASE_RELEVANT_PATHS=(
    'baldr'
    'cmake'
    'deps'
    'libutl'
    'CMakeLists.txt'
    'conanfile.txt'
    'dependencies.cmake'
    'settings.cmake'
)
