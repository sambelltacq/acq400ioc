/*
 * dac_step.cpp
 *
 *  Created on: 23 Dec 2024
 *      Author: pgm
 */

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <iocsh.h>

#include "dac_step.h"
#include <epicsExport.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "acq-util.h"

static const char *driverName="DacStep";

static void task_runner(void *drvPvt)
{
	DacStep *pPvt = (DacStep *)drvPvt;
	pPvt->task();
}


DacStep::DacStep(const char *_portName, int _site, int _nchan, int _maxPoints, unsigned _data_size):
	asynPortDriver(_portName,
/* maxAddr */		_nchan+1,
/* Interface mask */    asynEnumMask|asynInt32Mask|asynFloat64Mask|asynDrvUserMask|asynInt16ArrayMask|asynInt32ArrayMask,
/* Interrupt mask */	asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt16ArrayMask|asynInt32ArrayMask,
/* asynFlags no block*/ 0,
/* Autoconnect */       1,
/* Default priority */  0,
/* Default stack size*/ 0),
	site(_site), nchan(_nchan)
{
	asynStatus status = asynSuccess;

	createParam(PS_BQ,               asynParamInt32,         	&P_BQ);
	createParam(PS_AO_STEP,	 	 asynParamInt16Array,           &P_AO_STEP);
	/* Create the thread that handles the BQ feer background */
	status = (asynStatus)(epicsThreadCreate("bq_feed",
			epicsThreadPriorityMax,
			epicsThreadGetStackSize(epicsThreadStackMedium),
			(EPICSTHREADFUNC)::task_runner,
			this) == NULL);
	if (status) {
		printf("%s:%s: epicsThreadCreate failure\n", driverName, __FUNCTION__);
		return;
	}
}

#define BQ_FNAME 	"/dev/acq400.0.bqf"
#define DAC_STEP_FNAME	"/dev/acq400.5.dac_step"

int DacStep::step = ::getenv_default("DACSTEP_STEP", 50);

void DacStep::task()
{
	int fc = open(BQ_FNAME, O_RDONLY);
	int fs = open(DAC_STEP_FNAME, O_WRONLY);

	assert(fc >= 0);
	assert(fs >= 0);
	int ib;


	short* channels = new short[nchan];
	for (int ii = 0; ii < nchan; ++ii){
		channels[ii] = 0;
	}
	const int ssb = nchan*sizeof(short);


	while((ib = getBufferId(fc)) >= 0){

		if (verbose > 1){
			printf("%03d\n", ib);
		}
		setIntegerParam(P_BQ, ib);


		callParamCallbacks(0);

		for (int ii = 0; ii < nchan; ++ii){
			channels[ii] += (ii&1? -1: 1) * DacStep::step;
		}
		int rc = write(fs, channels, ssb);
		if (rc != ssb){
			perror("DacStep::task() write failed");
		}
	}
}

int DacStep::verbose = ::getenv_default("DACSTEP_VERBOSE", 0);
/** factory() method: creates concrete class with specialized data type: either epicsInt16 or epicsInt32 */
int DacStep::factory(const char *portName, int site, int nchan, int maxPoints, unsigned data_size)
{
	new DacStep(portName, site, nchan, maxPoints, data_size);
	return(asynSuccess);
}


/* Configuration routine.  Called directly, or from the iocsh function below */

extern "C" {

	/** EPICS iocsh callable function to call constructor for the testAsynPortDriver class.
	  * \param[in] portName The name of the asyn port driver to be created.
	  * \param[in] maxPoints The maximum  number of points in the volt and time arrays */
	int dacStepConfigure(const char *portName, int site, int nchan, int maxPoints, unsigned data_size)
	{
		return DacStep::factory(portName, site, nchan, maxPoints, data_size);
	}

	/* EPICS iocsh shell commands */

	static const iocshArg initArg0 = { "portName", iocshArgString };
	static const iocshArg initArg1 = { "site", iocshArgInt };
	static const iocshArg initArg2 = { "nchan", iocshArgInt };
	static const iocshArg initArg3 = { "max points", iocshArgInt };
	static const iocshArg initArg4 = { "data size", iocshArgInt };

	static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2, &initArg3, &initArg4 };
	static const iocshFuncDef initFuncDef = { "dacStepConfigure", 5, initArgs };
	static void initCallFunc(const iocshArgBuf *args)
	{
		dacStepConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
	}

	void DacStepRegister(void)
	{
	    iocshRegister(&initFuncDef,initCallFunc);
	}

	epicsExportRegistrar(DacStepRegister);
}
