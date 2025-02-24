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

/* we're interested in a nominal INPUT data set
 * raw[NSAM][NCHAN], 		but this may be extracted from a larger set:
 * raw[SPB][NCHAN], 		picked out as
 * raw[0:SPB:STRIDE][NCHAN]
 * the OUTPUT data set:
 * int32 MEAN_ALL[NCHAN]
 */

#define PS_NCHAN 		"NCHAN"				/* asynInt32, 	    r/o 	*/			/* asynInt32,       r/o 	*/
#define PS_SSB			"SSB"				/* sample size bytes */
#define PS_NSPAD		"NSPAD"				/* pseudo SPAD size (4 LW */
#define PS_MEAN_RAW		"MEAN_RAW"                      /* vector showing all mean values */
#define PS_MEAN_EGU		"MEAN_EGU"
#define PS_SLOWMONMS		"SLOWMONMS"

#define PS_SITE_ESLO		"SITE_ESLO"
#define PS_SITE_EOFF		"SITE_EOFF"

#define PS_MEAN_ESLO		"ALL_ESLO"
#define PS_MEAN_EOFF		"ALL_EOFF"


#define MAX_SITES		6

void runTask(void *drvPvt);

template <class T>
class SlowmonDriver: public asynPortDriver {

	void member_init();

protected:
	int P_NCHAN;
	int P_SSB;
	int P_NSPAD;
	int P_MEAN_RAW;
	int P_MEAN_EGU;
	int P_SLOWMONMS;

	int P_SITE_ESLO;
	int P_SITE_EOFF;
	int P_MEAN_ESLO;
	int P_MEAN_EOFF;

	unsigned *raw_mean;
	float *cal_mean;

	const int nchan;
	std::vector<int> site_list;
	std::vector<int> site_nchan;
	const int ssb;
	unsigned slowmonms;

	int* site_off;          /* offset each site in set */

	float* set_eoff;	/* float set_eoff[nchan] */
	float* set_eslo;        /* float set_eslo[nchan] */


	epicsTimeStamp t0, t1;

	static int nice;
	static int stub_es;
	static int verbose;
	static int trace;

	int nsites() { return site_list.size(); }

public:
	SlowmonDriver(const char *portName, int _nchan, std::vector<int> _site_list, std::vector<int> _site_nchan);

	virtual void task();
	//virtual void handle_buffer(int vbn) = 0;
	virtual void handle_buffer();

	virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
	virtual asynStatus writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
	                                        size_t nElements);
	virtual asynStatus readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
	                                        size_t nElements, size_t *nIn);

	static const int nspad;

	static void task_runner(void *drvPvt);
};



#endif /* ACQ400IOCAPP_SRC_SLOWMONDRIVER_H_ */
