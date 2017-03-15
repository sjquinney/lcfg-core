/*************************************************************************

  This is the C library of error and log message routines used
  by LCFG components.

*************************************************************************/

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifndef __APPLE__
#include <sys/sysmacros.h>
#endif
#include <syslog.h>
#ifdef __linux__
#include <string.h>
#endif
#ifdef __SVR4
#include <strings.h>
#endif
#ifdef __APPLE__
#include <sys/malloc.h>
#else
#include <malloc.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include "utils.h"
#include <fcntl.h>
#include <signal.h>
#ifdef __linux__
#include <asm/ioctls.h>
#endif

#define MOVE_TO_COL "\033\[60G"
#define SETCOLOR_NORMAL "\033\[0;39m"
#define SETCOLOR_SUCCESS "\033\[0;32m"
#define SETCOLOR_FAILURE "\033\[0;31m"
#define SETCOLOR_ERROR "\033\[0;31m"
#define SETCOLOR_WARNING "\033\[1;33m"
#define SETCOLOR_INFO SETCOLOR_NORMAL
#define SETCOLOR_DEBUG "\033\[1;35m"

typedef struct { char *name; int value; } FACILITY;
FACILITY fnametab[] = {
  { "auth", LOG_AUTH }, 
#ifdef __linux__
  { "authpriv", LOG_AUTHPRIV }, 
#endif
  { "cron", LOG_CRON }, 
  { "daemon", LOG_DAEMON }, 
  { "kern", LOG_KERN }, 
  { "local0", LOG_LOCAL0 }, 
  { "local1", LOG_LOCAL1 }, 
  { "local2", LOG_LOCAL2 }, 
  { "local3", LOG_LOCAL3 }, 
  { "local4", LOG_LOCAL4 }, 
  { "local5", LOG_LOCAL5 }, 
  { "local6", LOG_LOCAL6 }, 
  { "local7", LOG_LOCAL7 }, 
  { "lpr", LOG_LPR }, 
  { "mail", LOG_MAIL }, 
  { "news", LOG_NEWS }, 
  { "syslog", LOG_SYSLOG }, 
  { "user", LOG_USER }, 
  { "uucp", LOG_UUCP },
  { NULL, -1 }
};

static void LCFG_Escalate( char *omp, char *msg, char *arg );

static FILE *where = NULL;

/*************************************************************************/
  void LCFG_SetOutput( FILE *fp )
/*************************************************************************/
{
  /* Set file pointer for output */
  where = fp;
}

/*************************************************************************/
  void LCFG_SetFdOutput( int fd )
/*************************************************************************/
{
  /* Set file descriptor for output */
  where = fdopen(fd,"a");
}

/*************************************************************************/
  FILE *LCFG_GetOutput()
/*************************************************************************/
{
  return (where==NULL) ? stderr : where;
}

/*************************************************************************/
  /*VARARGS*/ char *LCFG_Append( char *first, ... )
/*************************************************************************/
{
  /* Append Strings */

  va_list ap;
  char *s, *p;
  int len = strlen(first);

  va_start(ap,first);
  while ( (s=va_arg(ap, char*)) != NULL ) len += strlen(s);
  va_end(ap);

  s = p = (char*)malloc(1+len); *p='\0';
  if (s == NULL) return NULL;
  (void)strcpy(p,first);
  va_start(ap,first);
  while ( (s=va_arg(ap, char*)) != NULL ) (void)strcat( p, s );
  va_end(ap);

  return p;
}

/*************************************************************************/
  char *LCFG_FirstLine( char *s )
/*************************************************************************/
{
  /* Copy of First Line of String */
  
  char *n, *e = index(s,'\n');
  if (e) {
    n = malloc(e-s+1);
    if (n==NULL) return NULL;
    strncpy(n,s,e-s);
    n[e-s]='\0';
  } else {
    n = malloc(strlen(s)+1);
    if (n==NULL) return NULL;
    strcpy(n,s);
  }
  return n;
}

