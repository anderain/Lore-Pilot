/*
 * GbTest.h
 *
 * GB2312 paged font test form for LorePilotReader.
 */

#ifndef GBTEST_H_
#define GBTEST_H_

#include <PalmOS.h>

/* Cell size requested by the Test menu: 8 or 12. */
void    GbTestRequest(UInt8 cell);

Boolean GbTestFormHandleEvent(EventType *eventP);

#endif /* GBTEST_H_ */
