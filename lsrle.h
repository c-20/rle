// requires sched.h for sched_yield();
/*
#define LSPNGSTRINGSIZE		128
#define LSPNGMAXDIGITS		9

#define LSPNGHEADERSIZE		8
#define LSPNGBITDEPTH		8
#define LSPNGWIDEBITDEPTH	16
#define LSPNGRGBCHANNELS	3
//#define LSPNGCOLOURTYPE		PNG_COLOR_TYPE_RGB
#define LSPNGCOLOURTYPE		PNG_COLOR_TYPE_RGBA
#define LSPNGNOINTERLACE	PNG_INTERLACE_NONE
#define LSPNGCOMPRESSTYPE	PNG_COMPRESSION_TYPE_DEFAULT
#define LSPNGCOMPRESSLEVEL	6 // Z_DEFAULT_COMPRESSION
#define LSPNGFILTERTYPE		PNG_FILTER_TYPE_DEFAULT
#define LSPNGVERSION		PNG_LIBPNG_VER_STRING

#define LSENDOFIMAGE            32
#define LSSAMECOUNT		0x40
#define LSDIFFCOUNT		0x00 // diff == !same (check for 'same' flag)	
*/
typedef uint8_t byte;
//typedef struct { UCH r, g, b, a; } px;
typedef struct { byte a, b, g, r; } px;
typedef struct {
  IN width, height;
  px *pixels;
  UCS compressed;
  IN compressedlen;
  FP compressionpct;
} bitmap;

static bitmap *createemptybitmap() {
  bitmap *map = (bitmap *)malloc(sizeof(bitmap));
  map->width = 0;
  map->height = 0;
  map->pixels = NULL;
  map->compressed = COUNTMEM(UCH, 8);
  map->compressedlen = 8;
  map->compressionpct = 0.0;
  UCS mc = map->compressed;
  mc[0] = mc[1] = mc[2] = 0xFF; // white SOF
  mc[4] = mc[5] = mc[6] = 0xFF; // white EOF
  mc[3] = mc[7] = 0x00; // empty! 0 pixels!
  RT map;
}

static bitmap *createbitmap(IN width, IN height) {
  IN numpixels = width * height;
  bitmap *map = (bitmap *)malloc(sizeof(bitmap));
  map->width = width;
  map->height = height;
  map->pixels = (px *)calloc(sizeof(px), numpixels);
//  map->pixelrows = NULL;
  map->compressed = COUNTMEM(UCH, numpixels * 4 + 8);
  map->compressedlen = numpixels * 4 + 8; // updated upon compression
  map->compressionpct = 0.0;
  UCS mc = map->compressed;
  mc[0] = mc[1] = mc[2] = 0xFF; // white SOF
  mc[4] = mc[5] = mc[6] = 0xFF; // white EOF
  mc[3] = mc[7] = 0x00; // empty! 0 pixels!
  RT map;
}

//static bitmap *

