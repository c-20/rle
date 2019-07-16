// waitpid requires sys/wait.h
/*
#define READFD		0
#define WRITEFD		1
FS shellopen(CS command, CH type, INP pid) {
  // CS shellcmd = "/bin/sh";
  IF2NQ('r', 'w', type) { RT NULL; }
  IN closechildfd = READFD;
  IN closeparentfd = WRITEFD;
  IN usechildfd = WRITEFD;
  IN useparentfd = READFD;
  CS fdopentype = "r";
  IF (type EQ 'w') {
    closechildfd = WRITEFD;
    closeparentfd = READFD;
    usechildfd = READFD;
    useparentfd = WRITEFD;
    fdopentype = "w";
  }
  IN pdest[2];
  IF (pipe(pdest) NQ 0)  { RT NULL; }
  pid_t childpid = fork();
  IF (childpid EQ -1)
    { Rs("fork fail"); RT NULL; }
  IF (childpid EQ 0) {
    // do not close stdout
//    dup2(fd[closechildfd], READFD); // 
//    close(fd[closechildfd]);    // write-only child if read-only
// readfd -> stdout ?
//    close(pipefd[1]);
//    dup2(pipefd[0], 0);
  // redirect stdin, leave stdout open
//    dup2(fd[usechildfd], WRITEFD); // redirect stdout to pipe
//    setpgid(childpid, childpid); // so negative pids can wut

    close(pdest[1]);
    IF (pdest[0] NQ STDIN_FILENO) {
      dup2(pdest[0], STDIN_FILENO);
      close(pdest[0]);
    }
//    close(pipein[1]);
//    dup2(pipein[0], 0);
//    close(pipeout[0]);
//    dup2(pipeout[1], 1);
//Cs(command);
    CS argv[] = { "sh", "-c", command, NULL };
    execv(_PATH_BSHELL, argv);
// or use execlp if no path ...
    _exit(99); RT NULL;
  } // EL {
//    close(fd[closeparentfd]);  // read-only parent if read-only
//    dup2(fd[closeparentfd], WRITEFD);
//    dup2(fd[useparentfd], READFD);
W1("shellopened %d", childpid);
  *pid = childpid;
  FS infs = fdopen(pdest[1], "w");
//  close(pdest[0]);
  RT infs;
//    RT fdopen(pdest[1], "w");
//  RT fdopen(fd[useparentfd], fdopentype);
}

IN shellclose(FS shellfs, pid_t pid) {
  IF (fclose(shellfs) NQ 0)
    { RT -2; } // -2 if failed to close stream pointer
  IN status;
  WI (waitpid(pid, &status, 0) EQ -1) {
    IF (errno NQ EINTR)
      { status = -1; BK; }
  } // wait for process to end
  RT status;
}
*/


// later support unblocking/reblocking and raw/normal mode as a feature
// require <termios.h>


struct termios savedstdin;
struct termios *stdinsaved = NULL;
VD userawmodestdin() {
  IN stdinfile = fileno(stdin);
  IF (stdinsaved) { // restore previously saved setting before overwriting with this one
    tcsetattr(stdinfile, TCSANOW, stdinsaved); // restore
    stdinsaved = NULL; // indicate that the buffer is unused
    Gs("{oldstdinrestored}");
  }
  stdinsaved = &savedstdin; // use the one static memory buffer
  tcgetattr(stdinfile, stdinsaved);            // save
  struct termios rawstdin;
  memcpy(&rawstdin, stdinsaved, sizeof(struct termios)); // copy
//  rawstdin.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
  rawstdin.c_lflag &= ~(ICANON | ECHO);
//  rawstdin.c_cc[VTIME] = 0;
  rawstdin.c_cc[VMIN] = 1;
  Gs("[RAWSTDIN]");
  tcsetattr(stdinfile, TCSANOW, &rawstdin);    // make raw
}
VD restorenormalstdin() {
  IN stdinfile = fileno(stdin);
  IF (stdinsaved) { // restore previously saved setting before overwriting with this one
    tcsetattr(stdinfile, TCSANOW, stdinsaved); // restore
    stdinsaved = NULL; // indicate that the buffer is unused
    Gs("[NORMALSTDIN]");
  } EL { Gs("{stdinnotsaved}"); }
}

