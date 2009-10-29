/* Copyright 2008 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *     
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */ 
 
 
/*
	Minimalistic threading API.
	
	This implementation borrows heavily from the Haggle "Thread.cpp" file.
*/

#include "thread.h"

#include <stdlib.h>
#if defined(OS_WINDOWS)
#include <windows.h>
#include <vector>
#elif defined(OS_LINUX) || defined(OS_MACOSX)
#define HAS_PTHREADS
#include <pthread.h>
#else
#error "Unknown operating system!"
#endif


struct mutex_s {
#ifdef OS_WINDOWS
	HANDLE mutex;
#else
	pthread_mutex_t mutex;
#endif
};

mutex mutex_create(void)
{
	mutex m;
	
	m = (mutex) malloc(sizeof(struct mutex_s));
	if(m == NULL)
		return NULL;
#if defined(OS_WINDOWS)
	m->mutex = CreateSemaphore(NULL, 1, 1, NULL);
	if(m->mutex == NULL)
		goto fail_mutex;
#else
	pthread_mutex_init(&(m->mutex), NULL);
#endif

	return m;
#if defined(OS_WINDOWS)
fail_mutex:
	free(m);
	return NULL;
#endif
}

void mutex_destroy(mutex m)
{
#if defined(OS_WINDOWS)
	CloseHandle(m->mutex);
#else
	pthread_mutex_destroy(&(m->mutex));
#endif
	free(m);
}

int mutex_lock(mutex m)
#if defined(OS_WINDOWS)
{
	return WaitForSingleObject(m->mutex,INFINITE) == WAIT_OBJECT_0;
}
#else
{
	return pthread_mutex_lock(&(m->mutex)) == 0;
}
#endif

void mutex_unlock(mutex m)
#if defined(OS_WINDOWS)
{
	ReleaseSemaphore(m->mutex,1,NULL);
}
#else
{
	pthread_mutex_unlock(&(m->mutex));
}
#endif

typedef struct run_arg_s {
	void	*func;
	int		param;
} *run_arg;

static
#ifdef OS_WINDOWS
DWORD WINAPI
#else
void *
#endif
	start_thread(run_arg arg)
{
	((void (*)(int))(arg->func))(arg->param);
	free(arg);
#ifdef HAS_PTHREADS
	return NULL;
#else
	return 0;
#endif
}

int thread_start(void thread_func(int), int param)
{
#ifdef OS_WINDOWS
	HANDLE	thrHandle;
	DWORD id;

#else
	int ret = 0;
	pthread_attr_t attr;
	pthread_t	thrHandle;
#endif
	run_arg	arg;
	
	if(thread_func == NULL)
		goto fail_func;
	
	arg = (run_arg) malloc(sizeof(struct run_arg_s));
	if(arg == NULL)
		goto fail_malloc;

	arg->func = (void *)thread_func;
	arg->param = param;

#ifdef OS_WINDOWS
	thrHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_thread, (void *)arg, 0, &id);

	if(thrHandle == NULL)
		goto fail_start;
#else
	
	ret = pthread_attr_init(&attr);
	
	ret = 
		pthread_create(
			&thrHandle, 
			&attr, 
			(void *(*)(void *))start_thread, 
			(void *)arg);

	if(ret != 0)
		goto fail_start;
#endif

	return 1==1;
fail_start:
	free(arg);
fail_malloc:
fail_func:
	return 0==1;
}