VD compressbitmap(bitmap *b) {
  IN pxwidth = b->width;
  IN pxheight = b->height;
  IN numpixels = pxwidth * pxheight;
  px *source = b->pixels;
  px *sourcemax = &source[numpixels];
  UCS result = b->compressed;
  UCS resultmax = &result[(numpixels + 2) * 4]; // 4 bytes per packet + SOF,EOF
  px transparentpixel = { r: 0xFF, g: 0x00, b: 0xFF, a: 0x00 };
//  transparentpixel.r = 0xFF;
//  transparentpixel.g = 0x00;
//  transparentpixel.b = 0xFF;
//  transparentpixel.a = 0x00;
  px *savedpixel = source;
  WI (savedpixel LT sourcemax) {
    px thispixel = *savedpixel; // search for magenta image pixels
    thispixel.a = 0x00; // alpha channel ignored for magenta pixels
    IF (thispixel.r EQ transparentpixel.r AND
        thispixel.g EQ transparentpixel.g AND
        thispixel.b EQ transparentpixel.b) {
//    IF (thispixel EQ transparentpixel) {
      M3("[%d,%d++,%d]", thispixel.r, thispixel.g, thispixel.b);
      IF (transparentpixel.g GQ 255) { // overflow!
        Rs("no unused colours from magenta to white");
        RT;
      } EL { // next colour, start again
        INC transparentpixel.g;
        savedpixel = source;
      }
    } EL { INC savedpixel; }
  } // could pre-create palette here too, maybe prioritise most used
  G3("[%d,%d,%d]", transparentpixel.r, transparentpixel.g, transparentpixel.b);
  *result = transparentpixel.r; INC result;
  *result = transparentpixel.g; INC result;
  *result = transparentpixel.b; INC result;
  *result = 0;                  INC result; // later: indicate how many transparent pixels there are
//Ms("SOI"); // TP w/ .a 0 is SOI marker
  // instruction 0: repeat the last pixel .a times (.a == 0 indicates start of image ^^ or that) <- set transparent pixel
  // transparent pixels should be a range with each value indicating an instruction
  // instruction 1: use palette reference (consider 1 and 2 byte palettes)
  savedpixel = source;
  px lastpixel = transparentpixel; // unused colour, should not match first pixel...but  might
//Y4("(%d,%d,%d,%d)", savedpixel->r, savedpixel->g, savedpixel->b, savedpixel->a);
  px paletteitems[256]; // skip the index of transparentpixel.r
  // later: 65K palette ..................................
  IN palettenumitems = 0;
  IN palettemode = 0;
  IN repeatcount = 0;
  WI (savedpixel LT sourcemax AND result LT resultmax) {
    px thispixel = *savedpixel;
//    IF (repeatcounter) {
    CH writepixel = 1; // yes, unless no
    IF (repeatcount) {
      CH endrepeat = 0;
//      CH lastpixelmatch = 0;
      IF (thispixel.r EQ lastpixel.r AND
          thispixel.g EQ lastpixel.g AND
          thispixel.b EQ lastpixel.b AND
          thispixel.a EQ lastpixel.a    ) {
        INC repeatcount;
        IF (savedpixel + 1 EQ sourcemax) {
//          lastpixelmatch = 1; <-- only needed for writepixel
          endrepeat = 1;  // write repeat count
          writepixel = 0; // don't write after end (this is end)
        } EL { writepixel = 0; } // don't write after if counting
      } EL { endrepeat = 1; writepixel = 1; } // write after end
//        IF (*repeatcounter EQ 254) { // max for first byte
//          IF (repeat2) { // counting in second byte already
//            IF (*repeat2 | 0x80) { // second byte is full
//              IF (repeat3
//            INC (*repeat2);
//          } EL {
//            repeat2 = repeatcounter + 1;
//            *repeat2 = 1; // start at 1 more ... (what would 0 be? use 0 for palette ?)
//          { repeatcounter = NULL; Rs("|"); } // max reached
//        } EL { INC (*repeatcounter); } // keep counting
  //    } EL { // new colour: write repeat bytes
  //    ^^ also need to match on last pixel
//      IF (endrepeat OR lastpixelmatch) {
      IF (endrepeat) { // includes lastpixelmatch !
//        Y1("%d", repeatcount);
        // write full repeat packet
        *result = transparentpixel.r; INC result;
        *result = transparentpixel.g; INC result;
        *result = transparentpixel.b; INC result;
        IF (repeatcount GQ 254) {
          *result = 254; INC result;
//B1("1:%d ", 254);
          repeatcount SUBS 254;
          IF (!repeatcount) {
            *result = 0; INC result; // need to write a 0 for 254 repeats
//Bs("0!");
          } EF (repeatcount) { // still more bytes? GQ 0x80) {
            UCH secondbyte = repeatcount & 0x7F;
            IF (repeatcount >> 7) { secondbyte |= 0x80; }
//B1("2:%d ", secondbyte);
            *result = secondbyte; INC result;
            repeatcount >>= 7;
            IF (repeatcount) { // still more bytes? GQ 0x80) {
              UCH thirdbyte = repeatcount & 0x7F;
              IF (repeatcount >> 7) { thirdbyte |= 0x80; }
//B1("3:%d ", thirdbyte);
              *result = thirdbyte; INC result;
              repeatcount >>= 7;
              IF (repeatcount) { // still more bytes? GQ 0x80) {
                UCH fourthbyte = repeatcount & 0x7F;
                IF (repeatcount >> 7) { fourthbyte |= 0x80; }
//B1("4:%d ", fourthbyte);
                *result = fourthbyte; INC result; // fourth byte
                repeatcount >>= 7;
                IF (repeatcount GT 0) {
                  Rs("max 4 bytes!!");
                }
              }
            }
          } // EL { *result = 0; INC result; } // 254 repeats expects an extra byte
        } EL { // can assume repeatcount > 0 from earlier
//C1("1:%d ", repeatcount);
//B1("[1: %d]", repeatcount);
          *result = repeatcount; INC result;
          repeatcount = 0;
// for palette mode: write 0 here? (one byte per palette pixel w/ 255 cmd to switch palette page ?
        } // end of repeat packet
      } // end of if endrepeat (write repeat count value)
      // repeatcount will be 0 here ...
 //     IF (endrepeat AND !lastpixelmatch) {
 //       writepixel = 1; // write next pixel unless this is last pixel
 //     }         handled earlier
      // end of repeatcount handling
    } EF (thispixel.r EQ lastpixel.r AND
          thispixel.g EQ lastpixel.g AND
          thispixel.b EQ lastpixel.b AND
          thispixel.a EQ lastpixel.a    ) {
      repeatcount = 1; // start counting for repeat packet
      writepixel = 0;  // do not write, it was lastpixel
//Gs("1");
    }
    IF (writepixel) {
      // IF this pixel is in the palette, use the palette index... otherwise toggle toggle so it can be added
      //  can only add if palette mode is enabled, so toggle toggle toggle might be required
      // UNLESS there are too many already in the palette, then just write the colour
      IN paletteindex = -1;
      WI (INC paletteindex LQ palettenumitems) { // iterate through palette considering possible gap
        IF (paletteindex EQ transparentpixel.r) { CT; } // no colour stored at this index
        EF (paletteindex EQ palettenumitems AND   // last index ?
            paletteindex LT transparentpixel.r) { BK; } // if below gap, skip last inc
        EF (thispixel.r EQ paletteitems[paletteindex].r AND
            thispixel.g EQ paletteitems[paletteindex].g AND
            thispixel.b EQ paletteitems[paletteindex].b AND
            thispixel.a EQ paletteitems[paletteindex].a) {
          IF (!palettemode) { // enable palette mode if it is disabled
            palettemode = !palettemode;
            *result = transparentpixel.r; INC result;
            *result = transparentpixel.g; INC result;
            *result = transparentpixel.b; INC result;
            *result = 0xFF;               INC result; // toggle
            *result = paletteindex; INC result;
W1("%d ", palettemode);
          } // palette mode enabled, write indexed value
          *result = paletteindex; INC result;
//C1("%d ", paletteindex);
          lastpixel = paletteitems[paletteindex];
          paletteindex = -1; // handle here to avoid gap weirdness
          BK; // this colour is in the palette
        }
      }
      IF (palettenumitems GT 254) {
        Rs("[palette overflow!]");
      } EF (palettenumitems EQ 254) {
        IF (paletteindex NQ -1) {
          // cannot add colour to palette because palette is full !
          IF (palettemode) { // disable palette mode if it is enabled
            palettemode = !palettemode;
            *result = transparentpixel.r; INC result;
            *result = transparentpixel.g; INC result;
            *result = transparentpixel.b; INC result;
            *result = 0xFF;               INC result; // toggle
W1("%d ", palettemode);
          } // palette mode disabled .. then write the colour
          *result = thispixel.r; INC result;
          *result = thispixel.g; INC result; // <---- write the colour
          *result = thispixel.b; INC result;
          *result = thispixel.a; INC result;
          lastpixel = thispixel;
Gs("*"); // unpaletted colours
// G4("(%d,%d,%d,%d) ", thispixel.r, thispixel.g, thispixel.b, thispixel.a);
        } // -1 means it was in the palette and handled earlier
      } EF (paletteindex GQ 0) { // -1 = indexed, else -> newindex
        // less than 254 items, valid index = add to palette
        IF (!palettemode) { // can only add a colour with palette mode enabled
          palettemode = !palettemode;
          *result = transparentpixel.r; INC result;
          *result = transparentpixel.g; INC result;
          *result = transparentpixel.b; INC result;
          *result = 0xFF;               INC result; // toggle
//W1("%d ", palettemode);
        } // enable palette mode, then add the colour ([toggle-]toggle-toggle)
        *result = transparentpixel.r; INC result;
        *result = transparentpixel.g; INC result;
        *result = transparentpixel.b; INC result;
        *result = 0xFF;               INC result; // toggle
        *result = transparentpixel.r; INC result;
        *result = transparentpixel.g; INC result;
        *result = transparentpixel.b; INC result;
        *result = 0xFF;               INC result; // toggle
        // paletteindex should be at new value location
        paletteitems[paletteindex].r = thispixel.r;
        paletteitems[paletteindex].g = thispixel.g;
        paletteitems[paletteindex].b = thispixel.b;
        paletteitems[paletteindex].a = thispixel.a;
//G4("+[%d,%d,%d,%d] ", thispixel.r, thispixel.g, thispixel.b, thispixel.a);
        INC palettenumitems; // index at tp.r will not be written to
        *result = thispixel.r;        INC result;
        *result = thispixel.g;        INC result; // <---- write the colour
        *result = thispixel.b;        INC result;
        *result = thispixel.a;        INC result; // new colour
        lastpixel = thispixel;
      } // added ! EL would be -1 paletteindex (indexed; handled earlier)
    } // end of writepixel
    INC savedpixel;
  }
//Ws("EOI");
  *result = transparentpixel.r; INC result;
  *result = transparentpixel.g; INC result;
  *result = transparentpixel.b; INC result;
  *result = 0;                  INC result; // 0 for end of image marker
  b->compressedlen = result - b->compressed;
//  *result = LSENDOFIMAGE;
  Y1("\n[COMPRESSEDSIZE: %d]", b->compressedlen);
  IN byteswritten = result - b->compressed;
//  Y1("[BYTESWRITTEN: %d]", byteswritten);
  IF (byteswritten NQ b->compressedlen)
    { Rs("compressed len mismatch .................^^ ...."); }
  IN pixelsread = savedpixel - b->pixels;
//  Y1("[PIXELSREAD: %d]", pixelsread);
  IN bytesread = pixelsread * 4;
//  Y1("[BYTESREAD: %d]", bytesread);
  b->compressionpct = (100.0 * (bytesread - byteswritten)) / bytesread;
  Y1("\n[Compression: %.2f%%]", b->compressionpct);
//  Y1("[Compression %%: %d]\n", ((100 * (savedbyte - source)) / (sourcemax - source)));
}