/*************************************************************************/
  char *LCFG_AddNewLine( char *s )
/*************************************************************************/
{
  /* Copy of String with Newline at end */

  char *n;
  if (*s && s[strlen(s)-1]=='\n') {
    n = malloc(strlen(s)+1);
    if (n==NULL) return NULL;
    strcpy(n,s);
  } else {
    n = malloc(strlen(s)+2);
    if (n==NULL) return NULL;
    strcpy(n,s);
    strcat(n,"\n");
  }
  return n;
}

/************************************************************************/
  int LCFG_ShiftPressed( void )
/************************************************************************/
{
  /* Detect if shift key pressed */
#ifdef __linux__
  union {
    char subcode;    
    char shiftstate;
  } arg;
  
  arg.subcode = 6;
  
  if (ioctl(0,TIOCLINUX,&arg)!=0) { return -1; }
  return arg.shiftstate;
#else
  return 0;
#endif
}

/************************************************************************/
  int LCFG_FancyStatus( void )
/************************************************************************/
{
  /* Fancy status if output is a vt */
#ifdef __linux__
  unsigned char twelve = 12;
  int maj;
  struct stat sb;

  FILE *fpout = LCFG_GetOutput();
  if (!fpout) return 0;

  if (fstat(fileno(fpout),&sb)<0) return 0;
  maj = major(sb.st_rdev);
  if (maj != 3 && (maj < 136 || maj > 143) &&
      (ioctl (0, TIOCLINUX, &twelve) >= 0)) return 1;
#endif
  return 0;
}


/************************************************************************/
  int LCFG_Progress()
/************************************************************************/
{
  /* Print progress */
  
  int fd, outfd, pcount = 0;
  char buf[16];
  static char ptab[] = "-\\|/";
  mode_t mode = S_IRUSR|S_IWUSR; /* User read and write access */

  FILE *fpout = LCFG_GetOutput();
  if (!fpout) return 0;  

  outfd = fileno(fpout);
  if ( outfd == -1 ) {
    return 1;
  }

  if (isatty(outfd)) {

    fd = open(PROGRESSFILE,O_RDONLY);
    if (fd>=0) {
      int count = read(fd,buf,15);
      close(fd);
      if (count>0) {
	buf[count]='\0';
	pcount = atoi(buf);
      }
    }

    fprintf(fpout,"%c]%c%c",ptab[pcount%4],8,8);

    fd = open(PROGRESSFILE,O_WRONLY|O_CREAT,mode);
    if (fd>=0) {
      sprintf(buf,"%d\n%c",++pcount,0);
      write(fd,buf,1+strlen(buf));
      close(fd);
    }
  }

  return 1;
}

/************************************************************************/
  int LCFG_StartProgress( char *comp, char *msg )
/************************************************************************/
{
  /* Start progress message */
  
  char *s = LCFG_FirstLine(msg);
  int fancy = LCFG_FancyStatus();

  int outfd, retval = 0;
  FILE *fpout = LCFG_GetOutput();
  if (!fpout) return 0;

  outfd = fileno(fpout);

  if (s==NULL) return 0;

  if ( outfd == -1 ) { 
    retval = 1;
  } else {

    if ( isatty(outfd) ) {

      if (fancy) {
        fprintf(fpout,"LCFG %s: %s", comp, s);
      } else {
        fprintf(fpout,"[WAIT] %s: %s", comp, s);
      }

      free(s);

      fprintf(fpout," [");
      unlink(PROGRESSFILE);
      return LCFG_Progress();
    }

  }

  free(s);
  return retval;
}

/************************************************************************/
  int LCFG_EndProgress()
/************************************************************************/
{
  /* End progress message */
  
  int fancy = LCFG_FancyStatus();

  int outfd;
  FILE *fpout = LCFG_GetOutput();
  if (!fpout) return 0;

  outfd = fileno(fpout);
  if ( outfd == -1 ) {
    return 1;
  }

  if ( isatty(outfd) ) {
    fprintf(fpout,"%c   %c%c%c",8,8,8,8);
    if (fancy) {
      fprintf(fpout,"%s[%s WAIT %s]", MOVE_TO_COL, SETCOLOR_INFO,
              SETCOLOR_NORMAL );
    }
    fprintf(fpout,"\n");
  }

  unlink(PROGRESSFILE);
  return 0;
}

