#include "NRSSL.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <jni.h>
#include <string>
#include <tuple>
#include <vector>

JavaVM *NRSSL::jvm = nullptr;

NRSSL::NRSSL() {
    if (!jvm) {
        JavaVMInitArgs vm_args;
        JavaVMOption options[1];

        char *jarsPath = getenv("NRSSL_JARS");
        if (jarsPath == nullptr) {
            std::cerr << "NRSSL_JARS environment variable not set" << std::endl;
            exit(1);
        }

        std::cout << "NRSSL_JARS: " << jarsPath << std::endl;

        options[0].optionString = new char[1024];
        snprintf(options[0].optionString, 1024,
                 "-Djava.class.path=%s/scala-library-2.13.8.jar"
                 ":%s/nrssl.jar",
                 jarsPath, jarsPath);

        vm_args.version = JNI_VERSION_10;
        vm_args.nOptions = 1;
        vm_args.options = options;
        vm_args.ignoreUnrecognized = false;

        auto ret = JNI_CreateJavaVM(&jvm, (void **)&env, &vm_args);
        if (ret != JNI_OK)
            std::cerr << "Failed to create Java VM with error code: " << ret << std::endl;

        delete[] options[0].optionString;
    } else {
        if (jvm->GetEnv((void **)&env, JNI_VERSION_10) != JNI_OK) {
            if (jvm->AttachCurrentThread((void **)&env, nullptr) != JNI_OK) {
                std::cerr << "Failed to attach current thread to Java VM" << std::endl;
                jvm->DestroyJavaVM();
                exit(1);
            }
            shouldDetach = true;
        }
    }
}

NRSSL::~NRSSL() {
    if (shouldDetach) {
        jvm->DetachCurrentThread();
    }
}

void NRSSL::shutdown() {
    std::cout << "Shutting down NRSSL" << std::endl;
    if (jvm) {
        jvm->DestroyJavaVM();
    }
}

jclass NRSSL::initializeJClass(std::string class_name) {
    jclass clazz = env->FindClass(class_name.c_str());
    if (clazz == nullptr) {
        std::cerr << "Failed to find " << class_name << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        exit(1);
    }
    return clazz;
}

jmethodID NRSSL::getJMethod(jclass clazz, std::string method_name, std::string signature,
                            bool is_static) {
    jmethodID method = is_static
                           ? env->GetStaticMethodID(clazz, method_name.c_str(), signature.c_str())
                           : env->GetMethodID(clazz, method_name.c_str(), signature.c_str());
    if (method == nullptr) {
        std::cerr << "Failed to find \"" << method_name << "\" sig: " << signature << std::endl;
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        jvm->DestroyJavaVM();
        exit(1);
    }
    return method;
}

std::string NRSSL::createSignature(std::string returnType,
                                   std::initializer_list<std::string> args) {

    static const std::vector<std::string> primitives = {
        JNI_TYPES::VOID, JNI_TYPES::BOOLEAN, JNI_TYPES::INT,   JNI_TYPES::SHORT,
        JNI_TYPES::BYTE, JNI_TYPES::LONG,    JNI_TYPES::FLOAT, JNI_TYPES::DOUBLE};

    std::string signature = "(";
    for (auto &arg : args) {
        if (std::find(primitives.begin(), primitives.end(), arg) == primitives.end()) {
            signature += "L";
            signature += arg;
            signature += ";";
        } else
            signature += arg;
    }

    signature += ")";

    signature += std::find(primitives.begin(), primitives.end(), returnType) != primitives.end()
                     ? returnType
                     : "L" + returnType + ";";

    return signature;
}

std::tuple<std::unordered_map<int, int>, std::string, std::string>
NRSSL::getTypeProperties(NRSSL::Type type) {

    std::unordered_map<int, int> currentNrsSizeMap;
    std::string currentNrsClassPath;
    std::string currentNrsClassBPath;

    switch (type) {
    case POSIT:
        currentNrsSizeMap = sizeToExpSize;
        currentNrsClassPath = JNI_TYPES::POSIT;
        currentNrsClassBPath = JNI_TYPES::POSIT_B;
        break;
    case MORRIS:
        currentNrsSizeMap = sizeToGSizeMorris;
        currentNrsClassPath = JNI_TYPES::MORRIS;
        currentNrsClassBPath = JNI_TYPES::MORRIS_B;
        break;
    case MORRIS_HEB:
        currentNrsSizeMap = sizeToGSizeMorris;
        currentNrsClassPath = JNI_TYPES::MORRIS_HEB;
        currentNrsClassBPath = JNI_TYPES::MORRIS_HEB_B;
        break;
    case MORRIS_BIAS_HEB:
        currentNrsSizeMap = sizeToGSizeMorris;
        currentNrsClassPath = JNI_TYPES::MORRIS_BIAS_HEB;
        currentNrsClassBPath = JNI_TYPES::MORRIS_BIAS_HEB_B;
        break;
    case MORRIS_UNARY_HEB:
        currentNrsSizeMap = sizeToGSizeMorris;
        currentNrsClassPath = JNI_TYPES::MORRIS_UNARY_HEB;
        currentNrsClassBPath = JNI_TYPES::MORRIS_UNARY_HEB_B;
        break;
    }

    return std::make_tuple(currentNrsSizeMap, currentNrsClassPath, currentNrsClassBPath);
}
