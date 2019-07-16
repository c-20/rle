#include "code.h"
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <EGL/egl.h>
#include <bcm_host.h>
#include <assert.h>
#include <termios.h>
#include <stdio.h> // was not included ............
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "token.h"
#include <png.h>
#include <paths.h>
#include "shell.h" // used by mousedriver
//#include "lspng.h" // uses token, shell
#include "lsrle.h"
#include "drawtext.h" // uses shell
#include "mousedriver.h"

#include "lf.h"
#include "lfmap.h"

#define drawshell main

#define RAMDIR          "/V:"
#define CODESIZE	8
#define TIMESIZE	25
#define NAMESIZE        128
// ^ should be same as in users.c
/*
struct termios newterm, savedterm;
VD saveterm() {
  tcgetattr(fileno(stdin), &savedterm);
}
VD rawterm() {
  memcpy(&newterm, &savedterm, sizeof(struct termios));
  newterm.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
  newterm.c_cc[VTIME] = 0;
  newterm.c_cc[VMIN] = 0;
  tcsetattr(fileno(stdin), TCSANOW, &newterm);
}
VD restoreterm() {
  tcsetattr(fileno(stdin), TCSANOW, &savedterm);
Gs("term restored\n");
}
*/
//VD interrupthandler() {
//  restoreterm();
//  _exit(0);
//}
CS spin(CS spinstr) {
  IF (spinstr[0] EQ '|' )  { spinstr[0] = '/';  }
  EF (spinstr[0] EQ '/' )  { spinstr[0] = '-';  }
  EF (spinstr[0] EQ '-' )  { spinstr[0] = '\\'; }
  EF (spinstr[0] EQ '\\') { spinstr[0] = '|';  }
  RT spinstr;
}

IN runsizecmd(CS cmd, INP width, INP height) {
  // consider variable argument alternative - cmd, delimeter, p, p, p, p
//  FS cmdf = OPENCMD(cmd);
  FS cmdf = OPENSCRIPT(cmd);
  CH fmode = AW;
  IN gwidth = 0;
  IN gheight = 0;
  IN inch = NUL;
  WI NOTFEOF(cmdf) {
    inch = GETFCH(cmdf);
    IF (fmode EQ AW AND CHISINT(inch)) {
      gwidth = (gwidth * 10) + (inch - A0);
    } EF(fmode EQ AH AND CHISINT(inch)) {
      gheight = (gheight * 10) + (inch - A0);
    } EF (inch EQ Ax) {
      IF (fmode EQ AW) { fmode = AH; } // consider more dimensions
    } EL { fmode = AX; BK; }
  }
//  CLOSECMD(cmdf);
  CLOSESCRIPT(cmdf);
  *width = gwidth;
  *height = gheight;
  RT 0; // should return command status
}

typedef struct _TextboxList {
  FS file;
  LF top, left, bottom, right, width, height;
  IN pxwidth, pxheight, pxlength;
  IN dtcols, dtrows;
//  PF data;
 /* PF = Pointer to structure with ->LF list, P to next, P to prev .. dynamic length! */
  CS pixels;
//  IN pvwidth, pvheight, pvlength;
//  CS pvpixels, pvdata;
  struct _TextboxList *parent;
  struct _TextboxList *next;
} TextboxList;
IN addtextbox(TextboxList **tblist, FS file) {
  TextboxList *tb = (TextboxList *)malloc(sizeof(TextboxList));
  IF (!tb) { Rxc(97); RT 0; } // malloc fail
  tb->file = file;
  IF (file EQNUL) { Ys("{NOTEXT}"); } // file not open
  tb->next = *tblist; // 
  tb->parent = tb->next; // <--- each new textbox is a child of the previous one by default for now
  *tblist = tb;       // <------- new boxes added to start of list
  RT 1; // file added
}

typedef struct _ItemBox {
  List *itemlist;
  IN selindex;
  CS fontpath;
  IN itemfontsize;
  IN selfontsize;
  IN itemsperrow;
  IN numitems;
  XY boxpos;
  XY boxmargin;
  IN numlines;
  XYWH itembox;
  XY selboxoffset;
  XY selboxpos;
  XY selboxmargin;
  XYWH selbox;
  RGB itemboxbg;
  RGB selboxbg;
  RGB selitemtext;
  XYWH itemselbox;
  RGB itemselboxbg;
  RGB itemboxtext;
  struct _ItemBox *next;
} ItemBox;
ItemBox *createitembox(List *itemlist, IN selindex, CS fontpath, IN itemfontsize, IN selfontsize, IN itemsperrow, IN numitems, IN x, IN y) {
  ItemBox *ib = (ItemBox *)MEM(sizeof(ItemBox));
  ib->itemlist = itemlist;
  ib->selindex = selindex;
  ib->fontpath = fontpath;
  ib->itemfontsize = itemfontsize;
  ib->selfontsize = selfontsize;
  ib->itemsperrow = itemsperrow;
  ib->numitems = numitems;
  ib->boxpos.x = x;
  ib->boxpos.y = y;
  ib->boxmargin.x = 8;
  ib->boxmargin.y = 8;
  ib->numlines = (ib->numitems / ib->itemsperrow) + 1;
  ib->selboxoffset.x = 0;
  ib->selboxoffset.y = 64;
  ib->selboxpos.x = ib->boxpos.x + ib->selboxoffset.x; // offset for itemselbox ........
  ib->selboxpos.y = ib->boxpos.y + ib->selboxoffset.y;
  ib->selboxmargin.x = 8;
  ib->selboxmargin.y = 8;
  ib->selbox.x = ib->boxpos.x - ib->selboxoffset.x;        // duplicate of selboxpos ? nope..
  ib->selbox.y = ib->boxpos.y - ib->selboxoffset.y; // flip?
  ib->selbox.w = ib->selboxmargin.x + ib->selfontsize + ib->selboxmargin.x;
  ib->selbox.h = ib->selboxmargin.y + ib->selfontsize + ib->selboxmargin.y;
  ib->selboxbg = (RGB){ 0x40, 0x40, 0x40 };
  ib->selitemtext = (RGB){ 0xFF, 0xFF, 0x00 };
  ib->itemselbox.x = ib->selboxpos.x - 2 + (((ib->selindex - 1) % ib->itemsperrow) * ib->itemfontsize);
  ib->itemselbox.y = ib->selboxpos.y - 3 - (((ib->selindex - 1) / ib->itemsperrow) * ib->itemfontsize);
  ib->itemselbox.w = ib->itemfontsize + 2; // extra 2 on left ....
  ib->itemselbox.h = ib->itemfontsize + 2; // extra 2 at top ....
  ib->itemselboxbg = (RGB){ 0xFF, 0xFF, 0x00 };
  ib->itembox.x = ib->boxpos.x - ib->boxmargin.x;
  ib->itembox.w = ib->boxmargin.x + (ib->itemsperrow * ib->itemfontsize) + ib->boxmargin.x;
  ib->itembox.h = ib->boxmargin.y + (ib->numlines * ib->itemfontsize) + ib->boxmargin.y;
  ib->itembox.h SUBS ((ib->numlines - 1) * ib->itemfontsize) / 2; // only one line should be full height
  ib->itembox.y = ib->boxpos.y - ib->itembox.h + ib->boxmargin.y; // - ((ib->numlines - 1) * ib->itemfontsize); // aligns with first line of text // CONSIDER VARIABLE FONT SIZE
  ib->itemboxbg = (RGB){ 0x30, 0x30, 0x30 };
  ib->itemboxtext = (RGB){ 0xFF, 0x00, 0xFF };
  RT ib;
}
ItemBox *allitemboxes = NULL;
VD additembox(ItemBox *ib) {
  IF (allitemboxes) {
    ItemBox *aib = allitemboxes;
    WI (aib->next) { aib = aib->next; }
    aib->next = ib;
  } EL { allitemboxes = ib; }
}

