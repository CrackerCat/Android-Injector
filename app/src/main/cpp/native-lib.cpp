#include <jni.h>
#include <string>
#include <android/log.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include "native-lib.h"
#include <dirent.h>
#include <dlfcn.h>
#include "Inject.h"

JavaVM *gJavaVM = NULL;
JNIEnv *gJNIEnv = NULL;
jobject sClassLoader = NULL;

int find_pid_of(const char *process_name) {
    int id;
    pid_t pid = -1;
    DIR *dir;
    FILE *fp;
    char filename[32];
    char cmdline[256];

    struct dirent *entry;

    if (process_name == NULL)
        return -1;

    dir = opendir("/proc/");
    if (dir == NULL)
        return -1;

    while ((entry = readdir(dir)) != NULL) {
        id = atoi(entry->d_name);
        if (id != 0) {
            sprintf(filename, "/proc/%d/cmdline", id);
            fp = fopen(filename, "r");
            if (fp) {
                fgets(cmdline, sizeof(cmdline), fp);
                fclose(fp);

                if (strcmp(process_name, cmdline) == 0) {
                    /* process found */
                    pid = id;
                    break;
                }
            }
        }
    }

    closedir(dir);
    return pid;
}


jint (*GetCreatedJavaVMs)(JavaVM **, jsize, jsize *) = NULL;

static void init_gvar() {
#ifdef __aarch64__
#define ART_PATH "/system/lib64/libart.so"
#define DVM_PATH "/system/lib64/libdvm.so"
#else
#define ART_PATH "/system/lib/libart.so"
#define DVM_PATH "/system/lib/libdvm.so"
#endif

    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "init");
    jsize size = 0;
    void *handle = NULL;
    if (access(DVM_PATH, F_OK) == 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "init_dvm");
        handle = dlopen(DVM_PATH, RTLD_NOW | RTLD_GLOBAL);
    } else if (access(ART_PATH, F_OK) == 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "init_art");
        handle = dlopen(ART_PATH, RTLD_NOW | RTLD_GLOBAL);
    }
    if (!handle) {
        return;
    }
    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "init_dlopen VM => %p", handle);
    GetCreatedJavaVMs = (jint (*)(JavaVM **, jsize, jsize *)) dlsym(handle,
                                                                    "JNI_GetCreatedJavaVMs");
    if (!GetCreatedJavaVMs) {
        return;
    }
    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "init_dlsym GetCreatedJavaVMs => %p",
                        GetCreatedJavaVMs);
    GetCreatedJavaVMs(&gJavaVM, 1, &size);
    if (size >= 1) {
        __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "init_GetCreatedJavaVMs => %p", &gJavaVM);
        gJavaVM->GetEnv((void **) &gJNIEnv, JNI_VERSION_1_6);
        if (gJNIEnv) {
            __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "init_GetEnv => %p", &gJNIEnv);
            jclass dex_class_loader = gJNIEnv->FindClass("java/lang/ClassLoader");
            if (dex_class_loader) {
                __android_log_print(ANDROID_LOG_DEBUG, "jnitest",
                                    "init_FindClass ClassLoader => %p", &dex_class_loader);
                jmethodID get_system_class_loader = gJNIEnv->GetStaticMethodID(dex_class_loader,
                                                                               "getSystemClassLoader",
                                                                               "()Ljava/lang/ClassLoader;");
                if (get_system_class_loader) {
                    __android_log_print(ANDROID_LOG_DEBUG, "jnitest",
                                        "init_GetMethodID getSystemClassLoader => %p",
                                        get_system_class_loader);
                    sClassLoader = gJNIEnv->CallStaticObjectMethod(dex_class_loader,
                                                                   get_system_class_loader);
                    __android_log_print(ANDROID_LOG_DEBUG, "jnitest",
                                        "init_CallMethod getSystemClassLoader=> %p", &sClassLoader);
                }
            }
        }
    }
}

jobject load_module(char *filepath) {

    if (sClassLoader) {
        jclass path_class_loader = gJNIEnv->FindClass("dalvik/system/PathClassLoader");
        if (path_class_loader) {
            jmethodID cort = gJNIEnv->GetMethodID(path_class_loader, "<init>",
                                                  "(Ljava/lang/String;Ljava/lang/ClassLoader;)V");
            if (cort) {
                return gJNIEnv->NewObject(path_class_loader, cort, gJNIEnv->NewStringUTF(filepath),
                                          sClassLoader);
            }

        }
    }
    return NULL;
}

int entry() {
    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "entry_3");
    init_gvar();
    jobject loader = load_module("/data/app/cn.hluwa.injector-1/base.apk");
    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "loader = %p", loader);
    jclass clazz = gJNIEnv->FindClass("java/lang/ClassLoader");
    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "clazz = %p", clazz);
    jmethodID forclass = gJNIEnv->GetMethodID(clazz, "loadClass",
                                              "(Ljava/lang/String;)Ljava/lang/Class;");
    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "loadClass = %p", forclass);
    if (!forclass) {
        return 0x100;
    }
    jstring MainActivity = gJNIEnv->NewStringUTF("cn.hluwa.injector.MainActivity");
    jobject cls = gJNIEnv->CallObjectMethod(loader, forclass, MainActivity);
//    jclass cls = gJNIEnv->FindClass("cn/hluwa/injector/MainActivity");
    __android_log_print(ANDROID_LOG_DEBUG, "jnitest", "cls = %p", cls);
//    gJNIEnv->CallStaticVoidMethod()
    return 0x100;
}

#if defined(__aarch64__)
#define ZYGOTE_NAME "zygote64"
#define SO_PATH "/data/local/libnative-lib64.so"
#else
#define ZYGOTE_NAME "zygote"
#define SO_PATH "/data/local/libnative-lib.so"
#endif

#define __DEBUG__ 0

int main(int argc, char *args[]) {
    char *load_file;
    if (__DEBUG__)
        load_file = SO_PATH;
    else {
        if (argc < 2) {
            return -1;
        }
        load_file = args[1];
    }
    system("su -c setenforce 0");
    Inject *injector = new Inject(find_pid_of(ZYGOTE_NAME));
    injector->call_sym(load_file, "_Z5entryv", NULL, 0);
    delete injector;
    return 0;
}