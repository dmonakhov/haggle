/* Copyright 2008-2009 Uppsala University
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
#include <libcpphaggle/Mutex.h>
#include <libcpphaggle/Thread.h>

namespace haggle {

bool Mutex::lock()
{
	bool ret;
#ifdef DEBUG_MUTEX
	thread_id_t thisid = Thread::selfGetId();

	printf("***Thread %lu tries to grab mutex \"%s\". Lock count=%d\n", 
	       (unsigned long)thisid, name.c_str(), lockCount);

	lastid = thisid;
#endif
#if defined(OS_WINDOWS)
	ret = WaitForSingleObject(mutex, INFINITE) == WAIT_OBJECT_0;
#else
	ret = pthread_mutex_lock(&mutex) == 0;
#endif

#ifdef DEBUG_MUTEX
	lockCount++;

	if (ret)
		printf("***Thread %lu successfully grabbed mutex \"%s\". Lock count=%d\n", 
			   (unsigned long)thisid, name.c_str(), lockCount);
	else
		printf("***Thread %lu failed to grab mutex \"%s\". Lock count=%d\n", 
		       (unsigned long)thisid, name.c_str(), lockCount);
#endif	
	return ret;
}

bool Mutex::trylock()
{
#ifdef DEBUG_MUTEX
	thread_id_t thisid = Thread::selfGetId();

	printf("***Thread %lu tries to grab mutex \"%s\". Lock count=%d\n", 
	       (unsigned long)thisid, name.c_str(), lockCount);
	
	lastid = thisid;
#endif
#if defined(OS_WINDOWS)
	bool ret = WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0;
#else
	bool ret = pthread_mutex_trylock(&mutex) == 0;
#endif
	
	if (ret) {
#ifdef DEBUG_MUTEX
		lockCount++;
		printf("***Thread %lu successfully grabbed mutex \"%s\". Lock count=%d\n", 
		       (unsigned long)thisid, name.c_str(), lockCount);
#endif
	}
		
#ifdef DEBUG_MUTEX
	else {
		printf("***Thread %lu failed to grab mutex \"%s\". Lock count=%d\n", 
			   (unsigned long)thisid, name.c_str(), lockCount);	
	}
#endif	
	return ret;
}

bool Mutex::unlock()
{
#ifdef DEBUG_MUTEX
	lockCount--;

	printf("***Thread %lu unlocks mutex \"%s\". Lock count=%d\n",
	       (unsigned long)Thread::selfGetId(), name.c_str(), lockCount);
#endif
#if defined(OS_WINDOWS)
	if (_recursive)
		ReleaseMutex(mutex);
	else
		ReleaseSemaphore(mutex, 1, NULL);
#else
	pthread_mutex_unlock(&mutex);
#endif
	return true;
}

Mutex::Mutex(const string _name, bool recursive) :
	name(_name)
#if defined(OS_WINDOWS)
	, _recursive(recursive)
#endif
#ifdef DEBUG_MUTEX
	, lockCount(0),
#endif
{
#if defined(OS_WINDOWS)
	if (recursive)
		mutex = CreateMutex(NULL, NULL, NULL);
	else
		mutex = CreateSemaphore(NULL, 1, 1, NULL);
	
	if (mutex == NULL)
		throw Exception(0, "Unable to create mutex\n");
#else
	if (recursive) {
		pthread_mutexattr_t attr;

		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mutex, &attr);
		pthread_mutexattr_destroy(&attr);
	} else {
		pthread_mutex_init(&mutex, NULL);
	}
#endif
}

Mutex::~Mutex()
{
#if defined(OS_WINDOWS)
	CloseHandle(mutex);
#else
	pthread_mutex_destroy(&mutex);
#endif
}

}; // namespace haggle
