# Uploading an executable

Indigo accepts the wiiload version 5 protocol over the network or a USB
Gecko. A transfer contains one zlib stream and optional NUL-terminated
arguments. One transfer runs at a time; the menus continue while it arrives.

The decoded image is at most 16 MiB. Its stream must finish with exactly
the input and output sizes the header advertises, with no trailing bytes.
A network read with no data for ten seconds ends the transfer. The USB
Gecko keeps its existing receive timeout. An incomplete image or arguments
are discarded, leaving the menus running.

Before launching a DOL or ELF, Indigo checks that the header, executable
tables, section/segment payloads and the entrypoint's argument probe fit
inside the received image. Existing raw BIN routes still use their explicit
load addresses. The loader itself stays Swiss's.

`buildtools/ui/tests/test_wiiload.py` runs the real receive and handoff
functions with counted socket and scheduler stand-ins. It covers EOF,
truncated checksums, output mismatch, trailing bytes, one-byte reads, image
bounds, arguments and suspension while the menu would own the video mutex.
`--sanitize` runs the same cases under AddressSanitizer and UBSan. The
`wiiload` target in `buildtools/ui/tests/fuzz/run_fuzz.sh` fuzzes the actual
decoder and image checks, comparing accepted output with `uncompress2`.
