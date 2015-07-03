/* openmp_test -- Test how perf_event deals with an openmp program	*/

/* by Vince Weaver, vincent.weaver _at_ maine.edu			*/

#include <stdio.h>
#include <stdlib.h>

#include <omp.h>

#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "perf_event.h"
#include "test_utils.h"
#include "perf_helpers.h"
#include "instructions_testcode.h"



char test_string[]="Testing OpenMP results...";
int quiet=0;
int fd;


int main (int argc, char **argv) {

	int nthreads, num_threads=0, tid, fd, result, read_result,i;
	struct perf_event_attr pe;
	long long count;
	int errors=0;

	quiet=test_quiet();

	if (!quiet) {
		printf("Testing OpenMP behavior\n\n");
		printf("Testing the inherit case\n");
		printf("Note!!! Inherit only works if the event is created\n");
		printf("before the fork happens!  OpenMP creates thread pools\n");
		printf("so if you create the event after the thread pools hae been\n");
		printf("created it may be too late and you won't get child events!\n\n");
	}


	/****************************************/
	/* Inherit				*/
	/****************************************/

	if (!quiet) {
		printf("Testing the inherit case\n");
	}

	memset(&pe,0,sizeof(struct perf_event_attr));
	pe.type=PERF_TYPE_HARDWARE;
	pe.size=sizeof(struct perf_event_attr);
	pe.config=PERF_COUNT_HW_INSTRUCTIONS;
	pe.disabled=1;
	pe.exclude_kernel=1;
	pe.exclude_hv=1;
	pe.inherit=1;
	pe.inherit_stat=1;

	arch_adjust_domain(&pe,quiet);

	fd=perf_event_open(&pe,0,-1,-1,0);
	if (fd<0) {
		fprintf(stderr,"Error opening event %llx %s\n",
			pe.config,strerror(errno));
		test_fail(test_string);
	}

	ioctl(fd, PERF_EVENT_IOC_ENABLE,0);

	/* Start a parallel group of threads */
#pragma omp parallel private(nthreads, tid)
{

	/* Obtain thread number */
	tid = omp_get_thread_num();

	/* Only master thread does this */
	if (tid == 0) {
		nthreads = omp_get_num_threads();
		num_threads = nthreads;
		if (!quiet) {
			printf("\tRunning with %d threads\n", nthreads);
		}
	}

	if (!quiet) {
		printf("\t+ Running 10 million instructions in thread %d\n", tid);
	}

	for(i=0;i<10;i++) {
		result=instructions_million();
	}


}
	/* All threads join master thread and disband */

	ioctl(fd, PERF_EVENT_IOC_DISABLE,0);
	read_result=read(fd,&count,sizeof(long long));

	if (result==CODE_UNIMPLEMENTED) {
		fprintf(stderr,"\tCode unimplemented\n");
		test_fail(test_string);
	}

	if (read_result!=sizeof(long long)) {
		fprintf(stderr,"\tImproper return from read: %d\n",read_result);
		test_fail(test_string);
	}

	if (!quiet) {
		printf("\tCount=%lldM, expected roughly %dM\n",
			count/1000000,10*num_threads);
	}
	close(fd);

	if ((count < 10000000*num_threads) || (count > 20000000*num_threads)) {
		if (!quiet) {
			printf("Error!  Count too low!\n");
		}
		errors++;
	}


	/************************************************/
	/* With non-inherit				*/
	/************************************************/

	if (!quiet) {
		printf("\nTesting the non-inherit case\n");
	}

	memset(&pe,0,sizeof(struct perf_event_attr));
	pe.type=PERF_TYPE_HARDWARE;
	pe.size=sizeof(struct perf_event_attr);
	pe.config=PERF_COUNT_HW_INSTRUCTIONS;
	pe.disabled=1;
	pe.exclude_kernel=1;
	pe.exclude_hv=1;

	/* Inherit does nothing as openmp typically forks off a thread pool */
	/* The first time, so the following never calls fork so the event   */
	/* Is not inherited */
	pe.inherit=1;
	pe.inherit_stat=1;

	arch_adjust_domain(&pe,quiet);

	fd=perf_event_open(&pe,0,-1,-1,0);
	if (fd<0) {
		fprintf(stderr,"Error opening event %llx %s\n",
			pe.config,strerror(errno));
		test_fail(test_string);
	}

	ioctl(fd, PERF_EVENT_IOC_ENABLE,0);

	/* Start a parallel group of threads */
#pragma omp parallel private(nthreads, tid)
{

	/* Obtain thread number */
	tid = omp_get_thread_num();

	/* Only master thread does this */
	if (tid == 0) {
		nthreads = omp_get_num_threads();
		printf("\tRunning with %d threads\n", nthreads);
	}

	printf("\t+ Running 10 million instructions in thread %d\n", tid);

	{
		int i;

		for(i=0;i<10;i++) {
			result=instructions_million();
		}

	}

}
	/* All threads join master thread and disband */

	ioctl(fd, PERF_EVENT_IOC_DISABLE,0);
	read_result=read(fd,&count,sizeof(long long));

	if (result==CODE_UNIMPLEMENTED) {
		fprintf(stderr,"\tCode unimplemented\n");
		test_fail(test_string);
	}

	if (read_result!=sizeof(long long)) {
		fprintf(stderr,"\tImproper return from read: %d\n",read_result);
		test_fail(test_string);
	}

	if (!quiet) {
		printf("\tCount=%lldM, expected roughly 10M "
			"(only counted thread 0)\n",count/1000000);
	}
	if ((count < 10000000) || (count > 20000000)) {
		if (!quiet) {
			printf("Error!  Count too high!\n");
		}
		errors++;
	}


	close(fd);

	if (errors==0) {
		test_pass(test_string);
	}
	else {
		test_fail(test_string);
	}

	return 0;
}

