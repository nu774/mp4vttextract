# mp4vttextract

Extract WebVTT track embedded in a MP4 file.

## build

$ make

## usage

```
mp4extract [-t TRACK] INFILE.mp4 [OUTFILE.vtt]
```

You can specify desired track number (>=1) by `-t TRACK`.

Without `-t TRACK`, mp4extract extracts first WebVTT track in the file.
