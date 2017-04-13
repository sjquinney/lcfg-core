#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "packages.h"

static void usage(void) {

  printf("parse_pkgspec [-R|--rpm] [-p|--prefix][-n|--name][-v|--version][-a|--arch][-f|--flags][-e|--eval][-h|--help] package_spec\n\n");

}

static void myoutput(const char *key, const char *value, LCFGPkgStyle style) {

  char * out_value = value != NULL ? (char *) value : "";

  switch (style) {
  case LCFG_PKG_STYLE_EVAL:
    printf("export %s='%s'\n", key, out_value);
    break;
  default:
    printf("%s: %s\n", key, out_value);
    break;
  }

}

int main(int argc, char* argv[])
{

  if (argc < 2) {
    usage();
    return(1);
  }

  LCFGPkgStyle style = LCFG_PKG_STYLE_SPEC;

  bool rpm_name = false;
  bool print_all     = true;
  bool print_prefix  = false, print_name  = false, print_version = false;
  bool print_arch    = false, print_flags = false, print_context = false;
  bool print_release = false;

  while (1)
    {
      static struct option long_options[] =
        {
	  {"rpm",      no_argument,  0,  'R' },
          {"prefix",   no_argument,  0,  'p' },
          {"name",     no_argument,  0,  'n' },
          {"version",  no_argument,  0,  'v' },
          {"release",  no_argument,  0,  'r' },
          {"arch",     no_argument,  0,  'a' },
          {"flags",    no_argument,  0,  'f' },
          {"context",  no_argument,  0,  'c' },
          {"eval",     no_argument,  0,  'e' },
          {"help",     no_argument,  0,  'h' },
          {0,0,0,0} /* This is a filler for -1 */
        };

      int option_index = 0;

      int c = getopt_long (argc, argv, "Rpnvrafceh", long_options, &option_index);

      if (c == -1) break;

      switch (c) {
      case 'R':
	rpm_name = true;
	break;
      case 'p':
        print_all = false;
        print_prefix = true;
        break;
      case 'n':
        print_all = false;
        print_name = true;
        break;
      case 'v':
        print_all = false;
        print_version = true;
        break;
      case 'r':
        print_all = false;
        print_release = true;
        break;
      case 'a':
        print_all = false;
        print_arch = true;
        break;
      case 'f':
        print_all = false;
        print_flags = true;
        break;
      case 'c':
        print_all = false;
        print_context = true;
        break;
      case 'e':
        style = LCFG_PKG_STYLE_EVAL;
        break;
      case 'h':
        usage();
        return(0);
        break;
      default:
        usage();
        return(1);
        break;
      }
    }

  char *input;
  if ( optind <= argc ) {
    input = argv[optind];
  }
  else {
    usage();
    return(1);
  }

  LCFGPackage * pkg = NULL;
  char *msg = NULL;

  LCFGStatus parse_rc;
  if ( rpm_name )
    parse_rc = lcfgpackage_from_rpm_filename( input, &pkg, &msg );
  else
    parse_rc = lcfgpackage_from_spec( input, &pkg, &msg );

  if ( parse_rc == LCFG_STATUS_ERROR ) {
    fprintf( stderr, "Error: %s\n",
	     ( msg != NULL ? msg : "unknown problem occurred" ) );

    free(msg);

    exit(EXIT_FAILURE);
  }

  /* output the results */
  if (print_name || print_all ) {
    myoutput("Name", lcfgpackage_get_name(pkg), style);
  }
  if (print_version || print_all ) {
    myoutput("Version", lcfgpackage_get_version(pkg), style);
  }
  if (print_release || print_all ) {
    myoutput("Release", lcfgpackage_get_release(pkg), style);
  }
  if (print_arch || print_all ) {
    myoutput("Arch", lcfgpackage_get_arch(pkg), style);
  }
  if (print_flags || print_all ) {
    myoutput("Flags", lcfgpackage_get_flags(pkg), style);
  }
  if (print_prefix || print_all ) {
    char prefix[2] = "";
    if ( lcfgpackage_has_prefix(pkg) )
      sprintf( prefix, "%c", lcfgpackage_get_prefix(pkg) );
    myoutput("Prefix", prefix, style);
  }
  if (print_context || print_all ) {
    myoutput("Context", lcfgpackage_get_context(pkg), style);
  }

  lcfgpackage_relinquish(pkg);

  return 0;
}