VD uncompressbitmap(bitmap *b) {
  IN pxwidth = b->width;
  IN pxheight = b->height;
  IN numpixels = pxwidth * pxheight;
  UCS source = b->compressed;
  UCS sourcemax = &source[(numpixels + 2) * 4]; // 4 bytes per packet +SOF,EOF
  px *result = b->pixels;
  px *resultmax = &result[numpixels];
  px transparentpixel = { 0, 0, 0, 0 }; // = { 0xFF, 0x00, 0xFF, 0x00 };
  px paletteitems[256];
  IN palettenumitems = 0;
  UCS savedbyte = source;
  transparentpixel.r = *savedbyte; INC savedbyte;
  transparentpixel.g = *savedbyte; INC savedbyte;
  transparentpixel.b = *savedbyte; INC savedbyte;
  transparentpixel.a = *savedbyte; INC savedbyte;
  G3("[%d,%d,%d]", transparentpixel.r, transparentpixel.g, transparentpixel.b);
  IF (transparentpixel.a) {
    Rs("extra instructions not yet expected!");
    RT;
  } EL { Ys("[DECOMPRESS]"); } //init
  px thispixel = { r: 0, g: 0, b: 0, a: 0 };
  // first three pixels are likely to be toggle toggle toggle: enable palette mode, add colour
/*
  thispixel.r = *savedbyte; INC savedbyte;
  thispixel.g = *savedbyte; INC savedbyte;
  thispixel.b = *savedbyte; INC savedbyte;
  thispixel.a = *savedbyte; INC savedbyte;
  IF (thispixel.r EQ transparentpixel.r AND
      thispixel.g EQ transparentpixel.g AND
      thispixel.b EQ transparentpixel.b) {
    IF (thispixel.a EQ 0xFF) { // toggle palette mode
    // IF first pixel is transparent pixel, it should indicate how many symbols are in the palette.
    // THEN width (up to 255px), THEN height (up to 255px for now) THEN palette symbols with INDEX
    Rs("[NO SYMBOL PALETTE MODE YET]");
    // OR colour palette header (strip TOGGLE-TOGGLE-COLOUR into prefix) (save 8 bytes per colour)
  } EL { // first pixel is a colour!
    result->r = thispixel.r;
    result->g = thispixel.g;
    result->b = thispixel.b;
    result->a = thispixel.a;
//Y4("(%d,%d,%d,%d)", thispixel.r, thispixel.g, thispixel.b, thispixel.a);
    INC result;
  } // continue from second pixel..
*/
  CH palettemode = 0;
  CH toggletoggle = 0;
  IN paletteindex = -1;
  WI (savedbyte LT sourcemax) { // limit should not be reached if image is compressed
    thispixel.r = *savedbyte; INC savedbyte;
    IF (palettemode) { // first pixel will read whole colour since palette mode begins off
      // IF (palettemode EQ 2) read g... test r and g ? 1 in 64K or test r and use g to set page ?
      IF (thispixel.r EQ transparentpixel.r OR toggletoggle EQ 2) { // read colour on toggle, add
        // read a whole colour if red byte matches
        thispixel.g = *savedbyte; INC savedbyte;
        thispixel.b = *savedbyte; INC savedbyte;
        thispixel.a = *savedbyte; INC savedbyte;
        paletteindex = thispixel.r; // index -> tp.r
      } EL { paletteindex = thispixel.r; } // 0-254 if tp.r is 255
    } EL { // read a whole colour if not in palette mode
      thispixel.g = *savedbyte; INC savedbyte;
      thispixel.b = *savedbyte; INC savedbyte;
      thispixel.a = *savedbyte; INC savedbyte;
      paletteindex = -1;
    }
    IF (thispixel.r EQ transparentpixel.r AND
        thispixel.g EQ transparentpixel.g AND
        thispixel.b EQ transparentpixel.b) {
      IF (thispixel.a EQ 0) { // end of image marker !
        IF (result EQ resultmax) {
          Gs("[SUCCESS! FULL IMAGE WRITTEN!]\n");
          IN bytesread = savedbyte - b->compressed;
   //       Y1("[BYTESREAD: %d]", bytesread);
          IF (bytesread NQ b->compressedlen)
            { Rs("compressed len mismatch"); }
          IN pixelswritten = result - b->pixels;
   //       Y1("[PIXELSWRITTEN: %d]", pixelswritten);
          IN byteswritten = pixelswritten * 4;
   //       Y1("[BYTESWRITTEN: %d]", byteswritten);
          FP compressionpct = (100.0 * (byteswritten - bytesread)) / byteswritten;
          Y1("\n[Compression: %.2f%%]\n", compressionpct);
          RT;
        } EL {
          R1("\n[%d missing pixels...]\n", resultmax - result);
          kill(0, SIGINT);

Ys("stillhere..");
          _exit(0);
          RT;
        }
      } EL { // pixel repeat marker ! or palette toggle !
        IN countvalue = thispixel.a;
//C1(" (%d)", countvalue);
//W1("1:%d ", countvalue);
        IF (thispixel.a EQ 255) { // palette instruction
/////          UCH palettebyte = *savedbyte;    INC savedbyte;
          palettemode = !palettemode; // toggle palette mode
M1("[T%d]", palettemode);
          IF (toggletoggle EQ 1) {
            IF (palettemode) { // toggle-toggle off-on
              toggletoggle = 2; // next colour is next palette colour
//Ys("wa");
              // palette mode was on, stays on, but save next colour
            } EL {             // toggle-toggle on-off (anticipate on-off-on)
              // special flags: enable two-byte mode
//              Rs("[toggle-toggle on-off has no flags]");
              toggletoggle = 2; // anticipating toggle-toggle-toggle (^_^ enable + add colour)
//Cs("wa");
            }
          } EF (toggletoggle EQ 2) { // on-off reaches here ... off-on should not ..........
            IF (!palettemode) { // if palettemode, this is an on-off-on (enable + add colour)
              Rs("[cannot disable palette mode + add colour]"); // off-on-off detected
            } EL { // ^_^ (on off on) = enable + add colour
              toggletoggle = 2; // same as toggle-toggle off-on (but palette mode is now enabled)
//Ys("waa");
            } // toggle-toggle-toggle-toggle-toggle-toggle would reach this condition 4 times
          } EL { toggletoggle = 1; } // anticipate second toggle
          countvalue = 0; // there are no repeats in the 0xFF trigger
//          Rs("[NO PALETTE MODE YET]");
        } EF (thispixel.a EQ 254) { // extra counter exists
          IN secondbyte = *savedbyte;      INC savedbyte;
//W1("2:%d ", secondbyte);
          IF (secondbyte GQ 0x80) { // extra bits in next byte
            secondbyte SUBS 0x80;  // remove flag from number
            IN thirdbyte = *savedbyte;     INC savedbyte;
//W1("3:%d ", thirdbyte);
            IF (thirdbyte GQ 0x80) { // extra bites in next byte
              thirdbyte SUBS 0x80;  // remove flag from number
              IN fourthbyte = *savedbyte;  INC savedbyte; // up to 4 bytes
//W1("4:%d ", fourthbyte);
              IF (fourthbyte GQ 0x80) {
                Rs("more than 4 bytes!");
                RT;
              } EL { countvalue ADDS (fourthbyte << 14) + (thirdbyte << 7) + secondbyte; }
            } EL { countvalue ADDS (thirdbyte << 7) + secondbyte; }
          } EL { countvalue ADDS secondbyte; } // add this many
        } // EL this is a count value .. >0 since 0 is EOI marker
 //       Y1(" (%d)", countvalue);
//        Gs("*");
//        WI (DEC thispixel.a GQ 0) {
        WI (DEC countvalue GQ 0) {
          px *prevpixel = result - 1; // first pixel should always be a colour (not a repeat)
          IF (result->r EQ prevpixel->r AND // !!
              result->g EQ prevpixel->g AND // !!
              result->b EQ prevpixel->b AND // !!
              result->a EQ prevpixel->a) {
//            Gs("="); // checks compression function when rewriting bitmap !! !! !!
          } EL { Rs("!"); }   // !!
          result->r = prevpixel->r;
          result->g = prevpixel->g;
          result->b = prevpixel->b;
          result->a = prevpixel->a;
          INC result; // copy forward
        } // if in palette mode, repeats work the same way - previous pixel copied

//        M1(" (%d)", countvalue);
      } // end of end-of-image vs repeat/palette decision
    } EL { // end of transparentpixel matching thispixel (any RGB match!)
      IF (toggletoggle EQ 1) { // this was a toggle... do not write thispixel
        Cs("T");
      } EF (toggletoggle EQ 2) { // toggle-toggle off-on means add colour to palette
        Cs("A");
        IF (!palettemode) // assumed to be true - consider on-off trigger for false option
          { Rs("[palettemode expected when toggletoggle is 2]"); }
        IF (palettenumitems LT transparentpixel.r) // if below skip-index
          { paletteindex = palettenumitems; }      //   next = len
        EL { paletteindex = palettenumitems + 1; } // el next = len + 1
        paletteitems[paletteindex].r = thispixel.r;
        paletteitems[paletteindex].g = thispixel.g;
        paletteitems[paletteindex].b = thispixel.b;
        paletteitems[paletteindex].a = thispixel.a;
//G4("+[%d,%d,%d,%d] ", thispixel.r, thispixel.g, thispixel.b, thispixel.a);
        INC palettenumitems; // index at tp.r will not be written to
        toggletoggle = 0; // end of toggle sequence
        result->r = thispixel.r;
        result->g = thispixel.g;
        result->b = thispixel.b;
        result->a = thispixel.a;
        INC result; // write newly added pixel (no extra index byte)
      } EF (palettemode) { // ^ byte after toggle not currently special
// Ys("P");  C1("%d ", paletteindex);
        IF (paletteindex EQ -1) {
          Rs("[colour in palette mode not expected]");
          thispixel = (px){ r: 0x00, g: 0xFF, b: 0xFF, a: 0xCC }; // cyan pixel
        } EF (paletteindex LT transparentpixel.r AND
              paletteindex GQ palettenumitems) {      // low index too high
          Rs("[unmapped low index palette item]");
          thispixel = (px){ r: 0xFF, g: 0x00, b: 0x00, a: 0xCC }; // red pixel
        } EF (paletteindex EQ transparentpixel.r) {   // trigger match
          Rs("[palette trigger index byte not expected]");
R4("(%d,%d,%d,%d) ", thispixel.r, thispixel.g, thispixel.b, thispixel.a);
// whole byte was read ...............because ff was seen....................................................
          // thispixel = thispixel because ff
          thispixel = (px){ r: 0xFF, g: 0xFF, b: 0x00, a: 0xCC }; // yellow pixel
        } EF (paletteindex GT transparentpixel.r AND
              paletteindex GT palettenumitems) {      // high index too high
          Rs("[unmapped high index palette item]");
          thispixel = (px){ r: 0x00, g: 0x00, b: 0xFF, a: 0xCC }; // blue pixel
        } EL { // valid palette index
//G1("%d ", paletteindex);
          thispixel.r = paletteitems[paletteindex].r;
          thispixel.g = paletteitems[paletteindex].g;
          thispixel.b = paletteitems[paletteindex].b;
          thispixel.a = paletteitems[paletteindex].a;
          result->r = thispixel.r;
          result->g = thispixel.g;
          result->b = thispixel.b;
          result->a = thispixel.a;
          INC result; // add pixel of colour from palette
        }
      } EL { // non-palette mode = normal colour
        result->r = thispixel.r;
        result->g = thispixel.g;
        result->b = thispixel.b;
        result->a = thispixel.a;
Gs("*");
        INC result; // .. happened earlier ?  colour being added to palette is also being written (no extra index byte)
      }
//      toggletoggle = 0; // no -toggle for toggle- (tt should be 1 or 0)
    }
  } // end of byte loop
  Rs("[FAIL! TOO MANY PIXELS!]\n");
}

