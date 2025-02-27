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

#include "acq400_asyn_common.h"
#include "SlowmonDriver.h"

#include <split2.h>

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


#define TRACE do { if (trace) fprintf(stderr, "TRACE %s %d\n", __FUNCTION__, __LINE__); } while (0)

template <class T>
SlowmonDriver<T>::SlowmonDriver(const char *portName, int _nchan, std::vector<int> _site_list, std::vector<int> _site_nchan):
asynPortDriver(portName,
/* maxAddr */		_nchan,    /* nchan from 0 */
/* Interface mask */    asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt16ArrayMask|asynInt32ArrayMask|asynFloat32ArrayMask|asynDrvUserMask,
/* Interrupt mask */	asynEnumMask|asynInt32Mask|asynFloat64Mask|asynInt16ArrayMask|asynInt32ArrayMask|asynFloat32ArrayMask,
/* asynFlags no block*/ 0,
/* Autoconnect */       1,
/* Default priority */  0,
/* Default stack size*/ 0),
	nchan(_nchan),
	site_list(_site_list),
	site_nchan(_site_nchan),
	ssb(_nchan*sizeof(T)),
	slowmonms(100),
	enable(0)
{
	member_init();
	asynStatus status = asynSuccess;

	createParam(PS_NCHAN, 	asynParamInt32, 	&P_NCHAN);
	createParam(PS_SSB,     asynParamInt32,         &P_SSB);
	createParam(PS_NSPAD,   asynParamInt32,         &P_NSPAD);
	createParam(PS_MEAN_RAW, asynParamInt32Array,   &P_MEAN_RAW);
	createParam(PS_MEAN_EGU, asynParamInt32Array,   &P_MEAN_EGU);
	createParam(PS_SLOWMONMS, asynParamInt32,   	&P_SLOWMONMS);

	createParam(PS_SITE_ESLO, asynParamFloat32Array, &P_SITE_ESLO);
	createParam(PS_SITE_EOFF, asynParamFloat32Array, &P_SITE_EOFF);
	createParam(PS_MEAN_ESLO, asynParamFloat32Array, &P_MEAN_ESLO);
	createParam(PS_MEAN_EOFF, asynParamFloat32Array, &P_MEAN_EOFF);
	createParam(PS_MEAN_EN,   asynParamInt32,   	 &P_MEAN_EN);
	createParam(PS_QUERY_ESLO, asynParamInt32,       &P_QUERY_ESLO);
	createParam(PS_QUERY_EOFF, asynParamInt32,       &P_QUERY_EOFF);
	createParam(PS_SET_WATERFALL, asynParamInt32,    &P_SET_WATERFALL);

	raw_mean = new unsigned[nchan*sizeof(T)/sizeof(unsigned)+nspad];
	cal_mean = new float[nchan];

	if (status){
		fprintf(stderr, "ERROR %s %d\n", __FUNCTION__, status);
	}

	TRACE;



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

const float V1 = 1.0/32768;

template <class T>
void SlowmonDriver<T>::member_init()
{
	memset(&t0, 0, sizeof(t0));
	memset(&t1, 0, sizeof(t1));


	set_eoff = new float[nchan];
	set_eslo = new float[nchan];
	/* make an obvious, incorrect, but reasonable default should ESLO/EOFF init not complete */
	for (int ic = 0; ic < nchan; ++ic){
		set_eslo[ic] = V1;
		set_eoff[ic] = 0;
	}
	site_off = new int[nsites()];

	for (int ii = 0, offset = 0; ii < nsites(); ++ii){
		site_off[ii] = offset;
		offset += site_nchan[ii];
	}
	TRACE;
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
int SlowmonDriver<T>::trace = ::getenv_default("SlowmonDriver_TRACE", 0);
template <class T>
int SlowmonDriver<T>::stub_es = ::getenv_default("SlowmonDriver_STUB_ES", 0);
template <class T>
int SlowmonDriver<T>::nice		= ::getenv_default("SlowmonDriver_NICE", 0);
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
void SlowmonDriver<T>::task_wait_params()
{
	int retries = 0;
	asynStatus rc;
	epicsInt32 _enable;
	do {
		sleep(1);
		rc = getIntegerParam(P_MEAN_EN, &_enable);
		TRACE;
	} while (rc == asynParamUndefined && ++retries < 5);

	if (rc != asynSuccess){
		reportGetParamErrors(rc, P_NCHAN, 0, "task()");
		fprintf(stderr, "ERROR P_ES_SPREAD %d rc %d\n", P_MEAN_EN, rc);
		exit(1);
	};
}

template <class T>
void SlowmonDriver<T>::task()
{
	int fc = open("/dev/acq400.0.subr", O_RDONLY);
	assert(fc >= 0);

	const int lenw = ssb/sizeof(unsigned)+nspad;
	const int lenb = lenw*sizeof(unsigned);

	task_wait_params();
	epicsTimeGetCurrent(&t0);

	printf("%s slowmon ms: %d\n", __FUNCTION__, slowmonms);

	unsigned iter = 0;

	TRACE;
	for (PosixPeriodTimer ppt(slowmonms);;
			ppt.wait_and_get_split(enable? slowmonms: 1000), ++iter){
		if (!enable){
			if (verbose) fprintf(stderr, "%s IDLE\n", __FUNCTION__);
			continue;
		} else if (int rc = read(fc, raw_mean, lenb) != lenb){
			fprintf(stderr, "ERROR read() return %d != %d\n", rc, lenb);
			continue;
		}else{
			if (verbose && iter < 10){
				fprintf(stderr, "%s %d lenb:%d\n", __FUNCTION__, iter, lenb);
			}
			epicsTimeGetCurrent(&t1);
			handle_buffer();
		}
	}
	TRACE;
}

typedef short RTYPE;

template<>
void SlowmonDriver<short>::handle_buffer()
{
	epicsInt16* mean16 = (epicsInt16*)raw_mean;

/*
	if (epicsTimeDiffGreaterThan(t1, t0, 1)){
		fprintf(stderr, "%s %d: %04x %04x %04x %04x\n",
			__FUNCTION__, nchan, mean[0], mean[1], mean[2], mean[3]);
	}
*/
	int waterfall = 0;
	asynStatus rc = getIntegerParam(P_SET_WATERFALL, &waterfall);
	if (rc != asynSuccess){
		reportGetParamErrors(rc, P_SET_WATERFALL, 0, "task()");
	};

	for (int ic = 0; ic < nchan; ++ic){
		cal_mean[ic] = mean16[ic]*set_eslo[ic] + set_eoff[ic] + waterfall*ic;
		if (verbose > 1 && ic < 2){
			fprintf(stderr, "%s [%d] cal %.0f = raw %04x\n",
				__FUNCTION__, ic, cal_mean[ic], mean16[ic]);
		}
	}
	doCallbacksFloat32Array(cal_mean, nchan, P_MEAN_EGU, 0);
	doCallbacksInt16Array(mean16, nchan, P_MEAN_RAW, 0);
}

template<>
void SlowmonDriver<epicsInt32>::handle_buffer()
{
	epicsInt32* mean32 = (epicsInt32*)raw_mean;
	// @@todo do something with the SPAD timestamps

	int waterfall = 0;
	asynStatus rc = getIntegerParam(P_SET_WATERFALL, &waterfall);
	if (rc != asynSuccess){
		reportGetParamErrors(rc, P_SET_WATERFALL, 0, "task()");
	};

	for (int ic = 0; ic < nchan; ++ic){
		cal_mean[ic] = mean32[ic]*set_eslo[ic] + set_eoff[ic] + waterfall*ic;
		if (verbose > 1 && ic < 2){
			fprintf(stderr, "%s [%d] cal %.0f = raw %04x\n",
				__FUNCTION__, ic, cal_mean[ic], mean32[ic]);
		}
	}
	doCallbacksFloat32Array(cal_mean, nchan, P_MEAN_EGU, 0);
	doCallbacksInt32Array(mean32, nchan, P_MEAN_RAW, 0);
}

template<class T>
void SlowmonDriver<T>::handle_buffer()
{
	assert(0);
}

#define CLAMPR(xx, ll, rr) ((xx<ll)? (ll): (xx)>=(rr)? (rr): (xx))

void cal_deb(const char *pram, int ii, float* xx){
	printf("%s:[%d] %f %f %f %f\n", pram, ii, xx[ii], xx[ii+1], xx[ii+2], xx[ii+3]);
}

template<class T>
asynStatus SlowmonDriver<T>::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
	int function = pasynUser->reason;
	int addr;
	asynStatus status = asynSuccess;
	const char *paramName;

	TRACE;
	status = parseAsynUser(pasynUser, &function, &addr, &paramName);
	if (status != asynSuccess) return status;

	if (function == P_SLOWMONMS){
		slowmonms = value;
		printf("%s() slowmonms set %d\n", __FUNCTION__, slowmonms);
	}else if (function == P_MEAN_EN){
		enable = value;
		printf("%s() enable set %d\n", __FUNCTION__, enable);
	}else if (function == P_QUERY_ESLO){
		cal_deb("ESLO", CLAMPR(value, 0, nchan-5), set_eslo);
	}else if (function == P_QUERY_EOFF){
		cal_deb("EOFF", CLAMPR(value, 0, nchan-5), set_eoff);
	}else{

		/* All other parameters just get set in parameter list, no need to act on them here */
	}
	return asynPortDriver::writeInt32(pasynUser, value);
}

int floatCopy(float* dst, const float* src, int nelems)
{
	memcpy(dst, src, nelems*sizeof(float));
	return nelems;
}

template<class T>
asynStatus SlowmonDriver<T>::writeFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
                                        size_t nElements)
{
	int function;
	int addr;
	asynStatus status = asynSuccess;
	const char *paramName;
	const char* functionName = "writeFloat32Array";

	TRACE;
	status = parseAsynUser(pasynUser, &function, &addr, &paramName);
	if (status != asynSuccess) return status;

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
				"%s:%s: function=%d, name=%s, value=%p\n",
				driverName, functionName, function, paramName, value);

	assert(addr >= 0 && addr < nsites());

	float* setv = 0;
	int all_fun;

	if (function == P_SITE_ESLO){
		setv = set_eslo;
		all_fun = P_MEAN_ESLO;
	}else if (function == P_SITE_EOFF){
		setv = set_eoff;
		all_fun = P_MEAN_EOFF;
	}else{
		return asynPortDriver::writeFloat32Array(pasynUser, value, nElements);
	}
	float* setv_site = setv + site_off[addr];

	printf("writeFloat32Array %s f:%d a:%d n:%lu %p\n",
			paramName, function, addr, nElements, setv_site);
	floatCopy(setv_site, value, nElements);
	doCallbacksFloat32Array(setv, nchan, all_fun, 0);
	TRACE;
	return status;
}

