/* Internal generic functions which are used for both lists and sets */

#ifndef LCFG_CORE_PACKAGES_CONTAINER_H
#define LCFG_CORE_PACKAGES_CONTAINER_H

/**
 * @brief Package container identifier
 *
 * This is used to specify which package container type is being
 * passed to a generic function.
 *
 */

typedef enum {
  LCFG_PKG_CONTAINER_LIST, /**< LCFGPackageList */
  LCFG_PKG_CONTAINER_SET   /**< LCFGPackageSet */
} LCFGPkgContainerType;

/**
 * @brief Union for passing package container
 *
 * This is used to pass a reference to a package container into a
 * generic function.
 *
 */

union LCFGPkgContainer {
  struct LCFGPackageList * list; /**< Reference to LCFGPackageList */
  struct LCFGPackageSet  * set;  /**< Reference to LCFGPackageSet */
};
typedef union LCFGPkgContainer LCFGPkgContainer;

LCFGChange lcfgpackages_from_cpp( const char * filename,
                                  LCFGPkgContainer * ctr,
				  LCFGPkgContainerType pkgs_type,
                                  const char * defarch,
                                  const char * macros_file,
				  char ** incpath,
                                  LCFGOption options,
                                  char ** msg )
  __attribute__((warn_unused_result));

#endif /* LCFG_CORE_PACKAGES_CONTAINER_H */

/* eof */