int testvar = -1;

static VD flipbitmaph(bitmap *map) {
  IN x = -1;
  IN xmax = map->width - 1;
  IN xhalf = xmax / 2; // w=4,5,6 xh=1,2,2
  WI (INC x LQ xhalf) {
    IN y = -1;
    WI (INC y LT map->height) {
      IN pxi = y * map->width + x;
      IN pxj = y * map->width + (xmax - x);
      px swap = map->pixels[pxj];
      map->pixels[pxj] = map->pixels[pxi];
      map->pixels[pxi] = swap;
    }
  }
}

static VD flipbitmapv(bitmap *map) {
  IN y = -1;
  IN ymax = map->height - 1;
  IN yhalf = ymax / 2; // y=4,5,6 yh=1,2,2
  WI (INC y LQ yhalf) {
    IN x = -1;
    WI (INC x LT map->width) {
      IN pxi = y * map->width + x;
      IN pxj = (ymax - y) * map->width + x;
      px swap = map->pixels[pxj];
      map->pixels[pxj] = map->pixels[pxi];
      map->pixels[pxi] = swap;
    }
  }
}

// to rotate a bitmap we need to copy it from -- shape to || shape

static VD destroybitmap(bitmap *map) {
  IF (map) {
    IF (map->pixels)
      { free(map->pixels); }
    IF (map->compressed)
      { free(map->compressed); }
    free(map);
  }
}
/*
static IN loadpngtobitmap(CS path, bitmap *map) {
  FILE *fp = OPENBLOCKFILE(path);
  IF (!fp) { RT -1; }
  CH header[LSPNGHEADERSIZE];
  fread(header, 1, LSPNGHEADERSIZE, fp);
  IF (png_sig_cmp(header, 0, LSPNGHEADERSIZE)) { RT -2; }
  png_structp readptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  IF (!readptr) { fclose(fp); RT -3; }
  png_infop rinfoptr = png_create_info_struct(readptr);
  IF (!rinfoptr)
    { png_destroy_read_struct(&readptr, NULL, NULL); fclose(fp); RT -4; }
  png_infop rendinfoptr = png_create_info_struct(readptr);
  IF (!rendinfoptr)
    { png_destroy_read_struct(&readptr, &rinfoptr, NULL); fclose(fp); RT -5; }
  IF (setjmp(png_jmpbuf(readptr)))
    { png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr); fclose(fp); RT -6; }
  png_init_io(readptr, fp);
  png_set_sig_bytes(readptr, LSPNGHEADERSIZE);
  png_read_info(readptr, rinfoptr);
  map->width = png_get_image_width(readptr, rinfoptr);
  map->height = png_get_image_height(readptr, rinfoptr);
  IN bitdepth = png_get_bit_depth(readptr, rinfoptr);
MLOG1("BITDEPTH: %d ", bitdepth);
  IF (bitdepth == LSPNGWIDEBITDEPTH)
    { png_set_strip_16(readptr); }
  EF (bitdepth < LSPNGBITDEPTH)
    { png_set_packing(readptr); }
  IN colortype = png_get_color_type(readptr, rinfoptr);
  IF (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA)
    { png_set_gray_to_rgb(readptr); }
  IF (colortype & PNG_COLOR_MASK_ALPHA)
    { png_set_strip_alpha(readptr); RLOG("MASKALPHA"); }
  png_read_update_info(readptr, rinfoptr);
  bitdepth = png_get_bit_depth(readptr, rinfoptr);
CLOG1("BITDEPTH2: %d ", bitdepth);
  colortype = png_get_color_type(readptr, rinfoptr);
  IF (bitdepth != LSPNGBITDEPTH || colortype != LSPNGCOLOURTYPE)
    { fprintf(stderr, "transform fail\n");
      png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr); fclose(fp); RT -7; }
  IN channels = png_get_channels(readptr, rinfoptr);
YLOG1("CHANNELS: %d ", channels);
  IN rowbytes = png_get_rowbytes(readptr, rinfoptr);
  IF (channels != LSPNGRGBCHANNELS || (channels != sizeof(px)) ||
      (rowbytes != (map->width * channels)))
    { fprintf(stderr, "channel or size mismatch\n");
      fprintf(stdout, "channels: %d %d %d\n", channels, LSPNGRGBCHANNELS, sizeof(px));
      fprintf(stdout, "size: %d %d\n", rowbytes, map->width * channels);
      png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr); fclose(fp); RT -8; }

  map->pixels = (px *)calloc(sizeof(px), map->width * map->height);

  png_bytep *rrowptrs = (png_bytep *)calloc(sizeof(png_bytep), map->height);
  IN rri = 0; WI (rri < map->height)
    { rrowptrs[rri] = (png_bytep)&map->pixels[rri * map->width]; rri++; }
//  png_set_rows(readptr, rinfoptr, &rrowptrs);
  png_read_image(readptr, rrowptrs);
  png_read_end(readptr, rendinfoptr);
  png_destroy_read_struct(&readptr, &rinfoptr, &rendinfoptr);
  free(rrowptrs);
  CLOSEBLOCKFILE(fp);
  RT 0;
}

static IN savebitmaptopng(bitmap *map, CS path) {
  IF (!map->pixels) { RT -1; }
  FILE *fp = SAVEBLOCKFILE(path);
  IF (!fp) { RT -2; }

  png_structp writeptr = png_create_write_struct(LSPNGVERSION, NULL, NULL, NULL);
  IF (!writeptr) { fclose(fp); RT -3; }
  png_infop winfoptr = png_create_info_struct(writeptr);
  IF (!winfoptr)
    { png_destroy_write_struct(&writeptr, NULL); fclose(fp); RT -4; }
  IF (setjmp(png_jmpbuf(writeptr)))
    { png_destroy_write_struct(&writeptr, &winfoptr); fclose(fp); RT -5; }

  png_init_io(writeptr, fp);
  png_set_compression_level(writeptr, LSPNGCOMPRESSLEVEL);
  png_set_IHDR(writeptr, winfoptr, map->width, map->height,
    LSPNGBITDEPTH, LSPNGCOLOURTYPE, LSPNGNOINTERLACE, LSPNGCOMPRESSTYPE, LSPNGFILTERTYPE);
  png_bytep *wrowptrs = (png_bytep *)calloc(sizeof(png_bytep), map->height);
  IN wri = 0; WI (wri < map->height)
    { wrowptrs[wri] = (png_bytep)&map->pixels[wri * map->width]; wri++; }
  png_write_info(writeptr, winfoptr);
  png_write_image(writeptr, wrowptrs);
  png_write_end(writeptr, winfoptr);
  free(wrowptrs);
  CLOSEBLOCKFILE(fp);
  RT 0;
}
*/
static IN savecompressedtorle(bitmap *map, CS path) {
  IF (!map->compressed) { Rs("NODATA"); RT -1; }
  FILE *fp = SAVEBLOCKFILE(path);
  IF (!fp) { Rs("SAVEFAIL"); RT -2; }
  CS cc = map->compressed;
  CS ccmax = &cc[map->compressedlen];
  WI (cc LT ccmax) {
    PUTFCH(fp, *cc);
    INC cc;
  }
  CLOSEBLOCKFILE(fp);
  RT 0;
}