template<class T>
asynStatus SlowmonDriver<T>::readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,
	                                        size_t nElements, size_t *nIn)
{
	int function;
	int addr;
	asynStatus status = asynSuccess;
	const char *paramName;
	float* src = 0;

	TRACE;
	status = parseAsynUser(pasynUser, &function, &addr, &paramName);
	if (status != asynSuccess) return status;

	asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
				"%s:%s: function=%d, name=%s, value=%p\n",
				driverName, __FUNCTION__, function, paramName, value);

	printf("%s():%d %s f:%d a:%d n:%lu %p\n", __FUNCTION__, __LINE__,
				paramName, function, addr, nElements, src);

	assert(addr >= 0 && addr < nsites());



	if (function == P_MEAN_ESLO){
		src = set_eslo;
	}else if (function == P_MEAN_EOFF){
		src = set_eoff;
	}else if (function == P_SITE_ESLO){
		src = set_eslo + site_off[addr];
	}else if (function == P_SITE_EOFF){
		src = set_eoff + site_off[addr];
	}else{
		return asynPortDriver::readFloat32Array(pasynUser, value, nElements, nIn);
	}

	printf("%s():%d %s f:%d a:%d n:%lu %p\n", __FUNCTION__, __LINE__,
					paramName, function, addr, nElements, src);

	*nIn = floatCopy(value, src, nElements);
	TRACE;
	return status;
}



