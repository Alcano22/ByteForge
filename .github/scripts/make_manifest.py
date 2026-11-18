#!/usr/bin/env python3
"""Writes the manifest of a ByteForge release: its version and, for every editor package, the platform, the file
name, the size and the SHA-256. The hub reads it to know what it can download and to check what it downloaded.

Usage: make_manifest.py <version> <output file> <editor packages...>
"""
import hashlib
import json
import sys
from pathlib import Path

PREFIX = "ByteForge-Editor-"
EXTENSIONS = (".zip", ".tar.gz")


def sha256_of(path):
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def platform_of(name, version):
    expected = f"{PREFIX}{version}-"
    if not name.startswith(expected):
        sys.exit(f"'{name}' is not an editor package of version {version}")

    for extension in EXTENSIONS:
        if name.endswith(extension):
            return name[len(expected):-len(extension)]

    sys.exit(f"'{name}' has none of the extensions {', '.join(EXTENSIONS)}")


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)

    version, output, *files = sys.argv[1:]

    packages = []
    for file in files:
        path = Path(file)
        packages.append({
            "platform": platform_of(path.name, version),
            "file": path.name,
            "size": path.stat().st_size,
            "sha256": sha256_of(path),
        })

    packages.sort(key=lambda package: package["platform"])

    manifest = {"format": 1, "version": version, "editor": packages}
    Path(output).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
