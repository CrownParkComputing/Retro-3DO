#!/usr/bin/env python3
"""Export the actual displayed orientation of simulator PNGs for App Store."""
import pathlib
import sys
import argparse

from PIL import Image, ImageOps


def export(root, prefixes, include_portrait=True):
    for prefix in prefixes:
        landscape = (2868, 1320) if prefix.startswith("iphone") else (2752, 2064)
        destination = root / "store" / prefix
        destination.mkdir(parents=True, exist_ok=True)
        names = ["01-library", "02-guide", "03-controls", "04-demo"]
        if prefix.startswith("ipad") and include_portrait:
            names += ["05-portrait-controls", "06-portrait-demo"]
        for name in names:
            candidates = [p for p in root.rglob(name + ".png")
                          if p.relative_to(root).parts[0].startswith(prefix)]
            assert len(candidates) == 1, (prefix, name, candidates)
            with Image.open(candidates[0]) as source:
                # XCTest uses portrait pixel storage plus EXIF orientation.
                # Apply that orientation once; never infer it from dimensions.
                image = ImageOps.exif_transpose(source)
                expected = landscape[::-1] if "portrait" in name else landscape
                assert image.size == expected, (candidates[0], image.size, expected)
                image.save(destination / (name + ".png"), format="PNG")
                print(prefix, name, image.size)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=pathlib.Path)
    parser.add_argument("prefixes", nargs="+")
    parser.add_argument("--landscape-only", action="store_true")
    args = parser.parse_args()
    export(args.root, args.prefixes, not args.landscape_only)