/************************************************************************/
  static int LCFG_Message( char *comp, char *tag, char *fancytag,
			   char *colour, char *msg, int escalate )
/************************************************************************/
{
  /* Print message to output in standard format */

  char *s = LCFG_AddNewLine(msg);
  char *k = s;
  int first = 1;
  int fancy = LCFG_FancyStatus();
  
  FILE *fpout = LCFG_GetOutput();
  if (!fpout) return 0;

  if (s==NULL) return 0;
  while (*s) {
    if (first) {
      if (fancy) fprintf(fpout, "LCFG %s: ", comp);
      else fprintf(fpout, "[%s] %s: ", tag, comp);
      first = 0;
    }
    if (*s == '\n') {
      if (fancy) {
	fprintf(fpout, "%s[", MOVE_TO_COL );
	if (colour) fprintf(fpout, "%s", colour );
	fprintf(fpout, "%s%s]", fancytag, SETCOLOR_NORMAL );
      }
      first=1;
    }
    putc(*s++,fpout);
  }
  free(k);
  return 1;
}

/************************************************************************/
  char *LCFG_TimeStamp( void )
/************************************************************************/
{
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  static char buf[128];
  sprintf(buf,"%02d/%02d/%02d %02d:%02d:%02d",
	  tm->tm_mday, tm->tm_mon+1, tm->tm_year%100,
	  tm->tm_hour, tm->tm_min, tm->tm_sec );
  return buf;
}

/************************************************************************/
  static int LCFG_LogMessage( char *comp, char *msg, char *prefix,
			      char *ext, int escalate )
/************************************************************************/
{
  /* Print message to logfile */
  /* Return non-zero if we have created a new file */

  char *s = LCFG_AddNewLine(msg);
  char *k = s;
  int first = 1;
  char *timestamp = LCFG_TimeStamp();
  char *logfile, *lf = getenv("_LOGFILE");
  FILE *fp;
  int newfile;
  
  if (s==NULL) return 0;
  if (lf && *lf) { logfile = LCFG_Append(lf,(ext?ext:NULL),NULL); }
  else { logfile = LCFG_Append(LOGDIR,"/",comp,(ext?ext:NULL),NULL); }
  if (logfile==NULL) { free(k); return 0; }
  
  newfile = access(logfile,F_OK);
  
  if (!(fp=fopen(logfile,"a"))) {
    if (escalate) LCFG_Escalate(comp,"failed to open logfile",logfile);
    free(k); free(logfile);
    return newfile;
  }
  while (*s) {
    if (first) {
      fprintf(fp, "%s: %s", timestamp, prefix );
      first = 0;
    }
    if (*s == '\n') first=1;
    putc(*s++,fp);
  }
  if (fclose(fp) != 0) {
    if (escalate) LCFG_Escalate(comp,"failed to write logfile",logfile);
    free(k); free(logfile);
    return newfile;
  }
  free(k); free(logfile);
  return newfile;
}

/************************************************************************/
  static int LCFG_Monitor( char *comp, char *tag, char *msg, int escalate )
/************************************************************************/
{
  /* Send message to monitoring system */
  
  char *s;
  char *pipe = getenv("LCFG_MONITOR");
  FILE *fp;
  
  if (pipe==NULL || *pipe=='\0') return 1;
  if (!(fp=fopen(pipe,"a"))) {
    if (errno==ENOENT) return 1;
    if (escalate) LCFG_Escalate(comp,"failed to open monitor pipe",pipe);
    return 0;
  }
  if ((s=LCFG_FirstLine(msg))==NULL) { fclose(fp); return 0; }
  fprintf( fp, "%s %s.%s %s\n", LCFG_TimeStamp(), comp, tag, msg );
  if (fclose(fp) != 0) {
    if (escalate) LCFG_Escalate(comp,"failed to write monitor pipe",pipe);
    free(s);
    return 0;
  }
  free(s);
  return 1;
}

