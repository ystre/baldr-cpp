# Baldr

Convenience tool for building and running C++ code.

Example commands:

```sh
baldr build
baldr run -t <target>
```

For a clean, end-to-end verification, use `ci/e2e.sh`. It copies the content of
the repository in `/tmp/workspace/baldr`, cleans the untracked content, and
packages the application. The result can be found in `/tmp/workspace/baldr/package`.

See `doc` directory for detailed documentation.

## Project Structure

```
├── baldr                   # Baldr CLI tool
├── cmake                   # Internal CMake modules
├── ci                      # CI-like workflow
├── doc
├── libutl                  # Utility library (will be moved out for reuse)
├── tests                   # Functional tests
└── tools
```
