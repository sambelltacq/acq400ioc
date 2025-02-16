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


SlowmonDriver::SlowmonDriver(const char *portName, int numChannels, int maxPoints, unsigned data_size):
asynPortDriver(portName,
/* maxAddr */		numChannels+1,    /* nchan from 0 + ADDR_WIN_ALL */
/* Interface mask */    asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt8ArrayMask|asynInt16ArrayMask|asynInt32ArrayMask|asynDrvUserMask,
/* Interrupt mask */	asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt8ArrayMask|asynInt16ArrayMask|asynInt32ArrayMask,
/* asynFlags no block*/ 0,
/* Autoconnect */       1,
/* Default priority */  0,
/* Default stack size*/ 0)
{

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
	static const iocshArg initArg1 = { "max chan", iocshArgInt };
	static const iocshArg initArg2 = { "max points", iocshArgInt };
	static const iocshArg initArg3 = { "data size", iocshArgInt };
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
