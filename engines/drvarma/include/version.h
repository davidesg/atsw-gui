/*
 * version.h -- la version de drvarma, en UN solo sitio.
 *
 * Used by the CLI usage and -version, the "Program" line of the .out of
 * both paths (.inp and .pre) and the GUI title. Up to 0.4.1 the program
 * did not state it anywhere: an .out did not say which binary wrote it.
 */
#ifndef DRVARMA_VERSION_H
#define DRVARMA_VERSION_H

#define DRVARMA_VERSION "5.0.0"

/* The commit it was built from: the Makefile writes build/git_hash.h. A
   build outside git (or without the Makefile) says "unknown".            */
#if defined(__has_include)
#  if __has_include("git_hash.h")
#    include "git_hash.h"
#  endif
#endif
#ifndef DRVARMA_GIT
#define DRVARMA_GIT "unknown"
#endif

/* "5.0.0 (git 63ada09abc)" */
#define DRVARMA_VERSION_FULL DRVARMA_VERSION " (git " DRVARMA_GIT ")"

#endif
