#include <jni.h>

JNIEXPORT jboolean JNICALL
Java_com_attunehelper_companion_CredentialsStore_isStubAvailableNative(JNIEnv *env, jclass clazz)
{
    (void)env;
    (void)clazz;
    return JNI_TRUE;
}
