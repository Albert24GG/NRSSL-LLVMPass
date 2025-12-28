#ifndef NRSSL_TPP
#define NRSSL_TPP

#include "NRSSL.h"
#include "jni.h"
#include <iostream>
#include <limits>
#include <type_traits>

template <const char *inputValueType, typename T>
jobject NRSSL::callApplyMethod(jclass nrsClass, Type floatType, T value, int exponentSize, int size,
                        jobject roundingType) {
    const auto &[_, _, currentNrsClassBPath] = getTypeProperties(floatType);

    auto applyMethodSig = [&] {
        switch (floatType) {
        case MORRIS_UNARY_HEB: {
            const static auto sig =
                createSignature(currentNrsClassBPath,
                                {inputValueType, JNI_TYPES::INT, JNI_TYPES::ROUNDING_TYPE});
            return sig;
        }
        default: {
            const static auto sig =
                createSignature(currentNrsClassBPath, {inputValueType, JNI_TYPES::INT,
                                                       JNI_TYPES::INT, JNI_TYPES::ROUNDING_TYPE});
            return sig;
        }
        }
    }();

    jmethodID applyMethod = getJMethod(nrsClass, JNI_METHODS::APPLY, applyMethodSig, true);

    switch (floatType) {
    case MORRIS_UNARY_HEB:
        return env->CallObjectMethod(nrsClass, applyMethod, value, size, roundingType);
    default:
        return env->CallObjectMethod(nrsClass, applyMethod, value, exponentSize, size,
                                     roundingType);
    }
}

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


    jobject roundingType = env->CallStaticObjectMethod(nrsClass, defaultRoundingMethod);
    int size = sizeof(T) * 8;
    int exponentSize = currentNrsSizeMap.at(size);

    jobject nrsB = callApplyMethod<JNI_TYPES::DOUBLE>(nrsClass, type, value, exponentSize, size, roundingType);

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

    jmethodID toDoubleMethod =
        getJMethod(nrsClassB, JNI_METHODS::TO_DOUBLE, createSignature(JNI_TYPES::DOUBLE, {}));

    jobject roundingType = env->CallStaticObjectMethod(nrsClass, defaultRoundingMethod);
    int size = sizeof(T) * 8;
    int exponentSize = currentNrsSizeMap.at(size);

    auto binaryString = UintToBinaryString(value);

    jobject convertedValue = callApplyMethod<JNI_TYPES::STRING>(nrsClass, type,
                                                    env->NewStringUTF(binaryString.c_str()),
                                                    exponentSize, size, roundingType);

    jdouble doubleValue = env->CallDoubleMethod(convertedValue, toDoubleMethod);

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
