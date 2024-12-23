/*
 * dac_step.h
 *
 *  Created on: 23 Dec 2024
 *      Author: pgm
 */

#ifndef ACQ400IOCAPP_SRC_DAC_STEP_H_
#define ACQ400IOCAPP_SRC_DAC_STEP_H_

#include "asynPortDriver.h"

#define PS_BQ		/* asynInt32,       r/o */

class DacStep: public asynPortDriver {
public:
	static int factory(
		const char *portName, int site, int nchan, int maxPoints, unsigned data_size);

	static int verbose;

	const int site;
	const int nchan;
protected:
	DacStep(const char *_portName, int _site, int _nchan, int _maxPoints, unsigned _data_size);
};



#endif /* ACQ400IOCAPP_SRC_DAC_STEP_H_ */
