/*
 * dac_step.h
 *
 *  Created on: 23 Dec 2024
 *      Author: pgm
 */

#ifndef ACQ400IOCAPP_SRC_DAC_STEP_H_
#define ACQ400IOCAPP_SRC_DAC_STEP_H_

#include "asynPortDriver.h"

#define PS_BQ		"BQ"		/* asynInt32,       r/o */
#define PS_CURSOR       "CURSOR"	/* asynInt32,       r/w */
#define PS_AO_STEP 	"AO_STEP"	/* asynInt16,       r/w */

class DacStep: public asynPortDriver {
public:
	static int factory(
		const char *portName, int site, int nchan, int maxPoints, unsigned data_size);

	virtual void task();
	static int verbose;
	static int step;

	const int site;
	const int nchan;
protected:
	DacStep(const char *_portName, int _site, int _nchan, int _maxPoints, unsigned _data_size);

	int P_BQ;
	int P_AO_STEP;
};



#endif /* ACQ400IOCAPP_SRC_DAC_STEP_H_ */