static VGImage createimagefrombitmap(bitmap *map) {
  VGImageFormat rgba = VG_sABGR_8888;
  VGImage image = vgCreateImage(rgba, map->width, map->height, VG_IMAGE_QUALITY_BETTER);
  vgImageSubData(image, map->pixels, sizeof(px) * map->width, rgba, 0, 0, map->width, map->height);
  // vgSetPixels(x, y, image, 0, 0, width, height); vgDestroyImage(image);
  RT image;
}

/*
static VGImage createimagefrompng(CS path) {
  bitmap *map = createemptybitmap();
  IF (loadpngtobitmap(path, map) EQ 0) {
    VGImage img = createimagefrombitmap(map);
    destroybitmap(map);
    RT img;
  } EL { destroybitmap(map); RT (VGImage)NULL; }
}
*/

static VD drawimage(VGImage img, VGfloat x, VGfloat y, IN w, IN h) {
  vgSetPixels(x, y, img, 0, 0, w, h);
}

static CH saveimagetobitmap(VGImage img, bitmap *map) {
Rs("TODO");
}

static CH saveimagetopng(VGImage img, CS path) {
Rs("TODO");
}

static VD destroyimage(VGImage img) {
  vgDestroyImage(img);
}
/*
//#define STREAMSRCDIR "/dev/shm/ramba"
#define STREAMSETID  "0"
#define STREAMEATID  "1"
CH savescreenshot(IN x, IN y, IN w, IN h, CS path) {
  bitmap *map = createbitmap(w, h);
  vgReadPixels(map->pixels, sizeof(px) * w, VG_sABGR_8888, x, y, w, h);
  flipbitmapv(map);

//  compressbitmap(map);
//  uncompressbitmap(map);
//FS cmd = OPENCMD("ls -lh /dev/shm/ramba/?*.png");
//WI (1) { IN ch = GETFCH(cmd); BKEQEOF(ch); Wc(ch); }
//CLOSECMD(cmd);
//  FS png0 = OPENFILE(path);
//  WI (png0) {
//    CLOSEFILE(png0);
//    Cxc(96); WAIT100MS;     // wait for receiver to remove frame
//    png0 = OPENFILE(path);
//  } // wait for failure (won't be open)

//  LINESCRIPT(uptimeA, "cat /proc/uptime");
  savebitmaptopng(map, path);
//  LINESCRIPT(uptimeB, "cat /proc/uptime");
//  LINESCRIPT(uptimeC, "cat /proc/uptime");
//  FP upA = atof(uptimeA);
//  FP upB = atof(uptimeB);
//  FP upC = atof(uptimeB);
//  FP savetime = (upB - upA) * 1000.0;
//  FP zerotime = (upC - upB) * 1000.0;
//  R2("\n[PNGSAVETIME: %.3fms, zerotime: %.3fms]", savetime, zerotime);
  //  R2("\n[compress+savetime: >>>>>> %.3fms <<<<<<, zerotime: %.2fms]", savetime, zerotime); // <--------- REFERENCE STATS!
  //  VGImage img = createimagefrombitmap(map);
  //  drawimage(img, 50, h / 2, w, h);
  //  destroyimage(img);
  destroybitmap(map);
Mxc(96);
  RT 0;
}
*/
CH savecompressedscreenshot(IN x, IN y, IN w, IN h, CS path) {
  bitmap *map = createbitmap(w, h);
//  vgReadPixels(map->pixels, sizeof(px) * w, VG_sABGR_8888, x, y, w, h);
  vgReadPixels(map->pixels, sizeof(px) * w, VG_sRGBA_8888, x, y, w, h);
  flipbitmapv(map);
  LINESCRIPT(uptimeA, "cat /proc/uptime");
  Ys("[COMPRESS]");
  compressbitmap(map);
  LINESCRIPT(uptimeB, "cat /proc/uptime");
  Cs("[SAVE]");
  savecompressedtorle(map, path);
//  Ms("[UNCOMPRESS]");
//  uncompressbitmap(map);
//  LINESCRIPT(uptimeC, "cat /proc/uptime");
  Bs("[DONE]");
  LINESCRIPT(uptimeC, "cat /proc/uptime");
  LINESCRIPT(uptimeD, "cat /proc/uptime");
  FP upA = atof(uptimeA);
  FP upB = atof(uptimeB);
  FP upC = atof(uptimeC);
  FP upD = atof(uptimeD);
//  FP upE = atof(uptimeE);
  FP compressiontime = (upB - upA) * 1000.0;
  //  FP uncompressiontime = (upC - upB) * 1000.0;
  FP savetime = (upC - upB) * 1000.0;
  savetime ADDS compressiontime;
  FP zerotime = (upD - upC) * 1000.0;
  Y2("\n[RLESAVETIME: %.3fms, zerotime: %.3fms]", savetime, zerotime);
  //  M2("\n[compressed in %.3fms, uncompressed in %.3fms]", compressiontime, uncompressiontime); // <---------------------------------- STATS !
  //  Y1("\n[predicted compress+save time: >>>>>> %.3fms <<<<<<]", compressiontime + savetime);
  //  C2("\n[savetime: %.3fms, zerotime: %.2fms]", savetime, zerotime); // <--------- REFERENCE STATS!
 //  savebitmaptopng(map, path);
 //  VGImage img = createimagefrombitmap(map);
 //  drawimage(img, 50, h / 2, w, h);
 //  destroyimage(img);
  destroybitmap(map);
  Mxc(96);
  RT 0;
}

