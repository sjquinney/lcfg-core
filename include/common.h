/* Common definitions */

#ifndef LCFG_COMMON_H
#define LCFG_COMMON_H

typedef enum {
  LCFG_STATUS_OK,
  LCFG_STATUS_WARN,
  LCFG_STATUS_ERROR
} LCFGStatus;

typedef enum {
  LCFG_CHANGE_ERROR,
  LCFG_CHANGE_NONE,
  LCFG_CHANGE_MODIFIED,
  LCFG_CHANGE_ADDED,
  LCFG_CHANGE_REPLACED,
  LCFG_CHANGE_REMOVED
} LCFGChange;

/* Various options for functions which read in and write out resources */

#define LCFG_OPT_NOCONTEXT      1
#define LCFG_OPT_NOPREFIX       2
#define LCFG_OPT_NEWLINE        4
#define LCFG_OPT_NOVALUE        8
#define LCFG_OPT_NOTEMPLATES   16
#define LCFG_OPT_ALLOW_NOEXIST 32
#define LCFG_OPT_ENCODE        64
#define LCFG_OPT_ALL_CONTEXTS 128
#define LCFG_OPT_USE_META     256

/* Generic single-linked list */

struct LCFGSListNode {
  void * data;
  struct LCFGSListNode * next;
};

typedef struct LCFGSListNode LCFGSListNode;

LCFGSListNode * lcfgslistnode_new(void * data);

void lcfgslistnode_destroy(LCFGSListNode * node);

#endif /* LCFG_COMMON_H */

/* eof */

