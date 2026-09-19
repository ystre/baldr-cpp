# Baldr

Convenience tool for building and running C++ code.

Example commands:

```sh
baldr build
baldr run -t <target>
```

## Project Structure

```
├── baldr                   # Baldr CLI tool
├── cmake                   # Internal CMake modules
├── doc
├── libutl                  # Utility library (will be moved out for reuse)
├── tests                   # Functional tests
└── tools
```