/************************************************************************/
  static int LCFG_Syslog( char *comp, char *tag, char *msg,
			  int level, char *deffname, int escalate )
/************************************************************************/
{
  /* Send message to syslog */
  
  char *s, *ident, *fname = getenv("LCFG_SYSLOG");
  int facility = -1;
  FACILITY *p = fnametab;
  
  if (fname && *fname=='\0') fname = NULL;
  if (fname==NULL) fname = deffname;
  if (fname==NULL) return 1;
  while (p->name != NULL) {
    if (strcasecmp(fname,p->name)==0) { facility=p->value; break; }
    ++p;
  }
  if (facility<0) {
    if (escalate) LCFG_Escalate(comp,"invalid syslog facility",fname);
    return 0;
  }
  
  if ((s=LCFG_FirstLine(msg))==NULL) return 0;
  ident = LCFG_Append(comp,".",tag,NULL);
  if (ident==NULL) { free(s); return 0; }
  openlog(ident,LOG_PID,facility);
  syslog(facility|level,"%s",s);
  closelog();
  free(s); free(ident);
  return 1;
}

/************************************************************************/
  static void LCFG_Escalate( char *comp, char *msg, char *arg )
/************************************************************************/
{
  /* Use this for reporting errors in error reporting! */
  
  char *s = (errno) ?
    LCFG_Append( msg, " : ", arg, "\n(", strerror(errno),")\n", NULL )
    : LCFG_Append( msg, " : ", arg, "\n", NULL );
    
  LCFG_Message( comp, "ERROR", " ERR  ", SETCOLOR_ERROR, s, 0 );
  LCFG_Syslog( comp, "err", s, LOG_ERR, "daemon", 0 );
  LCFG_LogMessage( comp, s, "** ", NULL, 0 );
  LCFG_LogMessage( comp, s, "", ".err", 0 );
  free(s);
}

/************************************************************************/
  void LCFG_Fail( char *comp, char *msg )
/************************************************************************/
{
  /* Use this for critical errors which cause an abort */

  int newerr;
  LCFG_Message( comp, "FAIL", "FAILED", SETCOLOR_FAILURE, msg, 1 );
  LCFG_LogMessage( comp, msg, "** ", NULL, 1 );
  newerr = LCFG_LogMessage( comp, msg, "", ".err", 1 );
  LCFG_Syslog( comp, "fail", msg, LOG_CRIT, NULL, 1 );
  LCFG_Monitor( comp, "fail", msg, 1 );
  if (newerr) LCFG_Ack();
}

/************************************************************************/
  void LCFG_Error( char *comp, char *msg )
/************************************************************************/
{
  /* Use this for non-fatal errors */

  int newerr;
  LCFG_Message( comp, "ERROR", " ERR  ", SETCOLOR_ERROR, msg, 1 );
  LCFG_LogMessage( comp, msg, "** ", NULL, 1 );
  newerr = LCFG_LogMessage( comp, msg, "", ".err", 1 );
  LCFG_Syslog( comp, "err", msg, LOG_ERR, NULL, 1 );
  LCFG_Monitor( comp, "err", msg, 1 );
  if (newerr) LCFG_Ack();
}

/************************************************************************/
  void LCFG_Warn( char *comp, char *msg )
/************************************************************************/
{
  /* Use this for warnings */

  int newwarn;
  LCFG_Message( comp, "WARNING", " WARN ", SETCOLOR_WARNING, msg, 1 );
  LCFG_LogMessage( comp, msg, "++ ", NULL, 1 );
  newwarn = LCFG_LogMessage( comp, msg, "", ".warn", 1 );
  LCFG_Syslog( comp, "warn", msg, LOG_WARNING, NULL, 1 );
  LCFG_Monitor( comp, "warn", msg, 1 );
  if (newwarn) LCFG_Ack();
}

