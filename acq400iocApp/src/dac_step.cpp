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


DacStep::DacStep(const char *_portName, int _site, int _nchan, int _maxPoints, unsigned _data_size):
	asynPortDriver(portName,
/* maxAddr */		1,
/* Interface mask */    asynEnumMask|asynInt32Mask|asynFloat64Mask|asynDrvUserMask,
/* Interrupt mask */	asynEnumMask|asynInt32Mask|asynFloat64Mask,
/* asynFlags no block*/ 0,
/* Autoconnect */       1,
/* Default priority */  0,
/* Default stack size*/ 0),
	site(site), nchan(_nchan)
{
	;
}

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
	int DacStepConfigure(const char *portName, int site, int nchan, int maxPoints, unsigned data_size)
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
	static const iocshFuncDef initFuncDef = { "acq400JudgementConfigure", 5, initArgs };
	static void initCallFunc(const iocshArgBuf *args)
	{
		DacStepConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
	}

	void DacStepRegister(void)
	{
	    iocshRegister(&initFuncDef,initCallFunc);
	}

	epicsExportRegistrar(DacStepRegister);
}