IN savedstdinflags;
IN *stdinsavedflags = NULL;
VD unblockstdin() {
  IN stdinfile = fileno(stdin);
  IF (stdinsavedflags) { // restore previously saved setting before overwriting with this one
    fcntl(stdinfile, F_SETFL, *stdinsavedflags); // restore
    stdinsavedflags = NULL; // indicate that the buffer is unused
    Gs("{oldstdinflagsrestored}");
  }
  stdinsavedflags = &savedstdinflags; // use the one static memory buffer
  *stdinsavedflags = fcntl(stdinfile, F_GETFL, 0);
  *stdinsavedflags |= O_NONBLOCK;
  fcntl(stdinfile, F_SETFL, *stdinsavedflags);
  Ms("[UNBLOCKSTDIN]");
}
VD reblockstdin() {
  IN stdinfile = fileno(stdin);
  IF (stdinsavedflags) {
    IF (*stdinsavedflags & O_NONBLOCK) {
      *stdinsavedflags -= O_NONBLOCK;
      fcntl(stdinfile, F_SETFL, *stdinsavedflags);
      Ms("[REBLOCKSTDIN]"); // NEED TO BLOCKRESTORE ON INTERRUPT TOO
    } EL { Rs("{shell:nonblockexpected}"); }
    stdinsavedflags = NULL;
  } EL { Gs("{stdinnotunblocked}"); }
}

#define PID   pid_t
#define PIDP  PID *

