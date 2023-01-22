#ifndef CODECARGUMENTS_H
#define CODECARGUMENTS_H

#include <QtGlobal>

#ifndef QSMEDIA_API
#  ifdef QSMEDIA_STATIC
#    define QSMEDIA_API
#  else
#    ifdef QSMEDIA_LIBRARY
#      define QSMEDIA_API Q_DECL_EXPORT
#    else
#      define QSMEDIA_API Q_DECL_IMPORT
#    endif
#  endif
#endif

#endif // CODECARGUMENTS_H