extern "C" {

	/** EPICS iocsh callable function to call constructor for the testAsynPortDriver class.
	  * \param[in] portName The name of the asyn port driver to be created.
	  * \param[in] nchan number of channels in array
	  * \param[in] data_size 2|4 bytes
	  */
	int slowmonDriverConfigure(const char *portName, int nchan, const char *_sites, const char *_site_nchan, unsigned data_size)
	{
		//return MultiChannelScope::factory(portName, nchan, maxPoints, data_size);
		printf("pgmwashere R1005\n");
		printf("%s, %s, %d, sites:%s site_nchan:%s data_size:%d\n", __FUNCTION__, portName, nchan, _sites, _site_nchan, data_size);


		std::vector<int> sitelist = csv2int(_sites);
		std::vector<int> site_nchan = csv2int(_site_nchan);

		if (data_size == 2){
			new SlowmonDriver<epicsInt16>(portName, nchan, sitelist, site_nchan);
		}else{
			new SlowmonDriver<epicsInt32>(portName, nchan, sitelist, site_nchan);
		}

		return 0;
	}

	/* EPICS iocsh shell commands */

	static const iocshArg initArg0 = { "port", iocshArgString };
	static const iocshArg initArg1 = { "nchan", iocshArgInt };
	static const iocshArg initArg2 = { "sites", iocshArgString };
	static const iocshArg initArg3 = { "site_nchan", iocshArgString };
	static const iocshArg initArg4 = { "data_size", iocshArgInt };
	static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2, &initArg3, &initArg4};
	static const iocshFuncDef initFuncDef = { "slowmonDriverConfigure", 5, initArgs };
	static void initCallFunc(const iocshArgBuf *args)
	{
		slowmonDriverConfigure(args[0].sval, args[1].ival, args[2].sval, args[3].sval, args[4].ival);
	}

	void slowmonDriverRegister(void)
	{
	    iocshRegister(&initFuncDef,initCallFunc);
	}

	epicsExportRegistrar(slowmonDriverRegister);
}
