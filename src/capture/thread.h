/*
 * thread.h
 *
 *  Created on: Dec 29, 2016
 *      Author: wang
 */

#ifndef APP_THREAD_H_
#define APP_THREAD_H_
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
static pthread_t start_thread(void *(*__start_routine) (void *), void * __arg)
{
	pthread_t ret ;
	pthread_attr_t attr;
	struct sched_param param;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	param.sched_priority = 50;
	pthread_attr_setschedparam(&attr, &param);
	int id = pthread_create(&ret, &attr, __start_routine,__arg);
	if(id != 0)
	{
		printf("error pthread_create failed\n");
	}
	pthread_attr_destroy(&attr);
//	pthread_mutex_init(&m_Mutex, NULL);
	return ret;
}

#endif /* APP_THREAD_H_ */
