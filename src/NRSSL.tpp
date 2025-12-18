#ifndef NRSSL_TPP
#define NRSSL_TPP

#include "NRSSL.h"
#include "jni.h"
#include <iostream>
#include <limits>
#include <type_traits>

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type
NRSSL::convertDoubleToUint(double value, Type type) {

    auto [currentNrsSizeMap, currentNrsClassPath, currentNrsClassBPath] = getTypeProperties(type);

    if (currentNrsSizeMap.find(sizeof(T) * 8) == currentNrsSizeMap.end()) {
        std::cerr << "Unsupported size: " << sizeof(T) << std::endl;
        exit(1);
    }

    jclass nrsClass = initializeJClass(currentNrsClassPath);
    jclass nrsClassB = initializeJClass(currentNrsClassBPath);

    jmethodID defaultRoundingMethod =
        getJMethod(nrsClass, JNI_METHODS::DEFAULTROUNDING,
                   createSignature(JNI_TYPES::ROUNDING_TYPE, {}), true);

    jmethodID applyMethod =
        getJMethod(nrsClass, JNI_METHODS::APPLY,
                   createSignature(currentNrsClassBPath, {JNI_TYPES::DOUBLE, JNI_TYPES::INT,
                                                        JNI_TYPES::INT, JNI_TYPES::ROUNDING_TYPE}),
                   true);

    jobject roundingType = env->CallStaticObjectMethod(nrsClass, defaultRoundingMethod);
    int size = sizeof(T) * 8;
    int exponentSize = currentNrsSizeMap.at(size);

    jobject nrsB =
        env->CallObjectMethod(nrsClass, applyMethod, value, exponentSize, size, roundingType);

    jmethodID getBitsMethod = getJMethod(nrsClassB, JNI_METHODS::TO_BINARY_STRING,
                                         createSignature(JNI_TYPES::STRING, {}), false);

    jstring bits = (jstring)env->CallObjectMethod(nrsB, getBitsMethod);

    const char *bitsStr = env->GetStringUTFChars(bits, nullptr);

    std::cout << "Bits: " << bitsStr << std::endl;

    return binaryStringToUint<T>(bitsStr);
}

template <typename T, typename std::enable_if<std::is_unsigned<T>::value, bool>::type>
double NRSSL::convertUintToDouble(T value, Type type) {

    auto [currentNrsSizeMap, currentNrsClassPath, currentNrsClassBPath] = getTypeProperties(type);

    if (currentNrsSizeMap.find(sizeof(T) * 8) == currentNrsSizeMap.end()) {
        std::cerr << "Unsupported size: " << sizeof(T) << std::endl;
        exit(1);
    }

    jclass nrsClass = initializeJClass(currentNrsClassPath);
    jclass nrsClassB = initializeJClass(currentNrsClassBPath);

    jmethodID defaultRoundingMethod =
        getJMethod(nrsClass, JNI_METHODS::DEFAULTROUNDING,
                   createSignature(JNI_TYPES::ROUNDING_TYPE, {}), true);

    jmethodID applyMethod = getJMethod(
        nrsClass, JNI_METHODS::APPLY,
        createSignature(currentNrsClassBPath, {JNI_TYPES::STRING, JNI_TYPES::INT, JNI_TYPES::INT,
                                               JNI_TYPES::ROUNDING_TYPE}),
        true);

    jmethodID toDoubleMethod =
        getJMethod(nrsClassB, JNI_METHODS::TO_DOUBLE, createSignature(JNI_TYPES::DOUBLE, {}));

    jobject roundingType = env->CallStaticObjectMethod(nrsClass, defaultRoundingMethod);
    int size = sizeof(T) * 8;
    int exponentSize = currentNrsSizeMap.at(size);

    auto binaryString = UintToBinaryString(value);

    jobject posit_1 =
        env->CallStaticObjectMethod(nrsClass, applyMethod, env->NewStringUTF(binaryString.c_str()),
                                    exponentSize, size, roundingType);

    jdouble doubleValue = env->CallDoubleMethod(posit_1, toDoubleMethod);

    return doubleValue;
}

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type
NRSSL::binaryStringToUint(std::string binaryString) {
    if (binaryString.size() > sizeof(T) * 8) {
        std::cerr << "Binary string is longer than " << sizeof(T) * 8 << " bits" << std::endl;
        exit(1);
    }

    T value = 0;
    for (int i = 0; i < binaryString.size(); i++) {
        if (binaryString[i] != '0' && binaryString[i] != '1') {
            std::cerr << "Binary string contains non-binary characters" << std::endl;
            exit(1);
        }
        value = (value << 1) | (binaryString[i] - '0');
    }

    return value;
}

template <typename T, typename std::enable_if<std::is_unsigned<T>::value, bool>::type>
std::string NRSSL::UintToBinaryString(T value) {
    constexpr size_t bitSize = std::numeric_limits<T>::digits;
    std::string binaryString(bitSize, '0');

    for (size_t i = 0; i < bitSize; i++) {
        binaryString[bitSize - 1 - i] = ((value >> i) & 1) + '0';
    }

    return binaryString;
}

#endif // NRSSL_TPP
