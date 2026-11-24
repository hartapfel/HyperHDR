"""Replace the engine in the pinned LG webOS loader, preserving its IPK format."""

import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile


def update_tar(compressed, update):
    raw = gzip.decompress(compressed)
    result = io.BytesIO()
    cursor = 0
    with tarfile.open(fileobj=io.BytesIO(raw), mode="r:") as archive:
        for entry in archive:
            if not entry.isfile():
                continue
            start = entry.offset_data
            content = update(entry.name, raw[start:start + entry.size])
            if content is None:
                continue
            # Keep the stock USTAR headers, modes, ordering and path prefixes.
            result.write(raw[cursor:start - 512])
            header = bytearray(raw[start - 512:start])
            header[124:136] = f"{len(content):010o} ".encode() + b"\0"
            header[148:156] = b" " * 8
            header[148:156] = f"{sum(header):06o} ".encode() + b"\0"
            result.write(header)
            result.write(content)
            result.write(bytes(-len(content) % 512))
            cursor = start + entry.size + (-entry.size % 512)
    result.write(raw[cursor:])
    return gzip.compress(result.getvalue(), compresslevel=6, mtime=0)


def package(stock, engine, version, output):
    raw = stock.read_bytes()
    if hashlib.sha256(raw).hexdigest() != "1341e25c1ebb6eeea172485dc885acb44c93d79bc5ac5ce698ed721664671f08":
        raise ValueError("Expected the webosbrew v22.0.1 release IPK")
    binary = engine.read_bytes()
    if binary[:6] != b"\x7fELF\x01\x01" or binary[18:20] != b"\x28\x00" or b"zoneBrightnessBalance" not in binary:
        raise ValueError("Expected a 32-bit little-endian ARM Hue test executable")

    # Older webOS installers expect LG ares-package's space-padded AR names.
    members = {}
    pos = 8
    while pos < len(raw):
        header = raw[pos:pos + 60]
        length = int(header[48:58])
        members[header[:16].strip()] = (header, raw[pos + 60:pos + 60 + length])
        pos += 60 + length + length % 2

    size_delta = 0
    def update_data(name, old):
        nonlocal size_delta
        if name.endswith("/hyperhdr/hyperhdr"):
            new = binary
        elif name.endswith(("/appinfo.json", "/packageinfo.json")):
            meta = json.loads(old)
            meta["version"] = version
            new = (json.dumps(meta, indent=2) + "\n").encode()
        else:
            return None
        size_delta += len(new) - len(old)
        return new

    def update_control(name, old):
        if name != "control":
            return None
        control = re.sub(rb"(?m)^Version: .*?$", b"Version: " + version.encode(), old)
        return re.sub(rb"(?m)^Installed-Size: (\d+)$",
                      lambda m: f"Installed-Size: {int(m[1]) + size_delta}".encode(), control)

    data = update_tar(members[b"data.tar.gz"][1], update_data)
    control = update_tar(members[b"control.tar.gz"][1], update_control)
    with output.open("wb") as file:
        file.write(raw[:8])
        for name, payload in [(b"debian-binary", b"2.0\n"), (b"control.tar.gz", control), (b"data.tar.gz", data)]:
            header = members[name][0]
            file.write(header[:48] + f"{len(payload):<10}".encode() + header[58:])
            file.write(payload)
            if len(payload) % 2:
                file.write(b"\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path)
    parser.add_argument("engine", type=Path)
    parser.add_argument("version")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    package(args.stock, args.engine, args.version, args.output)
