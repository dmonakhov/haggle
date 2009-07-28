#ifndef LIBHAGGLE_JNI_COMMON_H
#define LIBHAGGLE_JNI_COMMON_H

#include <libhaggle/haggle.h>
#include <jni.h>

jobjectArray libhaggle_jni_dataobject_to_node_jobjectArray(JNIEnv *env, haggle_dobj_t *dobj);
jobjectArray libhaggle_jni_dataobject_to_attribute_jobjectArray(JNIEnv *env, haggle_dobj_t *dobj);

#endif /* LIBHAGGLE_JNI_COMMON_H */
