//
// Created by hluwa on 2018/8/19.
//

#ifndef JNITEST_NATIVE_LIB_H
#define JNITEST_NATIVE_LIB_H

#include <jni.h>
#include <sys/types.h>

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *);

#endif //JNITEST_NATIVE_LIB_H