//typedef struct _Login {
//  IN loginid;
//  CS username;
//  struct _Login *next;
//} Login;
//Login *allLogins = NULL;
typedef struct _Box {
  IN x, y, w, h;
  IN id;
//  IN pathlen;
  CS pathstr;
  CS tgtpathstr;
  CS donepathstr;
//  struct _User *users;
//  struct _Box *prev;
  struct _Box *next;
} Box;

typedef struct _User {
  IN userid;
  CS username;
  Box *stream;
  IN streamx, streamy, streamw, streamh;
  CH needslogin;
  CH sessionstatus;
  CS sessioncode;
  FS sessionfile;
//  CH passchar; // consider Unicode entry
//  IN passcount;
//  CH passstate;
  LF gpslat, gpslong;
  LF mousex, mousey;
  LF fontsize;
  CS inputstr;
  FS inputfile;
//  Login *login;
  struct _User *next;
} User;
User *allusers = NULL;
Box *allstreams = NULL;
/*
//Box screenstream = { 0, 0, 0, 0, 0, 0, STREAMSRCDIR "/-0.png", NULL, NULL };
CH update0screenstream(Box *stream) {
  Box *this = stream;
  CS tgtpathstr = (CS)malloc(sizeof(CH) * (this->pathlen + 1));
//  snprintf(tgtpathstr, this->pathlen + 1, "%s/=%d.png", STREAMSRCDIR, this->id);
  snprintf(tgtpathstr, this->pathlen + 1, "=%d.png", this->id);
  FS cache = OPENFILE(tgtpathstr);
  WI (cache) {
    CLOSEFILE(cache);
//    sched_yield();
Yc('.');
    cache = OPENFILE(tgtpathstr);
  } // stop when cache cannot be opened (indicating receiver has received and removed it).... ---- this is a heavy loop! hence yield!
//  savescreenshot(this->x, this->y, this->w, this->h, this->pathstr);
  FS zeroframe = WRITEFILE(this->pathstr);
  IF (!zeroframe) { RT 0; } // failure
  CLOSEFILE(zeroframe); // should be empty and existing
  IF (rename(this->pathstr, tgtpathstr) EQ 0)
    { Gs("0="); } EL { Rs("0=!!"); }
  UNMEM(tgtpathstr);
  RT 1; // success
}
*/
CH waitforcacheexistence(CS pathstr, IN wait20ms) {
  FS cache = OPENFILE(pathstr);
  IN waitcount = 0;
  WI (!cache) {
    // CLOSEFILE(cache);
    IF (INC waitcount GQ wait20ms) {
      RT 'c'; // cache file still doesn't exist
    } EL {
//      Cc('[');
//      Dc('c'); // Server error: 500 !
//      Cc(']');
      WAIT20MS;
      cache = OPENFILE(pathstr);
    }
  } // if loop end reached, file is open
  CLOSEFILE(cache);
  RT 'e'; // exists
}