// 'l' (line) mode - run the command on each line of input for the args given
// 'm' (mesh) mode - run the command on each line of input using each of the args given
// 'p' (pipe) mode - run the command using input as stdin - should return a readable stream
// hopefully, that return stream can be passed as the input to the next command ....
// maybe better to just use r, R, w, W
FS openprogramscript(CS command, CH mode, PIDP pid, CS *args) { // input will be stdin, can pass fd but not file
  // use NULL command and argv as args
  // if command is NULL, args is argv
  // if command is not null, args will currently be ignored, maybe 'l' mode for line-by-line ?
  PID childpid;
  IN fds[2];
  pipe(fds);
  IN readfd = fds[0];
  IN writefd = fds[1];
  IF ((childpid = fork()) EQ -1)
    { perror("forkfail"); exit(1); }
  EF (childpid EQ 0) {
    IFEQ2(mode, 'r', 'R') {
      close(readfd);
      dup2(writefd, 1); // stdout -> writefd
    } EFEQ2(mode, 'w', 'W') {
      close(writefd);
//      dup2(readfd, 0);  // stdin  -> readfd
      dup2(0, readfd);    // readfd -> stdin
// dupes readfd as stdin ..... tis not what we want.... readfd should be readable !
//   // } EFEQ2(mode, 'n', 'N') {
/////  //    close(writefd);
    } EFEQ2(mode, 'o', 'O') { // 'o' open mode should pass stdin to the child then close it here .... ?
      // same as 'w' but stdout left open
// but that's the write end of the pipe ..........................................
  //    close(writefd);
// maybe don't close the write end .... just don't interact with it ???????
//      close(0); // dup2 should do this ........................................
//      dup2(readfd, 0);   // stdin  <- readfd .... this should be correct ...........................
      close(writefd);  // close the write end of the pipe since reading from it
      // close(0) should be performed by dup2
      close(0);
      dup2(readfd, 0); // this should close stdin and reopen it as readfd
//      dup2(0, readfd);
      // do not dupe readfd to stdin - consider closing stdin ... why no pipe tho ... dupe stdin as readfd of pipe... seems reasonable
      // so... in write mode, stdout closes ....? where ? seems open ... same as in 'W' mode ......................
    } EL { perror("invalidmode"); exit(2); }
//    setpgid(childpid, childpid); // so spawned sh can end
    // so setpgid(0, 0) means use this pid as the group id for this process (independent process group)
// ^^^ this one does not close stdin - should it? should there be a stdin param? yes ..
// shellopen seems to use modew... close outpipe, (if inpipe not stdin (dup stdin, close inpipe))
//    close(pdest[1]);
//    IF (pdest[0] NQ STDIN_FILENO) {
//      dup2(pdest[0], STDIN_FILENO);
//      close(pdest[0]);
//    }
    IFEQ3(mode, 'R', 'W', 'O') {
//    IF (mode EQ 'R' OR mode EQ 'W') {
      // run a script
      // if mode is 'L', execv for each line of input (but can also 'P' to line-handling commands)
      // if mode is 'P', run the command with stdin as input ... won't it already be available ? so 'P' == 'R'
// input -> stdin for execv ! ..
      CH commandline[2000]; // could use dynamic memory and calculate length
      // write each arg to commandline ... command = commandline
      IF (command EQNULL) { // no command given
        IF (args EQNULL) { RT NULL; } // no arglist given either
        IF (*args) { RT NULL; } // this is a NULL string command
        STRF(commandline, "%s", *args);
        WI (args) { // should always be a positive memory address
          INC args; // INC before add since $0 added already
          IF (*args EQNULL) { BK; } // break on trailing NULL
          CSENDP(commandend, commandline); // append
          STRF(commandend, " %s", *args);
        }
        command = commandline; // use built command
      } EF (args) { // need to append args to command
        STRF(commandline, "%s", command); // command was defined (also it could have args in it)
        WI (args) { // should always be a positive memory address
          CSENDP(commandend, commandline); // append
          STRF(commandend, " %s", *args);
          INC args; // add before INC since $0 was command
          IF (*args EQNULL) { BK; } // break on trailing NULL
        }
        command = commandline; // use rebuilt command
      }
      CS argv[] = { "sh", "-c", command, NULL };
      execv(_PATH_BSHELL, argv);
      // or use execlp if no path ...
// free dynamic memory once process exits ... it may not come back tho, oh well
      _exit(99); RT NULL; // -> child thread ends when execv ends
//    } EF (mode EQ 'l' OR 
//    } EFEQ2(mode, 'l', 'm') {
    } EL { // run a program ('r' or 'w') .. or 'o'
      IF (command EQNULL) { // no command given
        IF (args EQNULL) { RT NULL; } // no arglist given either
        IF (*args EQNULL) { RT NULL; } // this is a NULL string command
//        execv(_PATH_BSHELL, args); // pass args as argv directly
        execv(args[0], args);
      } EF (args) {
        // need to rebuild argv with command at position 0 ... difficult !
        Rs("USE SCRIPT MODE TO RUN COMMAND + ARGS UNTIL THIS IS FIXED");
        RT NULL;
      } EL { // command given, no args given
        execl(command, command);
//        CS argv[] = { command, NULL };
//        execv(_PATH_BSHELL, argv);
      }
      // or use execlp if no path ...
      _exit(99); RT NULL; // -> child thread ends when execv ends
    }
  } EL { // otherwise, manage the parent thread
    IFEQ2(mode, 'r', 'R') { close(writefd); } // close write end since parent fd is read-only
    EFEQ2(mode, 'w', 'W') { close(readfd);  } // close read end since parent fd is write-only
    EFEQ2(mode, 'o', 'O') {
      close(readfd); // <-- do not close the read end but simply don't interact with it ...?
//      close(0); // no, don't close stdin ....
//      dup2(0, 0); // try to close and reopen stdin
//      close(1); // try closing stdout in parent thread so that child thread can write ,,,,?
// assume stdout can be shared... seems to be possible
//      close(0); // close stdin in the parent thread .... also close readfd?
    } // ...........................!!!! // same behaviour as 'W' but can write to stdout // currently no difference!
  } //  Ms("("); W1("%d", childpid); Ms(")"); // notify stdout
  IF (pid) { *pid = childpid; } // set pid pointer for calling thread (if one given)
  IFEQ2(mode, 'r', 'R') { RT fdopen(readfd, "r"); } // ready for output to be read from it
  EFEQ2(mode, 'w', 'W') { RT fdopen(writefd, "w"); } // ready for input to be written to it
  EFEQ2(mode, 'o', 'O') { RT fdopen(writefd, "w"); } // same behaviour as W but W mode may block if stdout is waiting
  EL { RT NULL; }
// open outpipe for writing ?! - no, connect second end to write, first end to stdin ....?
//    FS infs = fdopen(fds[1], "w");
//  close(pdest[0]);
//    RT infs;
//  } EF (mode EQ 'w' OR mode EQ 'W') {
//    FS outfs = fdopen(fds[0], "r");
//    RT outfs; // rly ?
//  } EL { RT NULL; }
//    RT fdopen(pdest[1], "w");
//  RT fdopen(fd[useparentfd], fdopentype);
}

FS openprogram(CS command, PIDP pidp) {
  CH readprogram = 'r';
  RT openprogramscript(command, readprogram, pidp, NULL);
}

