/*
 * acq400_asyn_common.h
 *
 *  Created on: 21 Feb 2025
 *      Author: pgm
 */

#ifndef ACQ400IOCAPP_SRC_ACQ400_ASYN_COMMON_H_
#define ACQ400IOCAPP_SRC_ACQ400_ASYN_COMMON_H_

#include <epicsExport.h>
#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <iocsh.h>

#include <vector>
#include <split2.h>

static inline
std::vector<int> csv2int(const char* csv) {
	std::vector<std::string> strings;
	split2(csv, strings, ',');
	std::vector<int> vi;
	for (std::string s: strings){
		vi.push_back(stoi(s));
	}
	return vi;
}

static inline
bool epicsTimeDiffLessThan(epicsTimeStamp& t1, epicsTimeStamp& t0, double tgts)
{
	epicsTime et1 = t1;
	epicsTime et0 = t0;


	return (et1 - et0) < tgts;
}

static inline
bool epicsTimeDiffGreaterThan(epicsTimeStamp& t1, epicsTimeStamp& t0, double tgts)
{
	epicsTime et1 = t1;
	epicsTime et0 = t0;


	return (et1 - et0) > tgts;
}




#endif /* ACQ400IOCAPP_SRC_ACQ400_ASYN_COMMON_H_ */
