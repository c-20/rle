#define MAXUTF8CHARLEN     32

#define Uboxtl    0x250C
#define Uboxdr    Uboxtl   // down-right at top-left
//#define Uboxudl   0x251C
#define Uboxtbl   0x251C
#define Uboxudr   Uboxtbl  // up-down-right at middle-left
#define Uboxtbr   0x2524
#define Uboxudl   Uboxtbr  // up-down-left at middle-right
#define Uboxtr    0x2510
#define Uboxdl    Uboxtr   // down-left at top-right
#define Uboxbl    0x2514
#define Uboxur    Uboxbl   // up-right at bottom-left
#define Uboxbr    0x2518
#define Uboxul    Uboxbr   // up-left at bottom-right
#define Uboxhz    0x2500
#define Uboxlr    Uboxhz   // left-right is horizontal
#define Uboxvt    0x2502
#define Uboxud    Uboxvt   // up-down is vertical
#define Uboxudlr  0x253C   // up-down-left-right
#define Uboxdiamond  0x25C7
#define Uboxtinysquare   0x25AB
#define Uboxlozenge      0x25CA
// still need middle-top and middle-bottom

UCS nextutf8char(UCS str, INP codepoint) {
  IN seqlen = 0;
//  IN datalen = (IN)strlen((CCS)str);
//  UCS p = str;
//  IF (datalen LT 1 OR str[0] EQNUL)
//    { RT NULL; }
  IF (str[0] EQNUL) { RT NULL; } // NULL is always NULL
  EF ((str[0] & 0x80) EQ 0x00) {        // 0xxxxxxx
    *codepoint = (wchar_t)str[0];
    seqlen = 1;
  } EF ((str[0] & 0xE0) EQ 0xC0 AND     // 110xxxxx
        (str[1] & 0xC0) EQ 0x80    ) {  // 10xxxxxx
    *codepoint = (IN)(   (((IN)(str[0] & 0x1F)) << 6)
                       |              (str[1] & 0x3F) );
    seqlen = 2;
  } EF ((str[0] & 0xF0) EQ 0xE0 AND     // 1110xxxx
        (str[1] & 0xC0) EQ 0x80 AND     // 10xxxxxx
        (str[2] & 0xC0) EQ 0x80    ) {  // 10xxxxxx
    *codepoint = (IN)(
                      (((IN)(str[0] & 0x1F)) << 12)
                    | (((IN)(str[1] & 0x3F)) <<  6)
                    |               (str[2] & 0x3F) );
    seqlen = 3;
  } EF ((str[0] & 0xF8) EQ 0xF0 AND    // 11110xxx
        (str[1] & 0xC0) EQ 0x80 AND    // 10xxxxxx
        (str[2] & 0xC0) EQ 0x80 AND    // 10xxxxxx
        (str[3] & 0xC0) EQ 0x80    ) { // 10xxxxxx
    *codepoint = (IN)(   (((IN)(str[0] & 0x07)) << 18)
                       | (((IN)(str[1] & 0x3F)) << 12)
                       | (((IN)(str[2] & 0x3F)) <<  6)
                       |               (str[3] & 0x3F) );
    seqlen = 4;
  } EL { *codepoint = '!'; str++; Rs("UTF8!"); RT str; } // invalid char ! // NULL; } // moves str pointer to prevent infinite loop!
  str += seqlen;
  RT str; // consider risk of jumping over NUL if first two bytes imply
          // two more but actually there is 1 or 0 and a NUL ...........
  // fix: RT NULL on NUL
}

UCS nextutf8charstr(UCS str, UCS charstr) {
  UCS thisstr = str;
  IN codepoint = 0;
  UCS reststr = nextutf8char(str, &codepoint);
  WI (thisstr LT reststr) {
    charstr[0] = thisstr[0];
    INC thisstr;
    INC charstr;
  }
  charstr[0] = NUL;
  RT reststr;
}