FS saveprogram(CS command, PIDP pidp) {
  CH writeprogram = 'w';
  RT openprogramscript(command, writeprogram, pidp, NULL);
}

// FS openinputprogram(CS command, FS input, PIDP pidp)

FS openscript(CS command, PIDP pidp) {
  CH readscript = 'R';
  RT openprogramscript(command, readscript, pidp, NULL);
}

FS savescript(CS command, PIDP pidp) {
  CH writescript = 'W';
  RT openprogramscript(command, writescript, pidp, NULL);
}

#define CLOSELINESCRIPT(buffername)         \
    CSFSLINE(buffername, buffername##fs);   \
    CLOSEFILE(buffername##fs);
#define OPENLINESCRIPT(buffername, scriptstr) \
    PID buffername##pid = 0;                \
    FS buffername##fs = openscript(scriptstr, &buffername##pid);
//#define CLOSESCRIPT(buffername)             \
//    CLOSEFILE(buffername##fs);
// ^ only available in the context of OPENSCRIPT (else use CLOSESCRIPTFILE)
#define OPENSCRIPT(scriptstr) openscript(scriptstr, NULL)
#define CLOSESCRIPT(fstream)  CLOSEFILE(fstream)
#define CSLINESCRIPT(buffername, scriptstr) \
    OPENLINESCRIPT(buffername, scriptstr);      \
    CLOSELINESCRIPT(buffername);
#define LINESCRIPT(buffer, script)  CSLINESCRIPT(buffer, script)


#define SIZEMAXANIMATIONFRAMES     1501 // 1min@25fps+1frame
#define SIZENOANIMATIONZINDEX     -1000000000
#define SIZEANIMATIONREPEATVALUE  -1000000001

#define LASTFRAMEZINDEX SIZENOANIMATIONZINDEX
#define HOLDFRAMEVALUE  SIZEANIMATIONREPEATVALUE

typedef struct _Size {
  CH type;
  IN width, height, depth;
  IN marginwidth, marginheight;
  IN margindepth, marginexcess;
  IN xoffset, yoffset, zoffset;
  IN xyangle, xzangle, yzangle; // later: LF
  IN magnitude, magpower;
  IN magscale, scalepower;
  IN x2offset, y2offset, z2offset;
  IN margin2width, margin2height;
  IN margin2depth, margin2excess;
  IN x3offset[SIZEMAXANIMATIONFRAMES];
  IN y3offset[SIZEMAXANIMATIONFRAMES];
  IN z3offset[SIZEMAXANIMATIONFRAMES];
  IN numframes;
  struct _Size *next;
} Size;

Size *readsizes(FS stream) {
  Size *s1 = SIZEMEM(Size);
  Size *sn = s1;
  sn->type = A0;
  sn->width = sn->height = sn->depth = 0; //  X  x  Y  x  Z
  sn->marginwidth = sn->marginheight = 0; //   LR,    LR,TB
  sn->margindepth = sn->marginexcess = 0; // LR,T,B, L,T,B,R
  sn->xoffset = sn->yoffset = sn->zoffset = 0; // X,Y,Z
  sn->xyangle = sn->xzangle = sn->yzangle = 0; // later: LF
  sn->magnitude = sn->magscale = 0; // magscale 0 disables offset
  sn->magpower = sn->scalepower = 0; // ^0 value will be calculated as ^1
  sn->x2offset = sn->y2offset = sn->z2offset = 0; // X,Y,Z
  sn->margin2width = sn->margin2height = 0; //   LR,    LR,TB
  sn->margin2depth = sn->margin2excess = 0; // LR,T,B, L,T,B,R
  sn->x3offset[0] = sn->y3offset[0] = 0;    // no local offset
  sn->z3offset[0] = LASTFRAMEZINDEX;        // no animation
  sn->numframes = 0; // if there are 0 frames the 0-frame should be the last
  // margin, then offset, then magnified angle, then offset2, then margin2
  sn->next = NULL;
  INP svalue = &sn->width;
  IN inch = NUL;
  WI (1) {
    inch = GETFCH(stream);
    BKEQEOF(inch);
    IF (inch EQ '\n') {
      Size *sp = sn;
      sn = SIZEMEM(Size);
      sp->next = sn;
      sn->type = A1; // can be 1D, 2D or 3D (0x0x0 is 1D)
      sn->width = sn->height = sn->depth = 0;
      sn->marginwidth = sn->marginheight = 0; //   LR,    LRxTB
      sn->margindepth = sn->marginexcess = 0; // LRxTxB, LxTxBxR
      sn->xoffset = sn->yoffset = sn->zoffset = 0; // X,Y,Z
      sn->xyangle = sn->xzangle = sn->yzangle = 0; // later: LF
      sn->magnitude = sn->magscale = 0; // magscale 0 disables offset
      sn->magpower = sn->scalepower = 0; // ^0 value will be calculated as ^1
      sn->x2offset = sn->y2offset = sn->z2offset = 0; // X,Y,Z
      sn->margin2width = sn->margin2height = 0; //   LR,    LR,TB
      sn->margin2depth = sn->margin2excess = 0; // LR,T,B, L,T,B,R
      sn->x3offset[0] = sn->y3offset[0] = 0;    // no local offset
      sn->z3offset[0] = 0;                      // no depth offset
      sn->numframes = 0; // if there are 0 frames the 0-frame should be the last
      sn->next = NULL;
      svalue = &sn->width;
    } EF ISNUMBER(inch) {
      *svalue = (*svalue * 10) + (inch - A0);
    } EF (inch EQ 'x') {
      IFEQ(svalue, &sn->width) { svalue = &sn->height; sn->type = A2; } // 100x would be 100x0
      EFEQ(svalue, &sn->height) { svalue = &sn->depth; sn->type = A3; } // 100xx would be 100x0x0
      EFEQ(svalue, &sn->depth) { sn->type = AE; RT s1; } // stop if 4th dimension detected
    } EF (inch EQ '-') { // 1680x1050-40x40 adds a 40 margin (XxY)
      IFEQ3(svalue, &sn->width, &sn->height, &sn->depth) { svalue = &sn->marginwidth; }
      EFEQ3(svalue, &sn->xyangle, &sn->xzangle, &sn->yzangle) { svalue = &sn->margin2width; }    // 10x10@1,1,1*1^1*1^1-m2[,m2,m2,m2..]
      EFEQ2(svalue, &sn->magnitude, &sn->magpower)  { svalue = &sn->margin2width; }              // 10x10@1,1,1*1^1-m2[,m2,m2,m2..]
      EFEQ2(svalue, &sn->magscale, &sn->scalepower) { svalue = &sn->margin2width; }              // 10x10@1,1,1*1^1*1^1-m2[,m2,m2,m2..]
      EFEQ3(svalue, &sn->x2offset, &sn->y2offset, &sn->z2offset) { svalue = &sn->margin2width; } // 10x10@1*1^1+o2,o2,o2-m2[,m2,m2,m2..]
      EL { sn->type = AE; RT s1; } // XxYxZ-m,m,m,m+o,o,o@a,a,a*m^p*s^p+o2,o2,o2-m2,m2,m2,m2
      // margin must come before offset !
      // if - comes before +, - is a box margin. if after, it is invalid.....................
    } EF (inch EQ '+') { // 1680x1050+40,40 adds a 40 offset (X,Y) (which is relative to margin !)
      IFEQ3(svalue, &sn->width, &sn->height, &sn->depth) { svalue = &sn->xoffset; } // 10x10+5[,5,5..]
      EFEQ2(svalue, &sn->marginwidth, &sn->marginheight) { svalue = &sn->xoffset; } // 10x10-10,10+5[,5,5..]
      EFEQ2(svalue, &sn->margindepth, &sn->marginexcess) { svalue = &sn->xoffset; } // 10x10-10,10,10,10+5[,5,5..]
      EFEQ3(svalue, &sn->xyangle, &sn->xzangle, &sn->yzangle) { svalue = &sn->x2offset; }    // 10x10@1,1,1*1^1*1^1-m2[,m2,m2,m2..]
      EFEQ2(svalue, &sn->magnitude, &sn->magpower           ) { svalue = &sn->x2offset; }              // 10x10@1,1,1*1^1-m2[,m2,m2,m2..]
      EFEQ2(svalue, &sn->magscale, &sn->scalepower          ) { svalue = &sn->x2offset; }              // 10x10@1,1,1*1^1*1^1-m2[,m2,m2,m2..]
      EFEQ3(svalue, &sn->x2offset, &sn->y2offset, &sn->z2offset) { svalue = &sn->x3offset[0]; } // 10x10@1*1^1+o2,o2,o2-m2[,m2,m2,m2..]
      EFEQ2(svalue, &sn->margin2width, &sn->margin2height      ) { svalue = &sn->x3offset[0]; } // 10x10@0-10,10+5[,5,5..]
      EFEQ2(svalue, &sn->margin2depth, &sn->margin2excess      ) { svalue = &sn->x3offset[0]; } // 10x10@0-10,10,10,10+5[,5,5..]
      EFEQ3(svalue, &sn->x3offset[0], &sn->y3offset[0], &sn->z3offset[0]) { svalue = &sn->x3offset[INC sn->numframes]; } // point to frame 1
      EF (sn->numframes GT 0)                                             { svalue = &sn->x3offset[INC sn->numframes]; } // point to frame N
      EL { sn->type = AE; RT s1; } // XxYxZ-m,m,m,m+o,o,o@a,a,a*m^p*s^p+o2,o2,o2-m2,m2,m2,m2 <-- no offset after margin2, later have a list!!
//      svalue = &sn->offsetx; ........................... :5+0,0,0+1,1,1+2,2,2+3,3,3+4,4,4+5,5,5+6,6,6 offset2->offset3 since :5 is optional!!
      // consider an offset after margin before modifier .. and a second offset after modifier .. which then has a margin and series of local offsets
      // a margin after offset is currently invalid but maybe it should skip the @modifier and refer to -margin2... yeah ... 640x480+0-0+1,1,1+2,2,2
    } EF (inch EQ '@') { // @xy,xz,yzangle*magnutide^magpower*magscale^scalepower offset modifier (@modifier)
      svalue = &sn->xyangle;
//      IFEQ3(svalue, &sn->width, &sn->height, &sn->depth) { svalue = &sn->xyangle; } // 100x100x100+offset
//      EFEQ2(svalue, &sn->marginwidth, &sn->marginheight) { svalue = &sn->xyangle; } // 10x10-5x5+offset
//      EFEQ2(svalue, &sn->margindepth, &sn->marginexcess) { svalue = &sn->xyangle; } // 10x10-5x2x2x5+offset
//      EL { RT s1; } // no - after + allowed at this time (- margin relates to size before offset)
    } EF (inch EQ ',') { // allow x or , between offset angles and margin values (but not dimension sizes)
      IFEQ(svalue, &sn->marginwidth)  { svalue = &sn->marginheight; } // LR->TB
      EFEQ(svalue, &sn->marginheight) { svalue = &sn->margindepth;  } // LR->T->B
      EFEQ(svalue, &sn->margindepth)  { svalue = &sn->marginexcess; } // L->T->B->R
      EFEQ(svalue, &sn->marginexcess) { sn->type = AE; RT s1; } // stop on 5th margin: WxHxDxE LRxTB LRxTxB LxTxBxR
      EFEQ(svalue, &sn->xoffset) { svalue = &sn->yoffset; } // X->Y
      EFEQ(svalue, &sn->yoffset) { svalue = &sn->zoffset; } // Y->Z
      EFEQ(svalue, &sn->zoffset) { sn->type = AE; RT s1; } // stop on 4th dimension offset
      EFEQ(svalue, &sn->xyangle) { svalue = &sn->xzangle; } // XY->XZ
      EFEQ(svalue, &sn->xzangle) { svalue = &sn->yzangle; } // XZ->YZ
      EFEQ(svalue, &sn->yzangle) { sn->type = AE; RT s1; } // stop on 4th angle: +XY,XZ,YZ only!
      EF (sn->numframes GT 0 AND svalue EQ &sn->x3offset[sn->numframes]) { svalue = &sn->y3offset[sn->numframes]; }
      EF (sn->numframes GT 0 AND svalue EQ &sn->y3offset[sn->numframes]) { svalue = &sn->z3offset[sn->numframes]; }
      EF (sn->numframes GT 0 AND svalue EQ &sn->z3offset[sn->numframes]) { sn->type = AE; RT s1; } // stop on 4th dimension offset for now
      // later: { svalue = sn->t3offset[sn->numframes]; } for variable frame timingggggssssssssssssssssssssssssss!!
      EL { sn->type = AE; RT s1; } // any other , is invald
    } EF (inch EQ '*') { // * only appears after + as part of the offset
      IFEQ3(svalue, &sn->xyangle, &sn->xzangle, &sn->yzangle) { svalue = &sn->magnitude; } // 10x10+45,45,45*10
      EFEQ2(svalue, &sn->magnitude, &sn->magpower)  { svalue = &sn->magscale; }       // scale magnitude by 5: 10x10+45*10*5
      EFEQ2(svalue, &sn->magscale, &sn->scalepower) { sn->type = AE; RT s1; } // no exponent after exponent (at this time?)
      EL { sn->type = AE; RT s1; } // stop on 3rd multiplier: *MAGNITUDE*MAGSCALE only !
    } EF (inch EQ '^') { // ^ only appears after magscale as a means of scaling exponentially
      IFEQ(svalue, &sn->magnitude) { svalue = &sn->magpower;   } // +*1^2 for exp magnitude
      EFEQ(svalue, &sn->magscale)  { svalue = &sn->scalepower; } // +*1^2*1.6^5 for exp scale
      EL { sn->type = AE; RT s1; } // only magnitude and magscale have exponents
    } EL { sn->type = AE; RT s1; } // stop if unexpected character detected (includes -) [no negative sizes]
  } // the last screen in the list is likely to be 0x0 due to the trailing \n
  RT s1; // readsizes declares sizemem * sn Size elements and returns a pointer to the first
}



CH readuntilchar(FS stream, CH delim) {


}

CH readaline(FS stream, CS buffer, IN bufferlimit) {
  IN delimchar = NUL; // end on NUL unless expecting a data file
  IN delim2char = '\n'; // end on \n since this is readng a line
  IN bufindex = 0; // bufferlimit 100 will write \0 at [100]
  WI (stream AND (bufindex LT bufferlimit)) {
    IN inch = GETFCH(stream);
    IFEQ(inch, EOF)  { RT EOF; }  // presumably -1
    IFEQ(inch, '\n') { RT '\n'; } // delimiter was reached
    buffer[bufindex] = inch;
    buffer[INC bufindex] = NUL;
  } ; RT (EOF -1); // EOB endofbuffer, presumably -2
}

// ^ currently not used, instead favouring LINESCRIPT()
/*
FILE * popen2(string command, string type, int & pid)
{
    pid_t child_pid;
    int fd[2];
    pipe(fd);

    if((child_pid = fork()) == -1)
    {
        perror("fork");
        exit(1);
    }

    if (child_pid == 0)
    {
        if (type == "r")
        {
            close(fd[READ]);    //Close the READ end of the pipe since the child's fd is write-only
            dup2(fd[WRITE], 1); //Redirect stdout to pipe
        }
        else
        {
            close(fd[WRITE]);    //Close the WRITE end of the pipe since the child's fd is read-only
            dup2(fd[READ], 0);   //Redirect stdin to pipe
        }

        setpgid(child_pid, child_pid); //Needed so negative PIDs can kill children of /bin/sh
        execl("/bin/sh", "/bin/sh", "-c", command.c_str(), NULL);
        exit(0);
    }
    else
    {
        if (type == "r")
        {
            close(fd[WRITE]); //Close the WRITE end of the pipe since parent's fd is read-only
        }
        else
        {
            close(fd[READ]); //Close the READ end of the pipe since parent's fd is write-only
        }
    }

    pid = child_pid;

    if (type == "r")
    {
        return fdopen(fd[READ], "r");
    }

    return fdopen(fd[WRITE], "w");
}

int pclose2(FILE * fp, pid_t pid)
{
    int stat;

    fclose(fp);
    while (waitpid(pid, &stat, 0) == -1)
    {
        if (errno != EINTR)
        {
            stat = -1;
            break;
        }
    }

    return stat;
}

FS openreadprogram() {
  RT shell
}

IN logcmd(CS command) {
  IN pid;
  FS fp = openreadprogram(command);

  }


} // 

int main()
{
    int pid;
    string command = "ping 8.8.8.8"; 
    FILE * fp = popen2(command, "r", pid);
    char command_out[100] = {0};
    stringstream output;

    //Using read() so that I have the option of using select() if I want non-blocking flow
    while (read(fileno(fp), command_out, sizeof(command_out)-1) != 0)
    {
        output << string(command_out);
        kill(-pid, 9);
        memset(&command_out, 0, sizeof(command_out));
    }

    string token;
    while (getline(output, token, '\n'))
        printf("OUT: %s\n", token.c_str());

    pclose2(fp, pid);

    return 0;
}
*/
