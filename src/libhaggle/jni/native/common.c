#include "common.h"
#include "javaclass.h"

#include <libhaggle/attribute.h>

jobjectArray libhaggle_jni_dataobject_to_node_jobjectArray(JNIEnv *env, haggle_dobj_t *dobj)
{
        jobjectArray nodeArray;
        haggle_nodelist_t *nl;
        int i = 0;

        nl = haggle_nodelist_new_from_dataobject(dobj);
        
        if (!nl)
                return NULL;
        
        nodeArray = (*env)->NewObjectArray(env, haggle_nodelist_size(nl), java_object_class(JCLASS_NODE), NULL);

        while (haggle_nodelist_size(nl)) {
                (*env)->SetObjectArrayElement(env, nodeArray, i++, java_object_new(env, JCLASS_NODE, haggle_nodelist_pop(nl)));
        }

        /* Since we created a new list from the data object we also need to free it. */
        haggle_nodelist_free(nl);

        return nodeArray;
}

jobjectArray libhaggle_jni_dataobject_to_attribute_jobjectArray(JNIEnv *env, haggle_dobj_t *dobj)
{
        jobjectArray attrArray;
        haggle_attrlist_t *al;
        list_t *pos;

        int i = 0;

        /* This list belongs to the data object and should not be
         * free'd. We need to make copies of the attributes that we
         * add to the Java array.  */
        al = haggle_dataobject_get_attributelist(dobj);
        
        if (!al)
                return NULL;
        
        attrArray = (*env)->NewObjectArray(env, haggle_attributelist_size(al), java_object_class(JCLASS_ATTRIBUTE), NULL);

	list_for_each(pos, &al->attributes) {
		struct attribute *a = (struct attribute *)pos;
                (*env)->SetObjectArrayElement(env, attrArray, i++, java_object_new(env, JCLASS_ATTRIBUTE, haggle_attribute_copy(a)));
        }

        return attrArray;
}
