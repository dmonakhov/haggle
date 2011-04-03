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
#include <jni.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>

#include "Trace.h"
#include "Utility.h"
#include "HaggleKernel.h"

static JavaVM *jvm = NULL;
extern HaggleKernel *kernel;
extern int run_haggle(void);
extern void cleanup();

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_haggle_kernel_Haggle
 * Method:    nativeInit
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_kernel_Haggle_nativeInit
  (JNIEnv *, jclass);

/*
 * Class:     org_haggle_kernel_Haggle
 * Method:    mainLoop
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_kernel_Haggle_mainLoop
  (JNIEnv *, jobject, jstring);

/*
 * Class:     org_haggle_kernel_Haggle
 * Method:    shutdown
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_kernel_Haggle_shutdown
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif

/*
  JNI glue for running Haggle as an Android service.
*/
jint Java_org_haggle_kernel_Haggle_mainLoop(JNIEnv *env, 
					    jobject obj, 
					    jstring fileDirPath)
{	
        const char *str = env->GetStringUTFChars(fileDirPath, 0); 
        
        if (!str)
                return -1;
	
	HAGGLE_DBG("Data path is %s\n", str);
	
	char *tmp = (char *)malloc(strlen(str) + 1);

	if (tmp) {
		strcpy(tmp, str);

		// ddsp -- Haggle default data store path, defined in Utility.h
		if (ddsp) {
			free((void *)ddsp);
		}
		ddsp = tmp;
	}
	
        env->ReleaseStringUTFChars(fileDirPath, str);
	
	int ret = run_haggle();

	cleanup();

	return ret;
}

jint Java_org_haggle_kernel_Haggle_shutdown(JNIEnv *env, jobject obj)
{	
	if (kernel)
		kernel->shutdown();
	return 0;
}

jint Java_org_haggle_kernel_Haggle_nativeInit(JNIEnv *env, jclass cls)
{
	/* In case we need to do some initialization (in the future) */
	return 0;
}

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
        JNIEnv *env;
	jclass cls;

        jvm = vm;

        if (vm->GetEnv((void **)&env, JNI_VERSION_1_4) != JNI_OK) {
                fprintf(stderr, "Could not get JNI env in JNI_OnLoad\n");
                return -1;
        }

	return JNI_VERSION_1_4;
}

void JNI_OnUnload(JavaVM *vm, void *reserved)
{
	JNIEnv *env = NULL;
	
        if (vm->GetEnv((void **)&env, JNI_VERSION_1_4) != JNI_OK) {
                fprintf(stderr, "Could not get JNI env in JNI_OnUnload\n");
                return;
        }         
}
