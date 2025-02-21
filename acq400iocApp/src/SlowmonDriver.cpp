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

#include <errno.h>

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
SlowmonDriver<T>::SlowmonDriver(const char *portName, int _nchan):
asynPortDriver(portName,
/* maxAddr */		_nchan,    /* nchan from 0 */
/* Interface mask */    asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt16ArrayMask|asynInt32ArrayMask|asynDrvUserMask,
/* Interrupt mask */	asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt16ArrayMask|asynInt32ArrayMask,
/* asynFlags no block*/ 0,
/* Autoconnect */       1,
/* Default priority */  0,
/* Default stack size*/ 0),
	nchan(_nchan),
	slowmonms(100)
{
	asynStatus status = asynSuccess;

	createParam(PS_NCHAN, 	asynParamInt32, 	&P_NCHAN);
	createParam(PS_SSB,     asynParamInt32,         &P_SSB);
	createParam(PS_NSPAD,   asynParamInt32,         &P_NSPAD);
	createParam(PS_MEAN_ALL, asynParamInt32Array,   &P_MEAN_ALL);
	createParam(PS_SLOWMONMS, asynParamInt32,   	&P_SLOWMONMS);

	mean = new unsigned[nchan*sizeof(T)/sizeof(unsigned)+nspad];

	if (status){
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
		int rc = clock_gettime(CLOCK_REALTIME, &next_time);
		if (rc != 0){
			perror("clock_gettime");
		}
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
		struct timespec res = {};
		clock_getres(CLOCK_REALTIME, &res);
		printf("%s clock_getres %ld %ld\n", __FUNCTION__, res.tv_sec, res.tv_nsec);

		update_next_time(ms2timespec(deltams));
	}
	void wait_and_get_split(struct timespec delta){
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_time, NULL);
		update_next_time(delta);
	}
	void wait_and_get_split(int deltams){
		int rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_time, NULL);
		if (rc != 0){
			perror("clock_nanosleep()");
		}
		update_next_time(ms2timespec(deltams));
	}

	struct timespec ms2timespec(int ms){
		struct timespec ts;
		ts.tv_sec = ms/1000;
		ts.tv_nsec = (ms - ts.tv_sec*1000)*(NANO/1000);
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

	printf("%s slowmon ms: %d\n", __FUNCTION__, slowmonms);

	for (PosixPeriodTimer ppt(slowmonms);; ppt.wait_and_get_split(slowmonms)){
		if (int rc = read(fc, mean, lenb) != lenb){
			fprintf(stderr, "ERROR read() return %d != %d\n", rc, lenb);
		}
		//handle_buffer();
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
	  * \param[in] nchan number of channels in array
	  * \param[in] data_size 2|4 bytes
	  */
	int slowmonDriverConfigure(const char *portName, int nchan, unsigned data_size)
	{
		//return MultiChannelScope::factory(portName, nchan, maxPoints, data_size);
		printf("pgmwashere R1005\n");
		printf("%s, %s, %d, %d\n", __FUNCTION__, portName, nchan, data_size);
		if (data_size == 2){
			new SlowmonDriver<short>(portName, nchan);
		}else{
			new SlowmonDriver<int>(portName, nchan);
		}

		return 0;
	}

	/* EPICS iocsh shell commands */

	static const iocshArg initArg0 = { "port", iocshArgString };
	static const iocshArg initArg1 = { "nchan", iocshArgInt };
	static const iocshArg initArg2 = { "data_size", iocshArgInt };
	static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2};
	static const iocshFuncDef initFuncDef = { "slowmonDriverConfigure", 3, initArgs };
	static void initCallFunc(const iocshArgBuf *args)
	{
		slowmonDriverConfigure(args[0].sval, args[1].ival, args[2].ival);
	}

	void slowmonDriverRegister(void)
	{
	    iocshRegister(&initFuncDef,initCallFunc);
	}

	epicsExportRegistrar(slowmonDriverRegister);
}
