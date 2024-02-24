# remap

## Description

`remap` allows you to remap the colors of an image to a limited set of colors specified by your palette. Supported palette formats are Act, Microsoft Pal, JASC, GIMP, Paint .NET and png.

## Installation

```bash
git clone https://github.com/bernhardfritz/remap.git
cd remap
mkdir build
cd build
cmake ..
make
make install
```

## Usage

```
remap <inputFilename> <paletteFilename> <outputFilename>
```

## Example

```bash
remap dungeon.png endesga-32-1x.png output.png
```

![dungeon.png](dungeon.png)

![endesga-32-1x.png](endesga-32-32x.png)

![output.png](output.png)

## References

* https://github.com/markusn/color-diff
