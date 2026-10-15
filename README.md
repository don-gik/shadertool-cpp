# shadertool-cpp

A small C++ library for compiling Slang shaders to SPIR-V and extracting reflection data.

## Requirements

- CMake ≥ 3.20
- C++17
- Linux, Windows, or macOS (x86_64 / aarch64)

## Build

```bash
cmake -S . -B build
cmake --build build
```

## API

The library exposes a C-compatible interface. The main entry points are:

- `shadertool_create_context` — initialize
- `shadertool_destroy_context` — cleanup
- `shadertool_compile_shader` — compile a `.slang` module to SPIR-V
- `shadertool_free_compile_result` — free returned resources

## License

Licensed under the [GNU AGPL v3](LICENSE).