VD drawitembox(ItemBox *ib) {
// ^^^ itemboxy will be affected  yh this!!! itemboxh before itemboxy
  setfill(ib->itemboxbg.r, ib->itemboxbg.g, ib->itemboxbg.b, 1);
  drawroundrect(ib->itembox.x, ib->itembox.y, ib->itembox.w, ib->itembox.h, 32, 16);
  setfill(ib->selboxbg.r, ib->selboxbg.g, ib->selboxbg.b, 1);
  drawroundrect(ib->selbox.x, ib->selbox.y, ib->selbox.w, ib->selbox.h, 16, 32);
  List *selitem = ib->itemlist; // itemlist could be multichar...
  WI (selitem) {
    IF (selitem->index EQ ib->selindex) {
      setfill(ib->selitemtext.r, ib->selitemtext.g, ib->selitemtext.b, 1);
      drawlines(ib->fontpath, ib->selfontsize, ib->selboxpos.x, ib->selboxpos.y, selitem->item); // inc selx to draw several chars
// [ni hao ma] -> [aaaa bbbb cccc]
// need to draw selection before list so that selection boxes can also be drawn
// drawroundrect() 400, 500 + selx, sely
    }
    selitem = selitem->next;
  }
  setfill(ib->itemselboxbg.r, ib->itemselboxbg.g, ib->itemselboxbg.b, 1);
  drawroundrect(ib->itemselbox.x, ib->itemselbox.y, ib->itemfontsize, ib->itemfontsize, 4, 4);
  setfill(ib->itemboxtext.r, ib->itemboxtext.g, ib->itemboxtext.b, 1);
  drawlist(ib->fontpath, 32, ib->boxpos.x, ib->boxpos.y, ib->itemlist, ib->itemsperrow, ib->itemfontsize, ib->itemfontsize);
}

/*
VD signalhandler(IN signo) {
  Mc('-');
  Cs("SIG"); Yi(signo); 
  Mc(':');
  Cs(strsignal(signo));
  Mc('-'); _;
  restoreterm();
  _exit(0);
}

VD catchsignals() {
  struct sigaction sa;
  sa.sa_handler = signalhandler;
  IN si = 0;
  WI (INC si LQ 64) {
    IF (si EQ SIGCHLD) { CT; }
    sigaction(si, &sa, NULL);
  }
  // SIGTERM, SIGSEGV, SIGINT, SIGHUP, etc ... maybe skip some ..
//  signal(SIGTERM, interrupthandler);
//IF (errno) { eRi(errno); }
//  signal(SIGSEGV, interrupthandler);
//IF (errno) { eRi(errno); }
//  signal(SIGINT, interrupthandler);
//IF (errno) { eRi(errno); }
//  signal(SIGHUP, interrupthandler);
//IF (errno) { eRi(errno); }
}
*/
/*
typedef struct _LFList {
  LF value;
  struct _LFList *next;
} LFList;

typedef struct _Parameter {
  CS name;
  LFList values;
  struct _Parameter *subvalues;
  struct _Parameter *next;
} Parameter;
*/ // moved to lf.h
typedef struct _Context {
  IN index;
  CS id;
  CS name; // can maybe use a param instead
  CS label;
  CS fontpath;
//  IN fontsize;
  XYWH box;
  XYWH namebox;
  XYWH parambox;
  XYWH mapbox;
  XY curve;
  IN textsize;
  CS text;
  IN texti;
  CH visible;
  RGB colour;
  FP opacity;
  CH bdvisible;
  RGB bdcolour;
  FP bdwidth;
  FP bdopacity;
  CH bgvisible;
  RGB bgcolour;
  FP bgopacity;
  List *selectionlist;
  IN selectionindex;
  IN selnumitems;
  IN selitemsperrow;
  Parameter *parameters;
} Context;

typedef struct _Shell {
  CS fontpath;
  // -> multiple selections
  TextboxList *textfiles;
  //Textbox *userinputs;
  Box *stream;
  IN pagewidth, pageheight;
  IN gridwidth, gridheight;
  IN width, height;
  IN layer;
  UCH opacity;
//  FT_Library ftlib; maybe later
  Pointer pointer;
  Context context[10];
  Parameter *parameters;
  CH saveonchangetext;
  CH saveonchangespin;
  CH texthaschanged;
  CH spinhaschanged;
  CH spinstr[2]; // consider longer spinstrings - flag list .........................................................................12345678
  CH ctxti;
} Shell;
Shell shell;

VD drawinfobox() {
  Rs("NOTEXPECTEDDURINGTEST");
  setfill(0x33, 0x33, 0x33, 1);
  drawchars(shell.fontpath, 32, shell.width - 700, shell.height - 128, "\xE9\xA7\x9D\xE9\xB9\xBF \xE9\xA7\x9D\xE9\xB9\xBF.");
  setfill( 135,  206,  250, 1);
  drawchars(shell.fontpath, 20, 0, 0, "\xE6\x9B\xB2\xE7\xBA\xBF\xE6\x95\x91\xE5\x9B\xBD");
  setfill(0x00, 0xFF, 0x00, 1);
  drawcmd(shell.fontpath, 48, shell.width - 700, shell.height - 48, "date", 0, 0);
  setfill(0xFF, 0xCC, 0x33, 1);
  drawcmd(shell.fontpath, 64, shell.width - 700, shell.height - 64 - 48, "cat /proc/uptime", 0, 0);
  // consider stats for total textsize and total fontcache
}

VD signalhandler(IN signo) {
//  Ms("[SHELL <- ^C]");
//  IN stdinfileno = fileno(stdin);
//  M1("{fileno:%d}", stdinfileno);
//  IN stdinflags = fcntl(stdinfileno, F_GETFL, 0);
//  stdinflags |= O_NONBLOCK;
//  fcntl(stdinfileno, F_SETFL, stdinflags);
//  IF (stdinflags & O_NONBLOCK) {
//    stdinflags -= O_NONBLOCK;
//    fcntl(stdinfileno, F_SETFL, stdinflags);
//  } EL { Gs("{shell:nonblockASEXPECTED}"); }
  W1("[signo: %d]", signo);
  finish();
  Cs("[ENDGRAPHICS]");
  destroyglyphfacelib();
  Ms("[FREEGLYPHS]");
  reblockstdin();
  Cs("[ENDSHELL]");
  pointerstop(&shell.pointer);
// ^ fails to close ... what happens if I leave it open ?
  Ys("[FINISH]");
  _; _exit(0);
}

