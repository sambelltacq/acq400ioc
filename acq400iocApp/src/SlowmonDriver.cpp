/*
 * SlowmonDriver.cpp
 *
 *  Created on: 16 Feb 2025
 *      Author: pgm
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <iocsh.h>
#include <epicsExport.h>

#include "SlowmonDriver.h"

#include <string>



#include <sys/stat.h>
#include <iostream>
#include <string>

#include <fcntl.h>            	// open(2)
#include <unistd.h>		// close(2)

#include <time.h>

#include "acq-util.h"           // getRoot()

using namespace std;
#include "Buffer.h"
#include "ES.h"


static const char *driverName="SlowmonDriver";



template <class T>
SlowmonDriver<T>::SlowmonDriver(const char *portName, int _nchan, int _nsam):
asynPortDriver(portName,
/* maxAddr */		_nchan,    /* nchan from 0 */
/* Interface mask */    asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt32ArrayMask|asynDrvUserMask,
/* Interrupt mask */	asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt32ArrayMask,
/* asynFlags no block*/ 0,
/* Autoconnect */       1,
/* Default priority */  0,
/* Default stack size*/ 0),
	nchan(_nchan), nsam(_nsam),
	slowmonms(100)
{
	asynStatus status = asynSuccess;

	createParam(PS_NCHAN, 	asynParamInt32, 	&P_NCHAN);
	createParam(PS_NSAM,    asynParamInt32,         &P_NSAM);
	createParam(PS_SSB,     asynParamInt32,         &P_SSB);
	createParam(PS_NSPAD,   asynParamInt32,         &P_NSPAD);
	createParam(PS_MEAN_ALL, asynParamInt32Array,   &P_MEAN_ALL);
	createParam(PS_SLOWMONMS, asynParamInt32,   	&P_SLOWMONMS);

	mean = new unsigned[_nsam*sizeof(T)/sizeof(unsigned)+nspad];

	if (!status){
		fprintf(stderr, "ERROR %s %d\n", __FUNCTION__, status);
	}

	memset(&t0, 0, sizeof(t0));
	memset(&t1, 0, sizeof(t1));

	/* Create the thread that computes the waveforms in the background */
	status = (asynStatus)(epicsThreadCreate("SlowmonTask",
			epicsThreadPriorityHigh - SlowmonDriver::nice,
			epicsThreadGetStackSize(epicsThreadStackMedium),
			(EPICSTHREADFUNC)task_runner,
			this) == NULL);
	if (status) {
		printf("%s:%s: epicsThreadCreate failure\n", driverName, __FUNCTION__);
		return;
	}
}


template <class T>
void SlowmonDriver<T>::task_runner(void *drvPvt)
{
	SlowmonDriver *pPvt = (SlowmonDriver *)drvPvt;
	pPvt->task();
}

template <class T>
int SlowmonDriver<T>::verbose = ::getenv_default("SlowmonDriver_VERBOSE", 0);
template <class T>
int SlowmonDriver<T>::stub_es = ::getenv_default("SlowmonDriver_STUB_ES", 0);
template <class T>
int SlowmonDriver<T>::nice		= ::getenv_default("SlowmonDriver_NICE", 0);
template <class T>
int SlowmonDriver<T>::ssb     = 384;
template <class T>
const int SlowmonDriver<T>::nspad(4);



class PosixPeriodTimer {
	struct timespec next_time;
	void update_next_time(struct timespec delta) {
		clock_gettime(CLOCK_MONOTONIC, &next_time);
		next_time.tv_sec += delta.tv_sec;
		next_time.tv_nsec += delta.tv_nsec;
		if (next_time.tv_nsec >= NANO) {
			next_time.tv_sec++;
		        next_time.tv_nsec -= NANO;
		}
	}
public:
	PosixPeriodTimer(struct timespec delta)
	{
		update_next_time(delta);
	}
	PosixPeriodTimer(int deltams)
	{
		update_next_time(ms2timespec(deltams));
	}
	void wait_and_get_split(struct timespec delta){
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
		update_next_time(delta);
	}
	void wait_and_get_split(int deltams){
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
			update_next_time(ms2timespec(deltams));
		}

	struct timespec ms2timespec(int ms){
		struct timespec ts;
		ts.tv_sec = ms/1000;
		ts.tv_nsec = (ms/1000 - ts.tv_sec)*NANO;
		return ts;
	}
	static const int NANO;        // author has trouble counting the zeros
	static const int MICRO;
};

const int PosixPeriodTimer::NANO  = 1000000000;
const int PosixPeriodTimer::MICRO =    1000000;

template <class T>
void SlowmonDriver<T>::task()
{
	int fc = open("/dev/acq400.0.subr", O_RDONLY);
	assert(fc >= 0);

	const int lenw = ssb/sizeof(unsigned)+nspad;
	const int lenb = lenw*sizeof(unsigned);

	epicsTimeGetCurrent(&t0);

	for (PosixPeriodTimer ppt(slowmonms);; ppt.wait_and_get_split(slowmonms)){
		if (int rc = read(fc, mean, lenb) != lenb){
			fprintf(stderr, "ERROR read() return %d != %d\n", rc, lenb);
		}
		handle_buffer();
	}
}

typedef short RTYPE;

template<>
void SlowmonDriver<short>::handle_buffer()
{
	// @@todo do something with the SPAD timestamps
	doCallbacksInt16Array((epicsInt16*)mean, nchan, P_MEAN_ALL, 0);
}

template<>
void SlowmonDriver<unsigned>::handle_buffer()
{
	// @@todo do something with the SPAD timestamps
	doCallbacksInt32Array((epicsInt32*)mean, nchan, P_MEAN_ALL, 0);
}

template<class T>
void SlowmonDriver<T>::handle_buffer()
{
	assert(0);
}


extern "C" {

	/** EPICS iocsh callable function to call constructor for the testAsynPortDriver class.
	  * \param[in] portName The name of the asyn port driver to be created.
	  * \param[in] maxPoints The maximum  number of points in the volt and time arrays */
	int slowmonDriverConfigure(const char *portName, int nchan, int maxPoints, unsigned data_size)
	{
		//return MultiChannelScope::factory(portName, nchan, maxPoints, data_size);
		printf("%s, %s, %d, %d %d\n", __FUNCTION__, portName, nchan, maxPoints, data_size);
		if (data_size == 2){
			new SlowmonDriver<short>(portName, nchan, maxPoints);
		}else{
			new SlowmonDriver<int>(portName, nchan, maxPoints);
		}

		return 0;
	}

	/* EPICS iocsh shell commands */

	static const iocshArg initArg0 = { "uut", iocshArgString };
	static const iocshArg initArg1 = { "nchan", iocshArgInt };
	static const iocshArg initArg2 = { "nsam", iocshArgInt };
	static const iocshArg initArg3 = { "data_size", iocshArgInt };
	static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2, &initArg3 };
	static const iocshFuncDef initFuncDef = { "slowmonDriverConfigure", 4, initArgs };
	static void initCallFunc(const iocshArgBuf *args)
	{
		slowmonDriverConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
	}

	void slowmonDriverRegister(void)
	{
	    iocshRegister(&initFuncDef,initCallFunc);
	}

	epicsExportRegistrar(slowmonDriverRegister);
}
