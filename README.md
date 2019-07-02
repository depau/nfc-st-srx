# Libnfc ST SRx reading tool

Tool to read ST SRx tags

## Building

Requires `libnfc` (discovered with pkg-config) and CMake.

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```txt
usage: ./nfc_st_srx [-h] [-v] -o FILE [-t x4k|512]

Options:
  -h         Show this help message
  -v         Verbose - print transceived messages
  -o FILE    Dump memory content to FILE
  -o -       Dump memory content to stdout
  -t x4k|512 Select SRIX4K or SRI512 tag type. Default is SRIX4K
```