IN utf8len(UCS str) {
  IN len = 0;
  WI (str[0] NQNUL) {
    UCH charstr[10]; // expect max 4+NUL
    str = nextutf8charstr(str, charstr);
    IN codepoint = 0;
    nextutf8char(charstr, &codepoint);
    IN charwidth = 1;
    IF ( INRANGE(codepoint,  0x3400,  0x4DBF) OR
         INRANGE(codepoint,  0x4E00,  0x9FFF) OR
         INRANGE(codepoint,  0xF900,  0xFAFF) OR
         INRANGE(codepoint, 0x20000, 0x2A6DF) OR
         INRANGE(codepoint, 0x2A700, 0x2B73F) OR
         INRANGE(codepoint, 0x2B740, 0x2B81F) OR
         INRANGE(codepoint, 0x2B820, 0x2CEAF) OR
         INRANGE(codepoint, 0x2F800, 0x2FA1F) OR
         INRANGE(codepoint, 0x20000, 0x2FFFD) OR // match earlier, or also include reserved codepoints
         INRANGE(codepoint, 0x30000, 0x3FFFD)    )
      { charwidth = 2; }
    len ADDS charwidth;
Ws(charstr);
IF (charwidth GT 1) { Ri(charwidth); } EL { Yi(charwidth); }
//    INC len; // double-width ?
Ci(len);
IF (len GQ 100) { Rs("100"); RT 100; }
  }
Bi(len); _;
  RT len;
} 

IN utf8thischar(IN codepoint, UCS utf8buffer) {
  IF INRANGE(codepoint, 0, 127) {
    // 0xxxxxxx
    utf8buffer[0] = codepoint;
    utf8buffer[1] = NUL;
    RT 1;
  } EF INRANGE(codepoint, 128, 2047) {
    // 110xxxxx 10xxxxxx
    utf8buffer[0] = (codepoint >> 6) + 0xC0;
    utf8buffer[1] = (codepoint & 0x3F) + 0x80;
    utf8buffer[2] = NUL;
    RT 2;
  } EF INRANGE(codepoint, 2048, 65535) {
    // 1110xxxx 10xxxxxx 10xxxxxx
    utf8buffer[0] = (codepoint >> 12) + 0xE0;
    utf8buffer[1] = ((codepoint >> 6) & 0x3F) + 0x80;
    utf8buffer[2] = (codepoint & 0x3F) + 0x80;
    utf8buffer[3] = NUL;
    RT 3;
  } EF INRANGE(codepoint, 65536, 1112064) {
    // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    utf8buffer[0] = (codepoint >> 18) + 0xF0;
    utf8buffer[1] = ((codepoint >> 12) & 0x3F) + 0x80; 
    utf8buffer[2] = ((codepoint >> 6) & 0x3F) + 0x80;
    utf8buffer[3] = (codepoint & 0x3F) + 0x80;
    utf8buffer[4] = NUL;
    RT 4;
  } EL { RT 0; } // out of range
}

IN repeatutf8(IN codepoint, IN numtimes) {
  CH charbuffer[MAXUTF8CHARLEN];
  IN charlen = utf8thischar(codepoint, charbuffer);
  IN totallen = 0;
  IN ni = -1;
  WI (INC ni LT numtimes) {
    printf("%s", charbuffer);
    totallen ADDS charlen;
  }
  RT totallen;
}

#define U1(cp)       repeatutf8(cp, 1)
#define Un(cp, nt)   repeatutf8(cp, nt)

VD copyfromhex(CS target, CS source) {
  CS tgch = target; // write to
  CS srch = source; // read from
  WI (*srch NQNUL) {
    IF (*srch EQ '%') { // expect 2 more chars
      CH hexch1 = *(srch + 1);
      CH hexch2 = *(srch + 2);
      IF ISHEXPAIR(hexch1, hexch2) {
        IN hexval = HEXPAIRVALUE(hexch1, hexch2);
        *tgch = hexval; // write value
        INC tgch;
        srch += 3; // increment %FF
      } EL { // hex char not valid, % is %
        *tgch = *srch;
        INC tgch;
        INC srch;
      }
    } EL { // normal char, copy as is
      *tgch = *srch;
      INC tgch;
      INC srch;
    }
  }
  *tgch = NUL; // null-terminate!
}

VD copynopath(CS target, CS source) {
  CS tgch = target; // write to
  CS srch = source; // read from
  WI (*srch NQNUL) {
    IF (*srch EQ '/') { // expect 2 more chars
      *tgch = '-';
      INC tgch;
      INC srch;
    } EL {
      *tgch = *srch;
      INC tgch;
      INC srch;
    }
  }
  *tgch = NUL;
}