CH waitforcacheremoval(CS pathstr, IN wait20ms) {
  FS cache = OPENFILE(pathstr);
  IN waitcount = 0;
  WI (cache) {
    CLOSEFILE(cache);
    IF (INC waitcount GQ wait20ms) {
      RT 'c'; // cache file still exists
    } EL {
//      Dc('[');
//      Cc('c'); // Server error: 500 !
//      Dc(']');
      WAIT20MS;
      cache = OPENFILE(pathstr);
    }
  }
  RT 'r'; // removed
}

CH updatescreenstream(Box *stream, IN wait20ms) {
  Box *this = stream; // ss;         // ^ currently not used
  // make sure receiver picked up the previous frame
  // consider partial screenshots !! !! ! ! !! !! ! ! !! !! ! ! !! !!
  CH userlemode = 1;
  IF (userlemode) {
    IN waitcount = 0;
    WI (waitforcacheremoval(this->tgtpathstr, 10) EQ 'c') // loop until 'r'
      { Ys("."); } // every 10 checks, display a dot (five per second)
    IF (savecompressedscreenshot(this->x, this->y, this->w, this->h, this->pathstr) EQ 'c') {
      Rs("rle cache not removed\n"); RT 'c';
    } EL { Gxc(96); } // Gs("[RLESAVED]"); }
    IF (rename(this->pathstr, this->tgtpathstr) EQ 0)
      { Gs("[-]->[=]"); } EL { Ys("[-][=]!!!!"); } // RLE is ready
  }
  RT 'y'; //1;
}

IN numstreamids = 0; // count how many streams
Box *savescreenstream(IN x, IN y, IN w, IN h) {
  Box *this = (Box *)malloc(sizeof(Box));
  IF (allstreams) {
    Box *ss = allstreams;
    WI (ss->next) { ss = ss->next; }
    ss->next = this;    // next (last) in list
  } EL { allstreams = this; } // first in list
  this->id = INC numstreamids;
  this->x = x;
  this->y = y;
  this->w = w;
  this->h = h;
  this->next = NULL;
  this->pathstr     = "-1.rle";
  this->tgtpathstr  = "=1.rle";
  this->donepathstr = "+1.rle";
  IF ISFILE(this->pathstr) {
    IN val = unlink(this->pathstr); // rm -1.png in first screenshot of stream
    IF (val NQ 0)
      { Rs("savescreenstream: pathstr not del"); } // file failed to delete
//      { Wxc(97); } EL { Rxc(97); } // cache cleared
  } // EL { Y1("nofile %s", this->pathstr); }
//Ys("Connected...");
//  savescreenshot(this->x, this->y, this->w, this->h, this->pathstr);
  savecompressedscreenshot(this->x, this->y, this->w, this->h, this->pathstr);
  IF (savecompressedscreenshot(this->x, this->y, this->w, this->h, this->pathstr) EQ 'c') {
    Rs("rle cache not removed\n"); RT this; // watch for error/return NULL?
  } EL { Gxc(96); } // Gs("[RLESAVED]"); }
  IF (rename(this->pathstr, this->tgtpathstr) EQ 0)
    { Yxc(96); } EL { Ys("-=!!!!"); } // RLE is ready ... else no file available !
  RT this;
}
//IN newstreamid = 1; // first new stream will be 2 (1 is full frame)
//IN newpasswordstream() {
  
//}



