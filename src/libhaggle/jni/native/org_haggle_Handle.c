#include "org_haggle_Handle.h"
#include "javaclass.h"

#include <stdio.h>
#include <string.h>
#include <libhaggle/haggle.h>
#include <jni.h>

#include "common.h"

LIST(cdlist);

typedef struct callback_data {
        list_t l;
        haggle_handle_t hh;
	int type;
	JNIEnv *env;
        jclass cls;
	jobject obj;
} callback_data_t;

static void callback_data_free(callback_data_t *cd)
{
        if (!cd)
                return;

        (*cd->env)->DeleteGlobalRef(cd->env, cd->obj);
	(*cd->env)->DeleteGlobalRef(cd->env, cd->cls);
        free(cd);
}

static callback_data_t *callback_list_get(haggle_handle_t hh, int type)
{
	list_t *pos;
	
	list_for_each(pos, &cdlist) {
                callback_data_t *e = (callback_data_t *)pos;
                
                if (e->type == type && e->hh == hh)
			return e;
	}
	return NULL;
}


static callback_data_t *callback_list_insert(haggle_handle_t hh, int type, JNIEnv *env, jobject obj)
{
        callback_data_t *cd;
        jclass cls;

        if (callback_list_get(hh, type) != NULL)
                return NULL;

        cd = (callback_data_t *)malloc(sizeof(callback_data_t));

        if (!cd)
                return NULL;

        cd->hh = hh;
	cd->type = type;
	cd->env = env;
        // We must retain this object
	cd->obj = (*env)->NewGlobalRef(env, obj);
        // Cache the object class
        cls = (*env)->GetObjectClass(env, cd->obj);
	
	if (!cls) {
		free(cd);
		return NULL;
	}

	cd->cls = (*env)->NewGlobalRef(env, cls);

        list_add(&cd->l, &cdlist);

        return cd;
}

static int callback_list_erase_all_with_handle(haggle_handle_t hh)
{
        int n = 0;
        list_t *pos, *tmp;
        
        list_for_each_safe(pos, tmp, &cdlist) {
                callback_data_t *cd = (callback_data_t *)pos;
                
                if (cd->hh == hh) {
                        list_detach(&cd->l);
			n++;
                        callback_data_free(cd);
                }
        }
        
        haggle_handle_free(hh);

        return n;
}

