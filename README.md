# Libnfc ST SRx reading/writing tool

Tool to read/write ST SRx tags. Only tested with SRIX4K tags, your mileage with SRI512 tags may vary.

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
usage: ./nfc_st_srx [-h] [-v] [-w | -d] [-t x4k|512] [-f FILE]

Options:
  -h         Show this help message
  -v         Verbose - print transceived messages
  -w         Write dump instead of reading
  -d         Dry run - check for potential irreversible changes instead of writing
  -f FILE    Dump (write) memory content to (from) FILE
  -f -       Dump (write) memory content to stdout (from stdin) (default)
  -t x4k|512 Select SRIX4K or SRI512 tag type. Default is SRIX4K
```

## Note on writing tags

Compliant ST SRx tags have some blocks that, once changed, cannot be changed back to their original value.
Before writing a tag, make sure you're aware of this.

I implemented a "dry run" check feature which reads the tag and compares it with the dump to see if writing the dump
would cause irreversible changes.

However, I would advise you not to trust it as it's not been heavily tested.