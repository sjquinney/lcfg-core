/*************************************************************************

  This program is a simple command line interface to the LCFG 
  error message library. The command line utility is used by
  shell scripts and the ngeneric component for logging and
  error messages.

*************************************************************************/

#include <stdio.h>
#include <unistd.h>
#define _GNU_SOURCE
#ifdef __linux__
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#endif
#ifdef __SVR4
#include <stdlib.h>
#include <strings.h>
#endif
#include "utils.h"

#define MAXLINE 4096

extern char *LCFG_Append(char* first, ...);

/************************************************************************/
  int main ( int argc, char *argv[] )
/************************************************************************/
{
  char *comp = "lcfg";
  char *msgtype = "info";
  char buf[MAXLINE+1], *msg=""; 
  
  /* Message types */
  int ok=0, info=0, debug=0, fail=0, err=0, argerr=0,
    warn=0, log=0, start=0, progress=0, end=0, ack=0;
  char *tag = NULL;
  char *pfx = NULL;
  char *event = NULL;
  char *clear = NULL;
  opterr = 0;
  
  while (1) {
    
    int this_option_optind = optind ? optind : 1;

    int c = getopt (argc, argv, "+P:E:C:an:oidfewlspx");
    
    if (c == -1) break;
    
    switch (c) {
      
    case 'o': ok=1; break;
    case 'i': info=1; break;
    case 'd': debug=1; break;
    case 'f': fail=1; break;
    case 'e': err=1; break;
    case 'w': warn=1; break;
    case 'l': log=1; break;
    case 's': start=1; break;
    case 'p': progress=1; break;
    case 'a': ack=1; break;
    case 'x': end=1; break;
    case 'n': tag=optarg; break;
    case 'P': pfx=optarg; break;
    case 'E': event=optarg; break;
    case 'C': clear=optarg; break;
    case '?': argerr=1; break;
      
    default:
      break;
    }
  }

  /* Component name */
  if (optind<argc) comp = argv[optind++];
  
  /* Remaining arguments are the message */
  /* If non-present, then read message from stdin */
  if (optind<argc && strcmp(argv[optind],"-")) {
    
    while (optind<argc) {
      char *newmsg = (*msg) ? LCFG_Append(msg," ",argv[optind],NULL)
	: LCFG_Append(argv[optind],NULL);
      if (newmsg==NULL) {
	LCFG_Error("lcfg","lcfgmsg: malloc fail");
	exit(1);
      }
      if (*msg) free(msg); 
      msg = newmsg;
      ++optind;
    }
    
  } else if (optind<argc) {
    
    while (fgets(buf,MAXLINE,stdin)) {
      char *newmsg = (*msg) ? LCFG_Append(msg," ",buf,NULL)
	: LCFG_Append(buf,NULL);
      if (newmsg==NULL) {
	LCFG_Error("lcfg","lcfgmsg: malloc fail");
	exit(1);
      }
      if (*msg) free(msg);
      msg = newmsg;
    }
  }
  
  if (argerr) LCFG_Error("lcfg","lcfgmsg: bad arguments");
  
  if (clear) LCFG_ClearEvent(comp,clear);
  if (ok) LCFG_OK(comp,msg);
  if (info) LCFG_Info(comp,msg);
  if (debug) LCFG_Debug(comp,msg);
  if (fail) LCFG_Fail(comp,msg);
  if (err) LCFG_Error(comp,msg);
  if (warn) LCFG_Warn(comp,msg);
  if (event) LCFG_Event(comp,event,msg);
  if (log) { if (pfx) LCFG_LogPrefix(comp,pfx,msg); else LCFG_Log(comp,msg); }
  if (tag) LCFG_Notify(comp,tag,msg);
  if (start) LCFG_StartProgress(comp,msg);
  if (progress) LCFG_Progress();
  if (end) LCFG_EndProgress();
  if (ack) LCFG_Ack();
  
  return 0;
}