/************************************************************************/
  void LCFG_Event( char *comp, char *event, char *msg )
/************************************************************************/
{
  /* Use this to log special events like reboot requests */

  int newevent;
  char *ext = LCFG_Append(".",event,NULL); 

  LCFG_LogMessage( comp, msg, "== ", NULL, 1 );
  newevent = LCFG_LogMessage( comp, msg, "", ext, 1 );
  LCFG_Syslog( comp, event, msg, LOG_INFO, NULL, 1 );
  LCFG_Monitor( comp, event, msg, 1 );
  if (newevent) LCFG_Ack();
}

/************************************************************************/
  void LCFG_ClearEvent( char *comp, char *event )
/************************************************************************/
{
  /* Use this to reset errors, warnings and other events */
  
  char *logfile, *lf = getenv("_LOGFILE");
  
  if (lf && *lf) { logfile = LCFG_Append(lf,".",event,NULL); }
  else { logfile = LCFG_Append(LOGDIR,"/",comp,".",event,NULL); }
  if (logfile==NULL) { return; }
  
  if (unlink(logfile) != 0) {
    if (errno!=ENOENT) {
      char *msg = LCFG_Append("can't delete event file: ",logfile,"\n",
			      strerror(errno),NULL);
      LCFG_Warn(comp,msg);
      free(msg);
    }
  } else { LCFG_Ack(); }
  free(logfile);
}

/************************************************************************/
  void LCFG_Info( char *comp, char *msg )
/************************************************************************/
{
  /* Use this for informational messages */

  char *quiet;
  quiet = getenv("_QUIET");

  /* Only log to stderr when quiet is not set */
  if ( quiet == NULL || strlen(quiet) == 0 || strncmp( quiet, "0", 1 ) == 0 ) {
    LCFG_Message( comp, "INFO", " INFO ", SETCOLOR_INFO, msg, 1 );
  }

  LCFG_Syslog( comp, "info", msg, LOG_INFO, NULL, 1 );
  LCFG_LogMessage( comp, msg, "   ", NULL, 1 );
}

/************************************************************************/
  void LCFG_Debug( char *comp, char *msg )
/************************************************************************/
{
  LCFG_Message( comp, "DEBUG", " DBUG ", SETCOLOR_DEBUG, msg, 1 );
  LCFG_Syslog( comp, "debug", msg, LOG_DEBUG, NULL, 1 );
  LCFG_LogMessage( comp, msg, "-- [debug] ", NULL, 1 );
}

/************************************************************************/
  void LCFG_OK( char *comp, char *msg )
/************************************************************************/
{
  /* Use this for information messages which don't need logging */

  LCFG_Message( comp, "OK", "  OK  ", SETCOLOR_SUCCESS, msg, 1 );
}

/************************************************************************/
  void LCFG_Log( char *comp, char *msg )
/************************************************************************/
{
  /* Use this for simple log messages */

  LCFG_LogMessage( comp, msg, "   ", NULL, 1 );
}

/************************************************************************/
  void LCFG_LogPrefix( char *comp, char *pfx, char *msg )
/************************************************************************/
{
  /* Use this for simple log messages */

  LCFG_LogMessage( comp, msg, pfx, NULL, 1 );
}

/************************************************************************/
  void LCFG_Notify( char *comp, char *tag, char *msg )
/************************************************************************/
{
  /* Use this for syslog only messages (monitoring) */
  
  LCFG_Monitor( comp, tag, msg, 1 );
}

/************************************************************************/
  void LCFG_Ack()
/************************************************************************/
{
  /* Acknowledge server */

  pid_t pid;
  int status;

  /* "Best effort" so we ignore failures */  
  
  if ( (pid=fork()) ) {
    if (pid<0) return;
    while (1) {
      if (waitpid(pid, &status, 0) == -1) {
	if (errno != EINTR) return;
      } else return;
    }
  }

  execlp("lcfgack","lcfgack",(char*)NULL);
  exit(1);
}
