# rle
run length encoding

count repeated pixels

compressbitmap() in lsrle.h encodes a bitmap into RLE format

uncompressbitmap() in lsrle.h decodes a compressed RLE image back into a bitmap

uncompressbitmap() currently checks to make sure the decoded result is the same as the original image

testrle.html aso decodes a compressed RLE image, in Javascript, and draws it to a HTML5 canvas

With palette mode vs without: better file size (60KB vs 100KB), similar encoding/decoding speed

By default FF00FFnn (magenta) triggers an action

For nn = FF, toggle palette mode, FF FF to add, FF FF FF to enable and add

Add to palette adds the next item index. First index is 0, last is FE

If trigger byte is AA instead of FF, first is 0, last is FF and AA will be empty

Trigger byte is the red byte of the colours being read.

For nn = 1-254, this indicates a repeat. If 254 repeats, an extra byte is expected.

If byte 2 is 0x80 or above (128 or more), an extra byte is expected.

If byte 3 is 0x80 or above (128 or more), an extra byte is expected.

If byte 4 is 0x80 or above (128 or more), an error is triggered. Todo: support any length; massive canvases.

Total repeats of the last pixel to create = 254 + [(byte4 << 14) + [(byte3 << 7)]] + byte2; (without 0x80 flags)

todo: multi-byte palette indexes

todo: FF00FFFF to EE as mentioned in DETAILS.TXT

16/07/2019 tests:

local machine dummy client                1.33fps

local network web client                  1.14fps

local network wireless mobile web client  0.36fps

8MB raw to approx 60KB compressed

The binary may work if you have a Broadcom GPU, Freetype2 and libpng installed

Run ./shell -testrle on the server to start the sender

Open http://./rle/testrle.html to start the receiver

  (click L or double-click to enable/disable streaming mode)