static void on_event_loop_start(void *arg)
{
/* 
   The definition of AttachCurrentThread seems to be different depending
   on platform. We use this define just to avoid compiler warnings.
 */
#if defined(OS_ANDROID)
        JNIEnv *env; 
#else
        void *env;
#endif
        if ((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK) {
                fprintf(stderr, "libhaggle_jni: Could not attach thread\n");
        }
}

static void on_event_loop_stop(void *arg)
{        
        if ((*jvm)->DetachCurrentThread(jvm) != JNI_OK) {
                fprintf(stderr, "libhaggle_jni: Could not detach thread\n");
        }
}

static int event_handler(haggle_event_t *e, void *arg)
{
	callback_data_t *cd = (callback_data_t *)arg;
        jmethodID mid = 0;
        JNIEnv *env;
        int ret = 0;
     
        if (!e || !cd)
                return -1;

        env = get_jni_env();

        if (!env)
		return -1;

	if ((*env)->PushLocalFrame(env, 20) < 0) {
		return -1; 
	}

	switch (cd->type) {
                case LIBHAGGLE_EVENT_SHUTDOWN:
                        mid = (*env)->GetMethodID(env, cd->cls, "onShutdown", "(I)V");
                        if (mid) {
                                (*env)->CallVoidMethod(env, cd->obj, mid, (jint)e->shutdown_reason);

				if ((*env)->ExceptionCheck(env)) {
					fprintf(stdout, "An exception occurred when calling onShutdown()\n");
				}
                        }
                        break;
                case LIBHAGGLE_EVENT_NEIGHBOR_UPDATE:
                        mid = (*env)->GetMethodID(env, cd->cls, "onNeighborUpdate", "([Lorg/haggle/Node;)V");
                        if (mid) {
				jobjectArray jarr = libhaggle_jni_nodelist_to_node_jobjectArray(env, e->neighbors);
				
				if (!jarr)
					break;

                                (*env)->CallVoidMethod(env, cd->obj, mid, jarr);

				if ((*env)->ExceptionCheck(env)) {
					fprintf(stdout, "An exception occurred when calling onNeighborUpdate()\n");
				}
				
				(*env)->DeleteLocalRef(env, jarr);
                        }
                        break;
                case LIBHAGGLE_EVENT_NEW_DATAOBJECT:                
                        mid = (*env)->GetMethodID(env, cd->cls, "onNewDataObject", "(Lorg/haggle/DataObject;)V");	
                        if (mid) {
				jobject jdobj = java_object_new(env, JCLASS_DATAOBJECT, e->dobj);

				if (!jdobj)
					break;

                                (*env)->CallVoidMethod(env, cd->obj, mid, jdobj);
				 
				if ((*env)->ExceptionCheck(env)) {
					fprintf(stdout, "An exception occurred when calling onNewDataObject()\n");
				}

				(*env)->DeleteLocalRef(env, jdobj);
                        }
                        /* For this event the data object is turned
                         * into a java data object which will be garbage
                         * collected. */
                        e->dobj = NULL;
                        ret = 1;
                        break;
                case LIBHAGGLE_EVENT_INTEREST_LIST:
                        mid = (*env)->GetMethodID(env, cd->cls, "onInterestListUpdate", "([Lorg/haggle/Attribute;)V");	
                        if (mid) {
				jobjectArray jarr = libhaggle_jni_attributelist_to_attribute_jobjectArray(env, e->interests);

				if (!jarr)
					break;

                                (*env)->CallVoidMethod(env, cd->obj, mid, jarr);
				 
				if ((*env)->ExceptionCheck(env)) {
					fprintf(stdout, "An exception occurred when calling onInterestListUpdate()\n");
				}

				(*env)->DeleteLocalRef(env, jarr);
                        }
                        break;
                default:
                        break;
	}

	(*env)->PopLocalFrame(env, NULL);

	return ret;
}

/*
 * Class:     org_haggle_Handle
 * Method:    getHandle
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_getHandle(JNIEnv *env, jobject obj, jstring name)
{
        haggle_handle_t hh;
        const char *str;
        int ret = 0;
       
        str = (*env)->GetStringUTFChars(env, name, 0); 
        
        if (!str)
                return -1;

        ret = haggle_handle_get(str, &hh);

        (*env)->ReleaseStringUTFChars(env, name, str);

        if (ret == HAGGLE_NO_ERROR) {
                set_native_handle(env, JCLASS_HANDLE, obj, hh); 
        }

        return ret;
}

/*
 * Class:     org_haggle_Handle
 * Method:    nativeFree
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_haggle_Handle_nativeFree(JNIEnv *env, jobject obj)
{
        callback_list_erase_all_with_handle((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj));
}

/*
 * Class:     org_haggle_Handle
 * Method:    unregister
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_haggle_Handle_unregister(JNIEnv *env, jobject obj, jstring name)
{
        const char *str;

        str = (*env)->GetStringUTFChars(env, name, 0); 
        
        if (!str)
                return;

        haggle_unregister(str);
        
        (*env)->ReleaseStringUTFChars(env, name, str);
}

/*
 * Class:     org_haggle_Handle
 * Method:    getSessionId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_getSessionId(JNIEnv *env, jobject obj)
{
        return haggle_handle_get_session_id((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj));
}

/*
 * Class:     org_haggle_Handle
 * Method:    shutdown
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_shutdown(JNIEnv *env, jobject obj)
{
	return haggle_ipc_shutdown((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj));
}

/*
 * Class:     org_haggle_Handle
 * Method:    registerEventInterest
 * Signature: (ILorg/haggle/EventHandler;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_registerEventInterest(JNIEnv *env, jobject obj, jint type, jobject handler)
{
        callback_data_t *cd;
	haggle_handle_t hh;
        jint res;

        hh = (haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj);
        
        if (!hh)
                return -1;

        cd = callback_list_insert(hh, type, env, handler);
	
	if (!cd)
		return -1;

        res = haggle_ipc_register_event_interest_with_arg(hh, (int)type, event_handler, cd);

	if (res != HAGGLE_NO_ERROR)
                return res;

        return res;
}
/*
 * Class:     org_haggle_Handle
 * Method:    registerInterest
 * Signature: (Lorg/haggle/Attribute;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_registerInterest(JNIEnv *env, jobject obj, jobject attribute)
{
        haggle_attr_t *attr = (haggle_attr_t *)get_native_handle(env, JCLASS_ATTRIBUTE, attribute);
        
        if (!attr)
                return -1;

        return haggle_ipc_add_application_interest_weighted((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj), haggle_attribute_get_name(attr), haggle_attribute_get_value(attr), haggle_attribute_get_weight(attr));
}

/*
 * Class:     org_haggle_Handle
 * Method:    registerInterests
 * Signature: ([Lorg/haggle/Attribute;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_registerInterests(JNIEnv *env, jobject obj, jobjectArray attrArr)
{
        haggle_attrlist_t *al;
        int i, ret;

        al = haggle_attributelist_new();
        
        if (!al)
                return -1;

        if ((*env)->GetArrayLength(env, attrArr) == 0)
                return 0;
        
        for (i = 0; i < (*env)->GetArrayLength(env, attrArr); i++) {
		jobject jattr = (*env)->GetObjectArrayElement(env, attrArr, i);
                haggle_attr_t *attr = (haggle_attr_t *)get_native_handle(env, JCLASS_ATTRIBUTE, jattr);
                haggle_attributelist_add_attribute(al, haggle_attribute_copy(attr));
		(*env)->DeleteLocalRef(env, jattr);
        }

        ret = haggle_ipc_add_application_interests((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj), al);

        haggle_attributelist_free(al);

        return ret;
}

/*
 * Class:     org_haggle_Handle
 * Method:    unregisterInterest
 * Signature: (Lorg/haggle/Attribute;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_unregisterInterest(JNIEnv *env, jobject obj, jobject attribute)
{
        haggle_attr_t *attr = (haggle_attr_t *)get_native_handle(env, JCLASS_ATTRIBUTE, attribute);
        
        if (!attr)
                return -1;

        return haggle_ipc_remove_application_interest((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj), haggle_attribute_get_name(attr), haggle_attribute_get_value(attr));
}

/*
 * Class:     org_haggle_Handle
 * Method:    unregisterInterests
 * Signature: ([Lorg/haggle/Attribute;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_unregisterInterests(JNIEnv *env, jobject obj, jobjectArray attrArr)
{
        haggle_attrlist_t *al;
        int i, ret;

        al = haggle_attributelist_new();
        
        if (!al)
                return -1;

        if ((*env)->GetArrayLength(env, attrArr) == 0)
                return 0;
        
        for (i = 0; i < (*env)->GetArrayLength(env, attrArr); i++) {
		jobject jattr = (*env)->GetObjectArrayElement(env, attrArr, i);
                haggle_attr_t *attr = (haggle_attr_t *)get_native_handle(env, JCLASS_ATTRIBUTE, jattr);
                haggle_attributelist_add_attribute(al, haggle_attribute_copy(attr));
		(*env)->DeleteLocalRef(env, jattr);
        }

        ret = haggle_ipc_remove_application_interests((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj), al);

        haggle_attributelist_free(al);

        return ret;
}


/*
 * Class:     org_haggle_Handle
 * Method:    getApplicationInterestsAsync
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_getApplicationInterestsAsync(JNIEnv *env, jobject obj)
{
        return (jint)haggle_ipc_get_application_interests_async((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj));
}

/*
 * Class:     org_haggle_Handle
 * Method:    getDataObjectsAsync
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_getDataObjectsAsync(JNIEnv *env, jobject obj)
{
        return (jint)haggle_ipc_get_data_objects_async((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj));
}

/*
 * Class:     org_haggle_Handle
 * Method:    deleteDataObjectById
 * Signature: ([C)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_deleteDataObjectById(JNIEnv *env, jobject obj, jcharArray idArray)
{
        unsigned char id[20];
        jchar *carr;
        int i;

        if ((*env)->GetArrayLength(env, idArray) != 20)
                return -1;
        
        carr = (*env)->GetCharArrayElements(env, idArray, NULL);

        if (carr == NULL) {                
                return -1;
        }

        for (i = 0; i < 20; i++) {
                id[i] = (unsigned char)carr[i];
        }
        
        (*env)->ReleaseCharArrayElements(env, idArray, carr, 0);

        return (jint)haggle_ipc_delete_data_object_by_id((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj), id);
}
/*
 * Class:     org_haggle_Handle
 * Method:    deleteDataObject
 * Signature: (Lorg/haggle/DataObject;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_deleteDataObject(JNIEnv *env, jobject obj, jobject dObj)
{
        return (jint)haggle_ipc_delete_data_object((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj), (haggle_dobj_t *)get_native_handle(env, JCLASS_DATAOBJECT, dObj));
}

/*
 * Class:     org_haggle_Handle
 * Method:    sendNodeDescription
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_sendNodeDescription(JNIEnv *env, jobject obj)
{
	return (jint)haggle_ipc_send_node_description((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj));
}

/*
 * Class:     org_haggle_Handle
 * Method:    publishDataObject
 * Signature: (Lorg/haggle/DataObject;)I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_publishDataObject(JNIEnv *env, jobject obj, jobject jdObj)
{	
	struct dataobject *dobj;

	dobj = (struct dataobject *)get_native_handle(env, JCLASS_DATAOBJECT, jdObj);
	
	if (!dobj)
		return -1;
	
	return haggle_ipc_publish_dataobject((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj), dobj);
}

/*
 * Class:     org_haggle_Handle
 * Method:    eventLoopRunAsync
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_eventLoopRunAsync(JNIEnv *env, jobject obj)
{       
        haggle_handle_t hh = (haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj);

        if (!hh)
                return JNI_FALSE;
        
        if (haggle_event_loop_register_callbacks(hh, on_event_loop_start, on_event_loop_stop, NULL) != HAGGLE_NO_ERROR)
                return JNI_FALSE;
        
        return haggle_event_loop_run_async(hh) == HAGGLE_NO_ERROR ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_haggle_Handle
 * Method:    eventLoopRun
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_eventLoopRun(JNIEnv *env, jobject obj)
{
        return haggle_event_loop_run((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj)) == HAGGLE_NO_ERROR ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_haggle_Handle
 * Method:    eventLoopStop
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_eventLoopStop(JNIEnv *env, jobject obj)
{
        return haggle_event_loop_stop((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj)) == HAGGLE_NO_ERROR ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_haggle_Handle
 * Method:    eventLoopIsRunning
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_eventLoopIsRunning(JNIEnv *env, jobject obj)
{
        return haggle_event_loop_is_running((haggle_handle_t)get_native_handle(env, JCLASS_HANDLE, obj)) == 1 ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_haggle_Handle
 * Method:    getDaemonPid
 * Signature: ()I
 */
JNIEXPORT jlong JNICALL Java_org_haggle_Handle_getDaemonPid(JNIEnv *env, jclass cls)
{
        unsigned long pid;

	int ret = haggle_daemon_pid(&pid);

        /* Check if Haggle is running */
        if (ret == 1)
                return (jlong)pid;

        return (jlong)ret;
}

static jobject spawn_object;

static int spawn_daemon_callback(unsigned int milliseconds) 
{
        int ret = 0;
        jmethodID mid = 0; 
        JNIEnv *env;
        jclass cls;

        env = get_jni_env();
        
        if (!env)
                return -1;
        
        cls = (*env)->GetObjectClass(env, spawn_object);

	if (!cls)
		return -1;

        mid = (*env)->GetMethodID(env, cls, "callback", "(J)I");
	
	(*env)->DeleteLocalRef(env, cls);

        if (mid) {
                ret = (*env)->CallIntMethod(env, spawn_object, mid, (jlong)milliseconds);
						
		if ((*env)->ExceptionCheck(env)) {
			(*env)->DeleteGlobalRef(env, spawn_object);
			spawn_object = NULL;
			return -1;
		}
        } else {
                (*env)->DeleteGlobalRef(env, spawn_object);
		spawn_object = NULL;
                return -1;
        }

        if (milliseconds == 0 || ret == 1) {
                (*env)->DeleteGlobalRef(env, spawn_object);
		spawn_object = NULL;
        }

        return ret;
}

/*
 * Class:     org_haggle_Handle
 * Method:    spawnDaemon
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_spawnDaemon__(JNIEnv *env, jclass cls)
{
         return haggle_daemon_spawn(NULL) >= 0 ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_haggle_Handle
 * Method:    spawnDaemon
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_spawnDaemon__Ljava_lang_String_2(JNIEnv *env, jclass cls, jstring path)
{
        const char *daemonpath;
        jint ret;

        daemonpath = (*env)->GetStringUTFChars(env, path, 0); 
        
        if (!daemonpath)
                return JNI_FALSE;

        ret = haggle_daemon_spawn(daemonpath);

        (*env)->ReleaseStringUTFChars(env, path, daemonpath);

        return ret >= 0 ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_haggle_Handle
 * Method:    spawnDaemon
 * Signature: (Lorg/haggle/LaunchCallback;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_spawnDaemon__Lorg_haggle_LaunchCallback_2(JNIEnv *env, jclass cls, jobject obj)
{
	if (spawn_object) {
		(*env)->DeleteGlobalRef(env, spawn_object);
	}

        spawn_object = (*env)->NewGlobalRef(env, obj);

	if (!spawn_object)
		return JNI_FALSE;

        return haggle_daemon_spawn_with_callback(NULL, spawn_daemon_callback) >= 0 ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_haggle_Handle
 * Method:    spawnDaemon
 * Signature: (Ljava/lang/String;Lorg/haggle/LaunchCallback;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_haggle_Handle_spawnDaemon__Ljava_lang_String_2Lorg_haggle_LaunchCallback_2(JNIEnv *env, jclass cls, jstring path, jobject obj)
{
        const char *daemonpath;
        jint ret;

	if (spawn_object) {
		(*env)->DeleteGlobalRef(env, spawn_object);
	}

        spawn_object = (*env)->NewGlobalRef(env, obj);

	if (!spawn_object)
		return JNI_FALSE;

        daemonpath = (*env)->GetStringUTFChars(env, path, 0); 
        
        if (!daemonpath)
                return JNI_FALSE;

        ret = haggle_daemon_spawn_with_callback(daemonpath, spawn_daemon_callback);

        (*env)->ReleaseStringUTFChars(env, path, daemonpath);

        return ret >= 0 ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_haggle_Handle
 * Method:    getDaemonStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_haggle_Handle_getDaemonStatus(JNIEnv *env, jclass cls)
{
        return haggle_daemon_pid(NULL);
}
