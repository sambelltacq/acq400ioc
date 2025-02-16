/*
 * SlowmonDriver.h
 *
 *  Created on: 16 Feb 2025
 *      Author: pgm
 *
 *  vector<NCHAN> of scalar values, to 100Hz.
 *  TWO ways to do this
 *  - NACC slowmon on timer interrupt (data by PIO read), but with normal full rate as well
 *  - BQ on DMA
 *  ....  take limited samples (together with full rate) or
 *  ....  use NACC filter heavily, average all samples (INSTEAD of full rate)
 *
 *  WBN to demo BOTH, runtime switchable.
 *
 *  ref ACQ400DRV, 3d4915811bc4f6736ae363e92698c9d6d2ef44b7
 *  ... reach and store MEAN is MUCH faster than read and store TRANSPOSE which hammers the WB.
 */

#ifndef ACQ400IOCAPP_SRC_SLOWMONDRIVER_H_
#define ACQ400IOCAPP_SRC_SLOWMONDRIVER_H_



#include "asynPortDriver.h"

#define PS_NCHAN 		"NCHAN"				/* asynInt32, 		r/o */
#define PS_NSAM			"NSAM"				/* asynInt32,       r/o */


void runTask(void *drvPvt);

class SlowmonDriver: public asynPortDriver {

public:
	SlowmonDriver(const char *portName, int numChannels, int maxPoints, unsigned data_size);
};



#endif /* ACQ400IOCAPP_SRC_SLOWMONDRIVER_H_ */