IN drawshell($) {
//  signal(SIGINT, SIG_IGN); // interrupthandler);
  signal(SIGINT, signalhandler); // interrupthandler);
  signal(SIGHUP, signalhandler); // interrupthandler);
  signal(SIGTERM, signalhandler); // interrupthandler);
// ^ otherwise ^C is handled by sending SIGHUP ?
  // ignore ^C since leaving non-blocking enabled exposes a major su bug
  // use ^D instead ........... (pointer still fails to close)
  CHDIRORFAIL(RAMDIR) { RT1("CHDIRRAMDIR"); }
  FS ramfont = OPENCMD("intoram /font/ukai.ttf");
  CLOSECMD(ramfont);
  shell.fontpath = "/dev/shm/ukai.ttf";
//  atexit(restoreterm);
//  saveterm();
//  signal(SIGINT, signalhandler);
//  signal(SIGHUP, signalhandler);
//  signal(SIGTERM, signalhandler);
//  signal(SIGSEGV, signalhandler);
//  catchsignals();
//  rawterm();
//  IN pid = setsid();
//Ys("PID");
//Mi(pid);
Gs("START");
  shell.pagewidth = 0;
  shell.pageheight = 0;
  runsizecmd("pagesize", &shell.pagewidth, &shell.pageheight);
  shell.gridwidth = 0;
  shell.gridheight = 0;
  runsizecmd("gridsize", &shell.gridwidth, &shell.gridheight);
  initSize(0, 0, shell.pagewidth, shell.pageheight);
  shell.layer = 0;
  shell.opacity = 200;
Ys("INIT");
  init(shell.layer, &shell.width, &shell.height, shell.opacity);
Ms("GLYPHS");
  initglyphlibface(shell.fontpath);
Cs("POINTER");
  pointerinit(&shell.pointer);
//  savescreenreader(NULL);
Gs("INIT");
  // ------------------ FIRST FRAME -----------------------------------------------------------------
  start();
  IF ($N GT 0 AND STREQ2($1, "-testpng", "-testrle")) {
    setbackground(0, 0, 0, 1); // FULLBLACK    0, 0, 0, 1
Ws("-->");
    shell.stream = savescreenstream(0, 0, shell.width, shell.height);
Ws("<--");
// could check that it compresses to 19 bytes
//    QUICKCMD("ls -lh /V:/?*");
//    CH toggle = 0;
    FP lastuptimeval = 0.0;
    LOOP {
      setbackground(0, 0, 0, 0.5); // FULLBLACK    0, 0, 0, 1
//      IF (toggle = !toggle) {
//        setfill(0xFF, 0xCC, 0x33, 1); // FULLFC3   0xFF, 0xCC, 0x33, 1
//      } EL { setfill(0x00, 0xCC, 0x33, 1); }
      LINESCRIPT(uptimevar, "cat /proc/uptime");
      FP uptimeval = atof(uptimevar);
      FP uptimediff = uptimeval - lastuptimeval;
      lastuptimeval = uptimeval; // for diff value
      LINESCRIPT(lastfilesizevar, "stat -c %%s /V:/+1.rle");
      IN rawfilesize = 1920 * 1080 * 4;
      IN lastfilesize = -1;
      FS lastfile = OPENFILE("/V:/+1.rle");
      IF (lastfile) {
        GOTOEOF(lastfile);
        LN lastsize = GETFPOS(lastfile);
        CLOSEFILE(lastfile);
        lastfilesize = (IN)lastsize;
      }
//      IN lastfilesize = atoi(lastfilesizevar);
//      IN uptimelastdigit = *(uptimevarend - 1);
//      IF INRANGE(uptimelastdigit, A0, A9) {
//        uptimelastdigit -= A0; // 0 - 9
//      } EL { Rs("bad uptime digit"); RT 0; }
      IN posx = 200; //(shell.width  - 500) / (uptimelastdigit + 1);
      IN posy = 800; // (shell.height - 120) / (uptimelastdigit + 1) + 120;
//      FP kblastfilesize = (FP)lastfilesize / 1024.0;
      FP mbrawfilesize = (FP)rawfilesize / 1024.0 / 1024.0;
      FP uptimefps = 1.0 / uptimediff;
      FP compressionpct = (100.0 * (rawfilesize - lastfilesize)) / rawfilesize;
      STRF(uptimevar, "\n%.3ffps (%.3fsec/frame)"
                      "\n%.2fMB -> %dB"
                      "\n(%.2f%% compressed)",
        uptimefps, uptimediff,
        mbrawfilesize, lastfilesize, compressionpct);
//      STRENDP(uptimevarend, uptimevar);

      IF (uptimefps LT 1.0) {   // slow speed
        setfill(0xFF, 0xCC, 0x33, 1);
      } EF (uptimefps GQ 2.0) { // turbo speed
        setfill(0xCC, 0x33, 0xFF, 1);
      } EL { setfill(0x00, 0xCC, 0x33, 1); }
      drawlines(shell.fontpath, 120, posx, posy, uptimevar);
      end();
      IF (updatescreenstream(shell.stream, 0) EQ 'c') {
        Rs("RLE cache file still exists !"); RT 0;
      } // updaterlescreenstream updatepngscreenstream
//      QUICKCMD("ls -lh /V:/?*");
//      shell.stream = savescreenstream(0, 0, shell.width, shell.height);
      start();
    }
    RT 0;
  }
  setbackground(0, 0, 0, 1); // FULLBLACK    0, 0, 0, 1
Ms("BG");
  setfill(0xFF, 0xCC, 0x33, 1); // FULLFC3   0xFF, 0xCC, 0x33, 1
  drawchars(shell.fontpath, 200, 100, 100, "Connected...");
  end();
Gs("FRAME");
//eYs("(stream)");
  shell.stream = savescreenstream(0, 0, shell.width, shell.height);
  // waits for shell reader to read the initial frame
  // sometimes the first frame shows as grey ....
  // sometimes each frame times out but the connection stays open...
  // sometimes it's better to have one connection per frame
eGs("(init)");
  // ------------------------------------------------------------------------------------------------
  Parameter *spr = NULL;
  spr = lfaddparam(spr, "S1.WIDTH",   (LF){  shell.width, 0 }, "px");
  spr = lfaddparam(spr, "S1.HEIGHT",  (LF){ shell.height, 0 }, "px");
  spr = lfaddparam(spr, "S1.BOUNDL",  (LF){          720, 0 }, "degh");
  spr = lfaddparam(spr, "S1.BOUNDR",  (LF){         -720, 0 }, "degh");
  spr = lfaddparam(spr, "S1.BOUNDT",  (LF){          720, 0 }, "degv"); // or flip
  spr = lfaddparam(spr, "S1.BOUNDB",  (LF){         -720, 0 }, "degv");
  shell.parameters = spr;
// should draw to screen as debug for now
  shell.textfiles = NULL;
//  WI ($N GT 0) .. id:filename
//    { addtextbox(&textfiles, OPENFILE($1)); $$1; }
  // ------------------------------------------------------------------------------------------------
  FP ctxtheight = shell.height / 9.0f;
  CH ctxt = -1;
  WI (INC ctxt LQ 8) { // contexts 0-8 for now...
    Context *c = &shell.context[ctxt];
    c->index = ctxt;
//    c->name = " "; none because shell doesn't handle login
//    c->id = "overflow";
//    c->label = "OVERFLOW";
    c->fontpath = shell.fontpath;
//    c->fontsize = 64;
    // later: alignment
    IF (ctxt EQ 0) { c->box = (XYWH){ 100, shell.height - 100, 0, 0 }; } // size will be updated by text draw functions
    EL { c->box = (XYWH){ shell.width / 2, shell.height - (ctxt * ctxtheight), 0, 0 }; } // context 1-9 rows (probs 9-1)
    c->namebox = (XYWH){ 10, 10, 80, 10 };
// parambox
// mapbox
    c->curve = (XY){ 16, 16 }; // same as mouse
    c->textsize = 1024;
    c->text = CHMEM(c->textsize + 1);
    c->texti = 0;
    c->text[c->texti] = NUL;
    c->visible = 1;
    IF (ctxt EQ 0) { c->colour = (RGB){ 0xFF, 0xCC, 0x33 }; }
    EF (ctxt EQ 1) { c->colour = (RGB){ 0xFF,    0, 0    }; }
    EF (ctxt EQ 2) { c->colour = (RGB){ 0xFF, 0xFF, 0    }; }
    EF (ctxt EQ 3) { c->colour = (RGB){    0, 0xFF, 0    }; }
    EF (ctxt EQ 4) { c->colour = (RGB){    0, 0xFF, 0xFF }; }
    EF (ctxt EQ 5) { c->colour = (RGB){    0,    0, 0xFF }; }
    EF (ctxt EQ 6) { c->colour = (RGB){ 0xFF,    0, 0xFF }; }
    EF (ctxt EQ 7) { c->colour = (RGB){ 0xFF, 0xFF, 0xFF }; }
    EF (ctxt EQ 8) { c->colour = (RGB){ 0xAA, 0xAA, 0xAA }; }
    EF (ctxt EQ 9) { c->colour = (RGB){ 0x77, 0x77, 0x77 }; }
    c->opacity = 0.9;
    c->bdcolour = (RGB){ 0xFF, 0xCC, 0x33 };
    c->bdvisible = 1;
    c->bdopacity = 0.1f * ctxt;
    c->bgvisible = 1;
    c->bgcolour = c->colour;
//    c->bgcolour = (RGB){ 0x77, 0x77, 0x77 };
    c->bgopacity = 0.3;
    c->selectionlist = NULL;
    c->selectionindex = 0;
    c->selnumitems = 0;
    c->selitemsperrow = 16; // used by arrow keys
    IN centrex = shell.width / 2;
    IN centrey = shell.height / 2;
    IN smallw = 100;
    IN smallh = 100;
//    IN smalll = centrex - (smallw / 2);
//    IN smallt = centrey - 
    Parameter *cpr = NULL;
    cpr = lfaddparam(cpr,      "FONTSIZE",  (LF){           64, 0 }, NULL);
    cpr = lfaddparam(cpr, "LABELFONTSIZE",  (LF){           24, 0 }, NULL);
    cpr = lfaddparam(cpr, "POINTFONTSIZE",  (LF){           16, 0 }, NULL);
    cpr = lfaddparam(cpr,         "LABEL",  (LF){            0, 0 }, "`");
    cpr = lfaddparam(cpr,      "SCREEN.W",  (LF){  shell.width, 0 }, NULL);
    cpr = lfaddparam(cpr,      "SCREEN.H",  (LF){ shell.height, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.L",  (LF){ centrex - 50, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.T",  (LF){ centrey - 50, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.W",  (LF){          100, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.H",  (LF){          100, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.X",  (LF){            0, 0 }, NULL); // left ?
    cpr = lfaddparam(cpr,       "MOUSE.Y",  (LF){            0, 0 }, NULL); // top ?
    cpr = lfaddparam(cpr,       "M1.XFOCUS",   (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.YFOCUS",   (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.XOFFSET",  (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.YOFFSET",  (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.XVALUE",   (LF){ 0, 0 }, "~"); // need initial values
    cpr = lfaddparam(cpr,       "M1.YVALUE",   (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETL",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETR",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETW",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETT",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETB",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETH",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELL", (LF){ ctxt * 10, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELT", (LF){         0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELW", (LF){        10, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELH", (LF){        10, 0 }, "~"); // ..ignore them later
    c->parameters = cpr;
//lflistparams(c->parameters);
  }
  shell.saveonchangetext = 1;
  shell.saveonchangespin = 1;
  shell.texthaschanged = 0;
  shell.spinhaschanged = 0;
  shell.spinstr[0] = '-';
  shell.spinstr[1] = NUL;
  shell.ctxti = 0; // use overflow context by default
//  IN changes = 0;
//  IN everyframe = 1; // saves input EOF (spin) frames also
//  CH cmd = EOF; // should not become EOF in blocking mode
//  IN stdinfileno = fileno(stdin);
//  M1("{fileno:%d}", stdinfileno);
//  IN stdinflags = fcntl(stdinfileno, F_GETFL, 0);
//  stdinflags |= O_NONBLOCK;
//  fcntl(stdinfileno, F_SETFL, stdinflags);
//  Rs("{shell:nonblockcancelled}");
  unblockstdin();
  IN stdineofcount = 0;  // EOF counter (1s delay before next read attempt)
  IN stdinidlecount = 9; // number of EOFs required to generate an idle frame
  IN ch = NUL; // non-blocking mode no longer accepts chars after EOF (no idea why)
  LOOP {
//    ch = GETSTDCH;
    IF (ch NQ ':') { // : could be due to CT and will not get stuck since it reads
//      ch = getchar();
      errno = 0; // in case EOF is received without errno being set !
      ch = GETSTDCH;
      IF (ch EQEOF) {
        IF (errno EQ 0 OR errno EQ EWOULDBLOCK) {
          Cxc(48); //          Cs("shelleof...");
          WAIT100MS; // 2 seconds before \n idle frame
          IF (INC stdineofcount GQ stdinidlecount)
            { Cxc(96); ch = '\n'; stdineofcount = 0; }
          // send idle frame but only update spinner rather than whole image ....
          EL { NOP; } // continue with EOF to trigger spin ....... CT; } // else try another read
        } EF (errno EQ 5) {
          Rs("IOEOF ");
          WAIT500MS;
        } EL {
          Rs("SHELLEOF...");
          Y1("errno: %d ", errno);
//        IF (errno EQ 0 OR errno EQ EWOULDBLOCK) {
          BK; // continue with EOF to trigger spin
        } // unexpected error, close shell
      } EFEQ2(ch, '`', NUL) { // ` grave currently used to send an idle signal !
        Mxc(48); // unexpected unless importing data
        WAIT100MS;
        IF (INC stdineofcount GQ stdinidlecount)
          { Mxc(96); ch = '\n'; stdineofcount = 0; }
        EL { ch = EOF; NOP; } // use EOF to trigger spin ... no CT
//        SLEEP1S; // to be used by shell's host process for idle frames !
//        CT; // skip NUL since it was unexpected
      } EL { stdineofcount = 0; } // 5 consecutive EOF/NULs should trigger an idle frame
    }
    // a : will interrupt the input stream and read a whole command, perhaps ending in \n ?
    // need a replacement char for :

//C2("ch=%d(%c)", ch, (ch GQ 32) ? ch : ' ');

//Mi(ch); Wc(':');

    // \e1:VALUE=10:VALUE2=20:KEYS\n
    IF (ch EQ ':') { // SET PARAMETER (should have been preceded by \eCTXT) -------------------
      Context *c = &shell.context[shell.ctxti];
      // expect parameter
      CH parametername[NAMESIZE + 1];
      CS pname = parametername;
      WI (ch NQ '=') {
        ch = GETSTDCH;
        BKEQEOF(ch);
        IFEQ(ch, '=') { BK; }
        *pname = ch;
        INC pname;
        *pname = NUL;
      }
      IFEQ(ch, '=') { // this is a long float number
//Y1("%s=", parametername);
        LF lfvalue = lfreadfslf(stdin, &ch); // reads until ;, \n or any non-number char
        c->parameters = lfsetlfparam(c->parameters, parametername, lfvalue);
//Gs("=value");
//Mlf(lfvalue);
//Ylf(lfgetlfparam(c->parameters, parametername));
         // if parameter does not exist, addparam will be called by setparam
         // each call adds a new value to the list
      } EFEQ(ch, '|') { // this is a string of text
        CH strvalue[NAMESIZE + 1]; // username, code .... nothing longer than a variable name
        lfreadfscs(stdin, strvalue, &ch); // reads until ; or \n
        c->parameters = lfsetcsparam(c->parameters, parametername, strvalue);
      } EL { eRs(":param without =value or |string"); }
      IFEQ(ch, ';') { CT; } // check for another parameter / more input
      EFEQ(ch, ':') { CT; } // another parameter starts here ..
  //    EFEQ(ch, '\n') { } // input ends, time draw response frame
      EFNQ(ch, '\n')
        { eR2(":param=value ends in A%d (%c)", ch, ch); } // \e1:BLA=1;:X=2,1;:Z=1,2;KEYS\n
    } EF (ch EQ EOF) {
      // displayed cyan earlier
//      Gxc(48); // idle frame to be processed
    } EF (ch EQ NUL) {
      Gs("NUL"); // todo: wut ?!
    } EF (ch EQ '`') {
      Gs("NOP"); // todo: wut ?!
    } EF (ch EQ '\n') {
      Mxc(48); // full frame to be processed
    } // EL { G1(":%d:", ch); }
    // ----------------------------------------------- END OF SET PARAMETER ---------
    // :Param=Value; handled above - CT each ; .... keys and \n (draw) below ...
    // ------------------------------------------------ TIME TO DRAW -------------
    start();
    // draw the pointer - note that this is under everything ! draw an over pointer later ?
    // ----------------------------------------- MOUSE -----------------------------------------------------------------------------
    drawinfobox(); // behind pointer
    pointerupdate(&shell.pointer);
    RGB pointerbordercolour = { (shell.pointer.hasleftclicked  ) ? 0xFF : 0x77,
                          (shell.pointer.hasmiddleclicked) ? 0xFF : 0x77,
                          (shell.pointer.hasrightclicked ) ? 0xFF : 0x77 };
    FP pointerborderwidth = 2.5f; // border width
    CH spinclick = shell.pointer.hasclicked;
    IF (shell.pointer.hasmoved OR shell.pointer.hasclicked) { // IF (pointer -> haschanged
      Pointer *p = &shell.pointer;
      pointerhandlemove(p);
//      spinclick = p->hasleftclicked;
      pointerhandleclick(p);
      rgbaroundrect(p->mousebox, p->mousecurve, 1, pointerbordercolour, 0.5, pointerborderwidth, 0, (RGB){ 0, 0, 0 }, 0);
      
//      setfill(0x33, 0x33, 0x33, 1);
//      drawroundrect(shell->pointer->mousel, shell->pointer->mouseb, shell->pointer->mouser - shell->pointer->mousel,
//                                                                    shell->pointer->mouset - shell->pointer->mouseb, 16, 16);
//      setfill(red, 0x77, 0x77, 1);
//      drawroundrect(shell->pointer->mousel - 16.0f, shell->pointer->mouseb - 16.0f, 32.0f, 32.0f, 16, 16);
//      setfill(0x77, 0x77, blue, 1);
//      drawroundrect(shell->pointer->mouser - 16.0f, shell->pointer->mouset - 16.0f, 32.0f, 32.0f, 16, 16);
    }
    // ------------------------------------------------------ SPIN ----------------------------------------------------------
    IF (spinclick) {
//    IF (shell.pointer
      setfill(0x00, 0xFF, 0xFF, 1);
      ch = '\n'; // trigger frame refresh
    } EF (ch EQ '\n') {
      setfill(0xFF, 0xCC, 0x33, 1);
    } EL { setfill(0xFF, 0xCC, 0x33, 1); } // spinsmallsize same colour
//    } EL { setfill(0x00, 0xFF, 0x00, 1); }
    IN spinsmallsize = 30;
    IN spinsize = (ch EQ '\n') ? 100 : spinsmallsize;
    IN spinleft = 100;
    IN spintop  = 100; // actually bottom
    CS spinstr = (spinclick) ? "*" : spin(shell.spinstr);
    drawchars(shell.fontpath, spinsize, spinleft - 25, spintop - 40, spinstr);
    shell.spinhaschanged = 1;


    IF (ch EQEOF) {
      end(); // end the drawing to trigger an update
      CT; // continue after non-blocking EWOULDBLOCK delay .....
//    } EF (ch EQ '\r') { // draw spin but don't redraw everything ? need to clear underneath - replace certain box
//      currently no partial frame refresh, spin draws anyway
    } EF (ch EQ '\n') { // buffer only clears on \n - use ; to delimit actions and text "\e5:FONTSIZE=32;hi"
//eCs("(drawing)");
//      start();
      // ----------------------------------------- INFORMATION ------------------------------------------------------------------------      
      setbackground(0, 0, 0, 1.0);
      // setfill still set from earlier ....
//      setfill(0x77, 0x77, 0x77, 1);
      drawchars(shell.fontpath, spinsize, spinleft - 25, spintop - 40, spinstr);
      setfill(0x77, 0x77, 0x77, 1);
      drawchars(shell.fontpath, spinsmallsize, spinleft, spintop, spin(shell.spinstr));
      drawchars(shell.fontpath, spinsmallsize, spinleft, spintop, spin(shell.spinstr));
      drawchars(shell.fontpath, spinsmallsize, spinleft, spintop, spin(shell.spinstr));
      drawchars(shell.fontpath, spinsmallsize, spinleft, spintop, spin(shell.spinstr));

      drawinfobox();

      // ------------------------------------------------------ TEXT FILES ----------------------------------------------------------
//      TextboxList *tb = textfiles;
//      VGfloat y = 1000.0f;
//      WI (tb) {
//        setfill(0x33, 0xFF, 0xFF, 1); // use diff colours..
// get box position from tb later
//        y += drawfile(shell->fontpath, 20, 0, y, tb->file);
//        tb = tb->next;
//      }
// each needs a region !
      // ------------------------------------------------------ TEXT BUFFERS -------------------------------------------------------
      // draw input text in yellow, size 128: later use % of screen as guide
      IN ctxtj = -1;
      WI (INC ctxtj LQ 9) {
        Context *c = &shell.context[ctxtj];
        RGB bdcolour = c->bdcolour;
        IF (ctxtj EQ shell.ctxti)
          { bdcolour = (RGB){ 0xFF, 0xCC, 0x33 }; }
        IF (c->visible) { // should probably allow invisible-drawing to calculate size (no font loading)
          IN cfontsize = (lfgetlfparam(c->parameters, "FONTSIZE")).numb;
          IN clabelfontsize = (lfgetlfparam(c->parameters, "LABELFONTSIZE")).numb;
          IN cpointfontsize = (lfgetlfparam(c->parameters, "POINTFONTSIZE")).numb;
          CS clabel    = lfgetcsparam(c->parameters, "LABEL");
          IN cscreenw  = (lfgetlfparam(c->parameters, "SCREEN.W")).numb;
          IN cscreenh  = (lfgetlfparam(c->parameters, "SCREEN.H")).numb;
//          IN cpointfontsize = (lfgetlfparam(c->parameters, "POINTFONTSIZE")).numb;
          IN cmousex   = (lfgetlfparam(c->parameters, "MOUSE.X")).numb; // treat x as left to right (normal)
          IN cmousey   = (lfgetlfparam(c->parameters, "MOUSE.Y")).numb; // treat y as top to bottom (invert)
          IN cmousew   = (lfgetlfparam(c->parameters, "MOUSE.W")).numb; // box width
          IN cmouseh   = (lfgetlfparam(c->parameters, "MOUSE.H")).numb; // box height
          // draw text box
          XYWH cosbox = { c->box.x + cmousex, c->box.y - cmousey, 0, 0 }; // invert y, discard w/h (recalc)
          rgbatextlines(c->fontpath, cfontsize, c->text, &cosbox, 0, c->colour, 0.0f); // (visible=0) update box .w, .h only
          XYWH cbdbox = { cosbox.x - 3, cosbox.y - 3, cosbox.w + 6, cosbox.h + 6 };
          cbdbox.y SUBS cbdbox.h; // offset height due to vertical flip
          rgbaroundrect(cbdbox, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          rgbatextlines(c->fontpath, cfontsize, c->text, &cosbox, c->visible, c->colour, c->opacity); // updates box .w, .h
          c->box.w = cosbox.w; c->box.h = cosbox.h; // updated by rgbatextlines (both times)
          // draw name box
          XYWH labelbox = { cbdbox.x, cbdbox.y + cbdbox.h, 0, 0 };
          rgbatextlines(c->fontpath, clabelfontsize, clabel, &labelbox, 0, c->colour, 0.0f); // update .w, .h
          XYWH lbdbox = { labelbox.x - 1, labelbox.y - 1, labelbox.w + 2, labelbox.h + 2 };
          lbdbox.y SUBS lbdbox.h; // vertical flip
          lbdbox.x SUBS lbdbox.w; // offset left
          rgbaroundrect(lbdbox, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          rgbatextlines(c->fontpath, clabelfontsize, clabel, &labelbox, c->visible, c->colour, c->opacity); // updates .w, .h again
          // c->namebox.w = namebox.w; c->namebox.h = namebox.h
          // .... also draw param values
          XYWH mousebox = { cbdbox.x + cbdbox.w, cbdbox.y - cmouseh, cmousew, cmouseh };
          rgbaroundrect(mousebox, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          IN mpointx = cmousex * cmousew / cscreenw;
          IN mpointy = cmousey * cmouseh / cscreenh;
          RGB mpointc = { 0x00, 0xFF, 0x00 };
          XYWH mpoint = { mousebox.x + mpointx, mousebox.y + cmouseh - mpointy, 0, 0 };
          CS mpointchar = "\xE2\x97\x86"; // filled diamond
//          rgbatextpoint(c->fontpath, cpointfontsize, mpointchar, &mpoint, 0, mpointc, 0.0f);
// ^ no need to calculate size .........................
          rgbatextpoint(c->fontpath, cpointfontsize, mpointchar, &mpoint, c->visible, mpointc, c->opacity);
// count how many mouse1 values exist
          // recalculate map pixel bounds before each user's is drawn
          lfupdatemapcoords(shell.parameters, c->parameters);
          IN m1pixell   = (lfgetlfparam(c->parameters, "M1.PIXELL")).numb;
          IN m1pixelt   = (lfgetlfparam(c->parameters, "M1.PIXELT")).numb;
          IN m1pixelw   = (lfgetlfparam(c->parameters, "M1.PIXELW")).numb;
          IN m1pixelh   = (lfgetlfparam(c->parameters, "M1.PIXELH")).numb;
          LF s1boundl   = lfgetlfparam( c->parameters, "S1.BOUNDL");
          LF s1boundt   = lfgetlfparam( c->parameters, "S1.BOUNDT");
          LF s1incremx  = lfgetlfparam( c->parameters, "S1.INCREMX");
          LF s1incremy  = lfgetlfparam( c->parameters, "S1.INCREMY");
          XYWH m1box = { m1pixell, m1pixelt, m1pixelw, m1pixelh };
          rgbaroundrect(m1box, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          LFList *m1xvalues = lfgetparam(c->parameters, "M1.XVALUE"); // need XPIXEL XPIXEL ....
          LFList *m1yvalues = lfgetparam(c->parameters, "M1.YVALUE"); // need YPIXELS ... -S
          XYWH m1xlabel = { m1box.x,         m1box.y         , 200, clabelfontsize };
          XYWH m1ylabel = { m1box.x, m1box.y + clabelfontsize, 200, clabelfontsize };
          // ^ w and h ignored but could be used to pre-draw a background box
          RGB m1labelclr = { 0x00, 0xFF, 0xFF };
          RGB m1pointclr = { 0xFF, 0xFF, 0x00 };
          IN m1xtotal = lfvaluescount(m1xvalues);
          IN m1ytotal = lfvaluescount(m1yvalues);
          IF (m1xtotal NQ m1ytotal)
            { Rs("(x1 != y1)total"); }
          CH m1total[100];
          sprintf(m1total, "%d", m1xtotal);
          rgbatextpoint(c->fontpath, clabelfontsize, m1total, &m1xlabel, c->visible, m1labelclr, c->opacity);
          IN m1ix = -1;
          WI (INC m1ix LT m1xtotal) {
            LF m1valuex = lfvalueatindex(m1xvalues, m1ix);
            LF m1valuey = lfvalueatindex(m1yvalues, m1ix);
            LF m1pixelx = LFMUL(LFSUB(m1valuex, s1boundl), s1incremx);
            LF m1pixely = LFMUL(LFSUB(m1valuey, s1boundt), s1incremy);
            XYWH m1point = { m1pixelx.numb, m1pixely.numb, 0, 0 }; // no size, centre
            CS m1pointchar = "\xE2\x97\x86"; // filled diamond
            rgbatextpoint(c->fontpath, cpointfontsize, m1pointchar, &m1point, c->visible, m1pointclr, c->opacity);
          }
//lflistparams(c->parameters);
        }
      } // redraw on line break only rather than after every char - need linebreak to flush anyway - extra linebreaks won't trigger change
//        setfill(of->colour.r, of->colour.g, of->colour.b, of->opacity);
//        drawlines(shell.fontpath, 128, 300, 800, of->text);
      // ------------------------------------------------------ SELECTION BOXES -----------------------------------------------------        
      // each user sees differently ... need to save variants
      ctxtj = -1;
      WI (INC ctxtj LQ 9) {
        Context *c = &shell.context[ctxtj];
        IF (c->selectionlist) {
//        IN selnumitems = getlistsize(selectionlist);
//        IF (of->selecting EQ Ay) {
          ItemBox *ib = createitembox(c->selectionlist, c->selectionindex, shell.fontpath, 32, 128, c->selitemsperrow, c->selnumitems, 400, 500);
          drawitembox(ib);
          UNMEM(ib);
        }
      }
      // ----------------- REDRAW POINTER SINCE SCREEN WAS CLEARED --- ABOVE ALL EXCEPT SPIN BELOW -----------------
      IF (1) { // always draw pointer ....  (shell.pointer.hasmoved OR shell.pointer.hasclicked) { // IF (pointer -> haschanged
        Pointer *p = &shell.pointer;
//        pointerhandlemove(p);
//        pointerhandleclick(p);   // handled earlier for EOF w/o redraw
        RGB pointerframecolour = { 0xFF, 0xCC, 0x33 };
        rgbaroundrect(p->mousebox, p->mousecurve, 1, pointerframecolour, 0.5, pointerborderwidth, 0, (RGB){ 0, 0, 0 }, 0);
      }
      // ------------------ FILE SAVE - NEEDS SELECTION BOX VARIANTS --------------------------------------- 
      IF ((shell.texthaschanged AND shell.saveonchangetext) OR (shell.spinhaschanged AND shell.saveonchangespin)) {
//eCc('[');
//eYs("stream");
// update spin in background while waiting ...... detect 'c' state !!
        IN streamid = shell.stream->id;
        CH streamindex[20];
        STRF(streamindex, " %d", streamid);
        setfill(0xFF, 0xCC, 0x33, 1);
        drawchars(shell.fontpath, spinsmallsize, spinleft - spinsmallsize + (streamid * spinsmallsize), spintop, streamindex);
        end(); start(); // draw frame now
        CH flashtoggle = 0;
        WI (updatescreenstream(shell.stream, 0) EQ 'c') {
          IF (flashtoggle = !flashtoggle) {
            setfill(0xFF, 0x00, 0xFF, 1);
          } EL { setfill(0xFF, 0xCC, 0x33, 1); }
          drawchars(shell.fontpath, spinsmallsize, spinleft - spinsmallsize + (streamid * spinsmallsize), spintop, streamindex);
//        setfill(0x00, 0xFF, 0x00, 1);
//          drawchars(shell.fontpath, spinsize, spinleft + 20, spintop, streamindex);
          end();
          WAIT200MS;
          start();
        }
        setfill(0x00, 0xFF, 0x00, 1); // assuming square string for left offset
        drawchars(shell.fontpath, spinsmallsize, spinleft - spinsmallsize + (streamid * spinsmallsize), spintop, streamindex);
        setfill(0x00, 0xFF, 0xFF, 1);
        drawchars(shell.fontpath, spinsize, spinleft - 25, spintop - 40, spinstr);
//eGs("update");
//eCs("]\n");
//        IF (changes)
//          { updatescreenstream(stream); }
//        EL { update0screenstream(stream); }
//        savescreenshot(0, 0, width, height);
        shell.texthaschanged = 0;
        shell.spinhaschanged = 0;
      }
      end();

//Cc('@');
//      sched_yield();
//      SLEEP100MS; // 10 FPS FRAME LIMITER!!!!!!!! ENCODING WILL SLOW FURTHER
//      SLEEP10MS; // max 100fps-ish framerate
    } EL { // IF ch NQ '\n' ------------------------------------------------------------------
      IF (ch EQEOF) { Rs("MATCHEOF"); BK; } // EOF should have been handled earlier for non-blocking
//        eRs("(shell input eof)"); BK; } // WAIT500MS; CT; }
      // switching to normal terminal mode means no EOF is sent, but when used by popen, it seems to
      EF INRANGE(ch, ' ', '~') { eM1("%c", ch); }
      // EL { eR1("(%d)", ch); }
      // IF (ch IS ESC then read next char in case of context change \e1 \e2 \e3
      // input packets should (but don't have to) end with \e0 to reset context to overflow
      // \n\n\n\n should trigger spin change but no text change
      shell.texthaschanged = 1; // consider flagging for each context
      Context *c = &shell.context[shell.ctxti];
//Yi(ch); Mc('!');
      IF (ch EQ 2) {
//        eRs("Ctrl+C");
        Ys("[SHELL <- ^C]");
        BK; // end shell
//        signalhandler(SIGTERM);
//        _exit(0);
      } EF (ch EQ 3) { // ctrl + C or ctrl + D
//        eRs("Ctrl+D");
        Ms("[SHELL <- ^D]");
        BK; // end shell
//        signalhandler(SIGTERM); // not actually SIGTERM, just emulating the code
//        _exit(0);
  // later: interrupt savescreenid loop ...
//      } EF (ch EQ 9) { // TAB
//      } EF (ch EQ 10) { // NEWLINE
//      } EF (ch EQ 27) { // escape sequence ! perhaps remember it, eat it, apply the parameter instead ... handled below
      } EF (ch LT ' '
            AND ch NQ Aesc AND ch NQ Atab AND ch NQ Abackspace
            AND ch NQ Anewline AND ch NQ Acarriage) {
//      } EF (ch LT 32 AND ch NQ 9 AND ch NQ Anewline AND ch NQ Aesc) {
        R1("[SHELL <- control code NOT \\e \\t \\b \\n \\r: %d]", ch);
        BK; // end shell
//        _exit(0);
//        signalhandler(SIGTERM);
      // ------------------- BACKSPACE --------------------------------------------------------------------------------
//      } EF (ch EQ 127) { // backspace (or is this delete ? not \b? 8?) 
      } EF (ch EQ Abackspace OR ch EQ 127) { // 8 from pipe, 127 from keyboard
        IF (c->texti GT 0) // prevent over-backspace
          { c->text[DEC c->texti] = NUL; }
      } EF (ch EQ '\r') { // since \n is used to send input when canonical
        WRITEBUFCH(c->text, c->texti, c->textsize, '\r'); // can also write as \n
      // ---------------- ESCAPE SEQUENCES ----------------------------------------------------------------------------
      } EF (ch EQ Aescape) { // 27) { // }  AND selecting EQ Ay) { // ESC ESC to cancel selection box
        IN ch2 = GETSTDCH; // expect [
        IF (ch2 EQ '[') {
          IN ch3 = GETSTDCH; // expect char
          IF (INRANGE(ch3, AA, AZ) OR INRANGE(ch3, Aa, Az)) {
            // all escape sequences end in a lEtTer ?? ?! check ...
            IF (ch3 EQ AA) {    // up
              c->selectionindex SUBS c->selitemsperrow;
              IF (c->selectionindex LT 1)
                { c->selectionindex ADDS c->selitemsperrow; }
            } EF (ch3 EQ AB) {  // down
              c->selectionindex ADDS c->selitemsperrow;
              IF (c->selectionindex GT c->selnumitems)
                { c->selectionindex SUBS c->selitemsperrow; }
            } EF (ch3 EQ AC) {  // right
              INC c->selectionindex;
              IF (c->selectionindex GT c->selnumitems)
                { DEC c->selectionindex; }
            } EF (ch3 EQ AD) {  // left
              DEC c->selectionindex;
              IF (c->selectionindex LT 1)
                { INC c->selectionindex; }
            } EL {
              eBc('('); eYs("\\e"); eRi(-1); eBc(')'); // unexpected escape sequence
            }
          } EL {
            WI (!(INRANGE(ch3, AA, AZ) OR INRANGE(ch3, Aa, Az))) {
              // should continue reading until a valid end char is seen \e[0;31m \e[A
              eMi(99); ch3 = GETSTDCH; // check docs ...............import key list
            }
          }
        // =========== SET CONTEXT ==============================
        } EF (ch2 EQ Aescape) { // \e\e
          shell.ctxti = 0; // same as \e0\e
        } EF INRANGE(ch2, A0, A9) {
          IF (ch2 EQ A0) { // \e0nnnn\e
            IN contexti = 0;
            IN chn = GETSTDCH;
            WI (chn NQ '\e') {
              IF INRANGE(chn, A0, A9) {
                contexti = (contexti * 10) + (chn - A0);
//G1("contexti = %d", contexti);
              } EL { eR1("bad chn (%d)", chn); }
            }
            shell.ctxti = contexti; // c updates next iteration
          } EL { 
//G1("contexti = %d", ch2 - A0);
            shell.ctxti = ch2 - A0;
          } // \e1 - \e9
        } EL { eMi(88); } // bad escape sequence
      // -------------------------------------------------------------------------------------
      // ============ CONFIRM SELECTION ON SPACE =================
      } EF (ch EQ ' ' AND c->selectionlist NQNULL) {
        IN termi = c->texti;
        WI (termi GQ 0 AND c->text[termi] NQ '[')
          { DEC termi; } // find last [term] (to replace with selection value)
        IF (termi GQ 0 AND c->selectionindex GQ 0) { // confirm
          CS tbuf = &c->text[termi];
          List *lv = writelistvalue(c->selectionlist, c->selectionindex, tbuf);
          c->texti = termi + lv->length;
          lv->code[4] = '-';
          lv->code[5] = INCALPHANUM(lv->priority);
          lv->code[6] = NUL;
          QUICKCMD(lv->source);
          lv->code[4] = NUL;
          freeselectionlist(c->selectionlist);
          c->selectionlist = NULL;
        } EL { // lost track of [ or nothing selected
          c->text[c->texti] = ch; // ' '
          c->text[INC c->texti] = NUL;
          freeselectionlist(c->selectionlist);
          c->selectionlist = NULL;
        }
      // ============ OPEN SELECTION BOX ON ] =========================
      } EF (ch EQ ']') { // ] will begin a selection
        IF (c->selectionlist) {
          eRs("selection while selecting!");
          freeselectionlist(c->selectionlist); // will replace with new one .. whatever 
        }
        IN termi = c->texti;
        WI (termi GQ 0 AND c->text[termi] NQ '[')
          { DEC termi; }
        IF (termi GQ 0 AND c->text[termi] EQ '[') {
          CS term = &c->text[termi + 1];
          c->selectionlist = pinyinselectionlist(term);
          c->selnumitems = getlistsize(c->selectionlist);
          c->selectionindex = 1;
        } // EL failed to select, probably "bla]" ... watch out for "[bla] bla]"
        WRITEBUFCH(c->text, c->texti, c->textsize, ch);
      // =============== WRITE CHAR TO CURRENT CONTEXT BUFFER ==================
      } EL { WRITEBUFCH(c->text, c->texti, c->textsize, ch); }
      // consider extending buffer instead of overflow - copy, double size
    }
  }
  // ENDSHELL DOWN HERE PLZ
  Ys("[SHELLSTOP]");
  // disable non-blocking mode (very important !!)
  reblockstdin();
  signalhandler(0); // !! <- exits via here
// shouldn't happen ... maybe this was an issue (unblocking unblocked)

  Rs("[IMPOSSIBLE]");
//  stdinflags -= O_NONBLOCK;
//  fcntl(stdinfileno, F_SETFL, stdinflags);
//  Cs("[ENDSHELL]");
//  pointerstop(&shell.pointer);
// ^ fails to close ... what happens if I leave it open ?
//  Ms("[STOPPOINTER]");
//  finish();
//  Cs("[ENDGRAPHICS]");
//  destroyglyphfacelib();
//  Gs("[FINISH]");
  _; RT 0;
}

/*
  stoppointer(&shell.pointer);
  destroyglyphfacelib(); 
//  destroyglyphlib(ftlib);
//  IF (mouse) { stopmouse(mouse); }
//  restoreterm();
  finish(); // destroy graphics layer
  RT 0;
}
*/
