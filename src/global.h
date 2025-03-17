// $Id: global.h,v 1.1 2017/12/06 08:20:10 skalak Exp $

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

// set to 1 in signal handler, can be read after EINTR

// have to use extern keyword because of new gcc behaviour
// (can be mitigated by using command-line '-fcommon' ;-)
extern volatile int ShouldStop;

#endif // _GLOBAL_H_
