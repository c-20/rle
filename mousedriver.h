#define LEFTCLICK   1
#define MIDDLECLICK 4
#define RIGHTCLICK  2

FS startmouse() {
//  FS mouse = OPENCMD("mousedriver");
//  FS mouse = OPENCMD("mousdrv < /dev/input/mice");
  FS mouse = OPENSCRIPT("mousedriver");
  IN mfd = fileno(mouse);
  IN flags = fcntl(mfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(mfd, F_SETFL, flags);
  // check result ?
  Mxc(48); // signal that mouse has opened
  RT mouse;
}

VD stopmouse(FS mouse) {
  IF (mouse) {
    Cs("[RESETMOUSE]");
    IN mfd = fileno(mouse);
    IN flags = fcntl(mfd, F_GETFL, 0);
    IF (flags & O_NONBLOCK) {
      flags -= O_NONBLOCK;
      fcntl(mfd, F_SETFL, flags);
      // check result ?
    } EL { Rs("{mouse:expecnonblock}"); }
  //  WI (!feof(stream)) {
  //    IN ch = GETFCH(stream);
  //    IF (ch EQEOF) { Rxc(96); }
  //    Rc(ch);
  //  }
//    fflush(stream); // empty input ? or will it not ?
//    PID mousepid = childpid[mfd];
//    Gi(mousepid);
    
    Cs("[CLOSEMOUSE]");
//    CLOSEFILE(stream);
    CLOSESCRIPT(mouse);
//    WI (!feof(stream)) {
//      IN ch = GETFCH(stream);
//      IF (ch EQEOF) { Rxc(48); }
//      EL { Rc(ch); }
//    }
//    Cs("close stream..");
//    CLOSEFILE(stream);
//    CLOSECMD(stream);
    Ys("[MOUSECLOSED]"); // signal that mouse has closed
  } EL { Rs("{mouse:wasnotstarted}"); }
}

VD updatemouse(FS stream, IN *x, IN *y, IN *click) {
  IF (feof(stream)) { RLOG("FEOF"); RT; } // not expected
  IN clickcount = 0;
  IN ch = NUL;
  LOOP {
    ch = GETFCH(stream);
    IF (ch EQEOF) {
      IF (errno EQ EWOULDBLOCK) {
//        Yxc(96); // Ys("eofmouse...");
        BK; // no need for delay ... SLEEP100MS; BK; // !!!!!! end loop on wait signal
      } EL { Rs("{mouse:unexpeof}"); BK; }
    } EF (ch EQNUL) {
      Yxc(96); Wc('!');   // Ws("nulmouse...");
      SLEEP100MS;
    } EF (ch EQ '[') {
      CH closed = An;
      INP target = NULL;
      IN value = 0;
      CH negative = 0;
      WI (ch NQ '\n') { // (1) {
        ch = GETFCH(stream);
        IF (ch EQ '\n') { BK; }
        IF (ch EQEOF) { Rxc(96); Rc(Am); BK; } // unexpected EOF trying to read stream
        EF (ch EQ ']') { closed = Ay; }
        IF (ch EQ ',' OR ch EQ ']') {
          IF (target NQNULL)
            { *target = value; }
          target = NULL;
        } EL {
          IF (target EQNULL AND ch EQ 'x') { target = x;     }
          EF (target EQNULL AND ch EQ 'y') { target = y;     }
          EF (target EQNULL AND ch EQ 'c') { target = click; }
          EF (target EQNULL AND ch EQ 'n') {
            target = &clickcount; // internal
          } EF (target NQNULL AND ch EQ '=') {
            value = 0;
          } EF (target NQNULL AND ch EQ '-') {
            negative = 1; // val=- will be the same as val=0
          } EF (target NQNULL AND INRANGE(ch, A0, A9)) {
            value = (value * 10) + (ch - A0);
            IF (value GT 0 AND negative EQ 1)
              { value MULS -1; negative = 0; }
          } EL { Rc('!'); Yi(ch); Rc(Am); } // unexpected char
        }
      }
    } EL { Rc('['); Rc(Am); } // expected [
  }
}

typedef struct _Pointer {
  FS mousefile;
  XY mousepos;
  XY mouseprevpos;
  XY mouseoffset;
  XYWH mousebox;
  XY mousecurve;
  IN mouseclick;
  CH hasleftclicked;
  CH hasrightclicked;
  CH hasmiddleclicked;
  CH hasclicked;
  CH hasmovedup,   hasmoveddown;
  CH hasmovedleft, hasmovedright;
  CH hasmovedx,  hasmovedy;
  CH hasmoved;
} Pointer;

VD pointerinit(Pointer *p) {
  p->mousepos.x = 0;
  p->mousepos.y = 0;
  p->mouseprevpos.x = 0;
  p->mouseprevpos.y = 0;
  p->mouseoffset.x = -8;
  p->mouseoffset.y = -8;
  p->mousebox.x = 0 + p->mouseoffset.x;
  p->mousebox.y = 0 + p->mouseoffset.y;
  p->mousebox.w = 16;
  p->mousebox.h = 16;
  p->mousecurve.x = 16;
  p->mousecurve.y = 16;
  p->mouseclick = 0;
  p->hasleftclicked = 0;
  p->hasrightclicked = 0;
  p->hasmiddleclicked = 0;
  p->hasclicked = 0;
  p->hasmovedup = 0;
  p->hasmoveddown = 0;
  p->hasmovedleft = 0;
  p->hasmovedright = 0;
  p->hasmovedx = 0;
  p->hasmovedy = 0;
  p->hasmoved = 0;
  p->mousefile = startmouse();
  IF (!p->mousefile) { RLOG("NOMOUSE"); }
}

VD pointerupdate(Pointer *p) {
  p->mouseprevpos.x = p->mousepos.x;
  p->mouseprevpos.y = p->mousepos.y;
  updatemouse(p->mousefile, &p->mousepos.x, &p->mousepos.y, &p->mouseclick);
  p->mousebox.x = p->mousepos.x + p->mouseoffset.x;
  p->mousebox.y = p->mousepos.y + p->mouseoffset.y;
  p->hasleftclicked   = (p->mouseclick & LEFTCLICK  )  ? 1 : 0;
  p->hasrightclicked  = (p->mouseclick & RIGHTCLICK )  ? 1 : 0;
  p->hasmiddleclicked = (p->mouseclick & MIDDLECLICK)  ? 1 : 0;
  p->hasclicked = (p->hasleftclicked OR p->hasmiddleclicked OR p->hasrightclicked) ? 1 : 0;
  p->hasmovedup    = (p->mousepos.y LT p->mouseprevpos.y)  ? 1 : 0;
  p->hasmoveddown  = (p->mousepos.y GT p->mouseprevpos.y)  ? 1 : 0;
  p->hasmovedleft  = (p->mousepos.x LT p->mouseprevpos.x)  ? 1 : 0;
  p->hasmovedright = (p->mousepos.x GT p->mouseprevpos.x)  ? 1 : 0;
  p->hasmovedx = (p->hasmovedleft OR p->hasmovedright) ? 1 : 0;
  p->hasmovedy = (p->hasmovedup OR p->hasmoveddown)    ? 1 : 0;
  p->hasmoved  = (p->hasmovedx OR p->hasmovedright)    ? 1 : 0;
}

CH pointerhandlemove(Pointer *p) {
  CH flagstatus = p->hasmoved;
  p->hasmovedup   = p->hasmoveddown  = 0;
  p->hasmovedleft = p->hasmovedright = 0;
  p->hasmovedx    = p->hasmovedy     = 0;
  p->hasmoved = 0;
  RT flagstatus; // return status of flag, reset
}

CH pointerhandleclick(Pointer *p) {
  CH flagstatus = p->hasclicked;
  p->hasleftclicked = 0;
  p->hasrightclicked = 0;
  p->hasmiddleclicked = 0;
  p->hasclicked = 0;
  RT flagstatus;
}

VD pointerstop(Pointer *p) {
  IF (p->mousefile)
    { stopmouse(p->mousefile); }
}

