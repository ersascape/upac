# upac

`upac` is a small C++ utility/library for reading Unisoc/Spreadtrum `.pac` firmware packages.

It can:
- inspect PAC headers and file tables
- extract embedded files
- dump the embedded XML flash scheme
- expose reusable PAC parsing logic to other tools like `uflash`

## Build

```bash
cmake -S . -B build
cmake --build build
```

The CLI binary is produced as:

```bash
./build/upac
```

## Usage

```bash
./build/upac info firmware.pac
./build/upac list firmware.pac
./build/upac extract firmware.pac extracted/
./build/upac xml firmware.pac
./build/upac ops firmware.pac
./build/upac verify firmware.pac
```

## Library

The reusable library target is `upac_lib` with alias `upac::upac`.

Other CMake projects in this source tree can consume it with:

```cmake
add_subdirectory(../upac upac-build)
target_link_libraries(my_tool PRIVATE upac::upac)
```

## Notes

- PAC metadata strings are UTF-16LE on disk and are converted to UTF-8 in memory.
- The reader caches decoded file metadata and XML config so repeated operations are faster.
- Extraction is streamed in chunks, so large images do not need to be fully loaded into memory first.