/*
LF gpsminylat = { -90, 0 };
LF gpsmaxylat = {  90, 0 };
LF gpsminlong = { -180, 0 };
LF gpsmaxlong = {  180, 0 };

#define USERBOXW  100
#define USERBOXH  20

IN numuserids = 0; // holds a count of how many ids have been created
User *addscreenstreamuser(Box *stream, CS username, CS usertime) { //, IN streamid) {
// userparams -> username, usertime, userpassword (later allow char-by-char)
  User *this = (User *)MEM(sizeof(User));
  this->userid = INC numuserids; // ID 1, 2, 3, 4 etc
  this->username = username;
//  ssu->streamid = INC newstreamid; // newpasswordstream();
// setstreamregion(ssu->streamid, x, y, w, h);
  this->stream = stream;
  this->streamx = stream->w - USERBOXW;
  this->streamy = stream->h - (USERBOXH * this->userid);
  this->streamw = USERBOXW;
  this->streamh = USERBOXH;
// calculate session code from input
//  this->sessioncode = "ABCDEFG"; // <--- user will be POSTing their time
//  this->sessionstatus = AP; // need to log in
// if password is correct, login else trigger clear/retry
  this->sessionstatus = AJ; // skip password checking for now
  this->sessioncode = CHMEM(10);
  this->sessioncode[0] = '$';
  token8(usertime, AT1(this->sessioncode));
  // use pwd to check we are in /dev/shm/ramba
  this->sessionfile = WRITEFILE(this->sessioncode); // file stays open
  fprintf(this->sessionfile, "MODE=%c", this->sessionstatus);
  fprintf(this->sessionfile, " USERNAME=%s\n", this->username);
// where to put the password buffer ... better to receive all at once
// if user included GPS and/or FILES -- process them
// consider special key file upload process
// tag gps location
// sessionfile:
// MODE=NPJSZL (New Pass Join Stream pauZe Leave) NAME=username
// GPS: LAT=123.456 LONG=12.345
// FILES: thisone.png thatone.png
//  this->needslogin = 1; // becomes 0 if password matches on enter
  // ^ sessionstatus = P implies it
//  this->passcount = 0; // abcdef => digits = 1-6
//  this->passstate = 1; // becomes 0 if any key does not match
// XXXXXB bad char -> state = AD Denied, XXXXXX
// how to pass passkeys via streamframe to here ... need non-stdin FD
// hash = username:passcode:when:salt -> code stored should be a login salt to point to profile info
// if you forget your passcode, what happens to profile info? it should be forgotten
// if you want to keep track of it, you can export it to / import it from email
//-----------------------------------
// Property: Value
// Property2: Value2
//----------------------------------- 5 dashes or more = section break / object delimeter
// File: "/Space Path/file.txt"
//-----------------------------------
// when needslogin is 0, streamid should become 1 or something else new (eg TL)
//  this->needslogin = 0; // LOGIN SKIPPED
//  this->streamid = 1; // FULLSCREEN ASSUMED
//  ssu->streamid = streamid; // starts in its own password stream, then gets the main stream once logged in
  this->gpslat = (LF){ 99, 999999 }; // latitude N-S
//  this->gpsxlat = { 99, 999999 };
  this->gpslong = (LF){ 99, 999999 }; // longitude E-W
  
//  ssu->gpsylatmin = {
//, gpsmaxx
  // gpsminy, gpsmaxy
  this->mousex = (LF){ 0, 0 };
  this->mousey = (LF){ 0, 0 };
  this->fontsize = (LF){ 0, 0 };
  this->next = NULL;
  IF (allusers) {
    User *su = allusers;
    WI (su->next) { su = su->next; }
    su->next = this;  // next (last) in list
  } EL { allusers = this; } // first in list

  // list users
  User *s = allusers;
  WI (s) {
    Ys(s->username);
    Mi(s->userid);
    BKEQNULL(s->next); // stop on last
    s = s->next;
  }
//  IF (s) {
//    ssu->id = s + 1;
//  } EL { ssu->id = 1; }
  // RT userid ... automatic ? retrieved from name in table .... need user table !
//  RT ssu->id;
  RT this;
}
*/
// frame writes to fifomux, fifomux writes to drawframe
// fifomux checks user fifos in a loop -- if it sees a [, it keeps
// waiting until it sees a ].... 

// (username: blaaaaa)

/*
IN addscreenstreamlogin(CS username) {
  // for quick login this should return a direct stream ID .... 
  // for password login this should return a password stream id
  IN userid = addscreenstreamuser(username, 1);
} all users have a corresponding login section on the screen */



// currently deprecated by /tau/web/http/index.html
CH savescreenreader(CS filename) {
  IF (!filename) { filename = "screenreader.html"; }
//  IF (!filename) { filename = STREAMSRCDIR "/screenreader.html"; }
  FS sr = WRITEFILE(filename);
  IF (!sr) { Rs("Failed to write screen reader."); RT -1; }
  fprintf(sr, "<html><head>\n");
  fprintf(sr, "  <title>hostname-0 screen reader</title>\n");
  fprintf(sr, "  <style id=\"imgzoom\" type=\"text/css\">\n");
  fprintf(sr, "    img { zoom: 0.5; }\n");
  fprintf(sr, "  </style>\n");
  fprintf(sr, "  <script type=\"text/javascript\">\n");
  fprintf(sr, "    function id(id) { return document.getElementById(id); }\n");
  fprintf(sr, "    function imgzoom() {\n");
  fprintf(sr, "      imgpct = id('imgpct').value;\n");
  fprintf(sr, "      imgmag = id('imgmag').value;\n");
  fprintf(sr, "      imgval = 'img { zoom: ' + (imgpct * 0.01 * imgmag) + '; }';\n");
  fprintf(sr, "      id('imgpctval').innerHTML = '' + imgpct;\n");
  fprintf(sr, "      id('imgmagval').innerHTML = '' + imgmag;\n");
  fprintf(sr, "      id('imgzoom').innerHTML = \"    \" + imgval + \"\\n\";\n");
  fprintf(sr, "    }\n");
  fprintf(sr, "  </script>\n");
  fprintf(sr, "</head><body>\n");
  fprintf(sr, "  <center><input id=\"imgpct\" type=\"range\" min=\"1\" max=\"50\" value=\"50\" oninput=\"imgzoom();\" onchange=\"imgzoom();\" /><span id=\"imgpctval\">50</span><b>%</b> \n");
  fprintf(sr, "  <input id=\"imgmag\" type=\"range\" min=\"1\" max=\"2\" step=\"0.01\" value=\"1\" oninput=\"imgzoom();\" onchange=\"imgzoom();\" /><span id=\"imgmagval\">1</span><b>x</b></center>\n");
  fprintf(sr, "  <br /><center><img id=\"img\" src=\"./hostname-0.png?\" /></center>\n");
  fprintf(sr, "  <script type=\"text/javascript\">\n");
  fprintf(sr, "    function updateimg() {\n");
  fprintf(sr, "      src = id('img').src.substr(0, id('img').src.indexOf('?'));\n");
  fprintf(sr, "      id('img').src = src + '?' + new Date().getTime();\n");
  fprintf(sr, "    }\n");
  fprintf(sr, "    imgtimer = setInterval(updateimg, 1000);\n");
  fprintf(sr, "  </script>\n");
  fprintf(sr, "</body></html>\n");
  CLOSEFILE(sr);
  RT 0;
}

