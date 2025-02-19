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

#include "acq-util.h"           // getRoot()

using namespace std;
#include "Buffer.h"
#include "ES.h"


static const char *driverName="SlowmonDriver";


static void task_runner(void *drvPvt)
{
	SlowmonDriver *pPvt = (SlowmonDriver *)drvPvt;
	pPvt->task();
}


SlowmonDriver::SlowmonDriver(const char *portName, int _nchan, int _nsam, int _spb):
asynPortDriver(portName,
/* maxAddr */		_nchan,    /* nchan from 0 */
/* Interface mask */    asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt32ArrayMask|asynDrvUserMask,
/* Interrupt mask */	asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt32ArrayMask,
/* asynFlags no block*/ 0,
/* Autoconnect */       1,
/* Default priority */  0,
/* Default stack size*/ 0),
	nchan(_nchan), nsam(_nsam), spb(_spb), stride(spb/nsam)
{
	asynStatus status = asynSuccess;

	createParam(PS_NCHAN, 	asynParamInt32, 	&P_NCHAN);
	createParam(PS_NSAM,    asynParamInt32,         &P_NSAM);
	createParam(PS_SPB,     asynParamInt32,         &P_SPB);
	createParam(PS_STRIDE,  asynParamInt32,         &P_STRIDE);
	createParam(PS_MEAN_ALL, asynParamInt32Array,   &P_MEAN_ALL);

	mean = new int[_nsam];

	if (!status){
		fprintf(stderr, "ERROR %s %d\n", __FUNCTION__, status);
	}

	memset(&t0, 0, sizeof(t0));
	memset(&t1, 0, sizeof(t1));

	/* Create the thread that computes the waveforms in the background */
	status = (asynStatus)(epicsThreadCreate("SlowmonTask",
			epicsThreadPriorityHigh - SlowmonDriver::nice,
			epicsThreadGetStackSize(epicsThreadStackMedium),
			(EPICSTHREADFUNC)::task_runner,
			this) == NULL);
	if (status) {
		printf("%s:%s: epicsThreadCreate failure\n", driverName, __FUNCTION__);
		return;
	}
}

int SlowmonDriver::verbose = ::getenv_default("SlowmonDriver_VERBOSE", 0);
int SlowmonDriver::stub_es = ::getenv_default("SlowmonDriver_STUB_ES", 0);
int SlowmonDriver::nice		= ::getenv_default("SlowmonDriver_NICE", 0);

void SlowmonDriver::task()
{
	int fc = open("/dev/acq400.0.bq", O_RDONLY);
	assert(fc >= 0);
	for (unsigned ii = 0; ii < Buffer::nbuffers; ++ii){
		Buffer::create(getRoot(0), Buffer::bufferlen);
	}

	int ib;

	if ((ib = getBufferId(fc)) < 0){
		fprintf(stderr, "ERROR: getBufferId() fail");
		return;
	}

	epicsTimeGetCurrent(&t0);

	while((ib = getBufferId(fc)) >= 0){
		epicsTimeGetCurrent(&t1);
		handle_buffer(ib);
		t0 = t1;
	}
	printf("%s:%s: exit on getBufferId failure\n", driverName, __FUNCTION__);
}

typedef short RTYPE;

void SlowmonDriver::handle_buffer(int ib)
{
	RTYPE* raw = (RTYPE*)Buffer::the_buffers[ib]->getBase();
	int ic;
	int is;

	memset(mean, 0, nchan*sizeof(int));

	for (is = 0; is < nsam; is += stride){
		for (ic = 0; ic < nchan; ++ic){
			mean[ic] += raw[is*nchan+ic];
		}
	}
	for (ic = 0; ic < nchan; ++ic){
		mean[ic] /= nsam;
	}
	doCallbacksInt32Array(mean, nchan, P_MEAN_ALL, 0);
}


extern "C" {

	/** EPICS iocsh callable function to call constructor for the testAsynPortDriver class.
	  * \param[in] portName The name of the asyn port driver to be created.
	  * \param[in] maxPoints The maximum  number of points in the volt and time arrays */
	int slowmonDriverConfigure(const char *portName, int nchan, int maxPoints, unsigned data_size)
	{
		//return MultiChannelScope::factory(portName, nchan, maxPoints, data_size);
		printf("%s, %s, %d, %d %d\n", __FUNCTION__, portName, nchan, maxPoints, data_size);
		new SlowmonDriver(portName, nchan, maxPoints, data_size);

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
