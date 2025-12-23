#ifndef NRSSL_H
#define NRSSL_H

#include <jni.h>
#include <string>
#include <unordered_map>

namespace JNI_TYPES {
std::string const VOID = "V";
std::string const BOOLEAN = "Z";
std::string const INT = "I";
std::string const SHORT = "S";
std::string const BYTE = "B";
std::string const LONG = "J";
std::string const FLOAT = "F";
std::string const DOUBLE = "D";

std::string const STRING = "java/lang/String";
std::string const OBJECT = "java/lang/Object";

std::string const POSIT = "ro/upb/nrs/sl/Posit";
std::string const POSIT_B = "ro/upb/nrs/sl/Posit_B";
std::string const MORRIS = "ro/upb/nrs/sl/Morris";
std::string const MORRIS_B = "ro/upb/nrs/sl/Morris_B";
std::string const MORRIS_HEB = "ro/upb/nrs/sl/MorrisHEB";
std::string const MORRIS_HEB_B = "ro/upb/nrs/sl/MorrisHEB_B";
std::string const MORRIS_BIAS_HEB = "ro/upb/nrs/sl/MorrisBiasHEB";
std::string const MORRIS_BIAS_HEB_B = "ro/upb/nrs/sl/MorrisBiasHEB_B";
std::string const MORRIS_UNARY_HEB = "ro/upb/nrs/sl/MorrisUnaryHEB";
std::string const MORRIS_UNARY_HEB_B = "ro/upb/nrs/sl/MorrisUnaryHEB_B";
std::string const ROUNDING_TYPE = "ro/upb/nrs/sl/RoundingType";
} // namespace JNI_TYPES

namespace JNI_METHODS {
std::string const APPLY = "apply";
std::string const TO_BINARY_STRING = "toBinaryString";
std::string const TO_DOUBLE = "toDouble";

std::string const DEFAULTROUNDING = "default_rounding";
std::string const DEFAULTSIZE = "default_size";
std::string const DEFAULTEXPSIZE = "default_exponent_size";
} // namespace JNI_METHODS

class NRSSL {

    static JavaVM *jvm;
    JNIEnv *env = nullptr;

    bool shouldDetach = false;

    const std::unordered_map<int, int> sizeToExpSize = {{8, 2}, {16, 2}, {32, 2}, {64, 2}};
    const std::unordered_map<int, int> sizeToGSizeMorris = {{8, 2}, {16, 3}, {32, 4}, {64, 6}};

    std::reference_wrapper<const std::string> currentNrsClassPath = JNI_TYPES::POSIT;
    std::reference_wrapper<const std::string> currentNrsClassBPath = JNI_TYPES::POSIT_B;
    std::reference_wrapper<const std::unordered_map<int, int>> currentNrsSizeMap = sizeToExpSize;

  public:
    NRSSL();
    ~NRSSL();

    enum Type {
        POSIT,
        MORRIS,
        MORRIS_HEB,
        MORRIS_UNARY_HEB,
        MORRIS_BIAS_HEB,
    };

    static void shutdown();

    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, T>::type convertDoubleToUint(double value,
                                                                                     Type type);

    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, T>::type
    binaryStringToUint(std::string binaryString);

    template <typename T, typename std::enable_if<std::is_unsigned<T>::value, bool>::type = true>
    double convertUintToDouble(T value, Type type);

    template <typename T, typename std::enable_if<std::is_unsigned<T>::value, bool>::type = true>
    std::string UintToBinaryString(T value);

    std::string convertBinaryStringToString(std::string binaryString, Type type);

  private:
    jclass initializeJClass(std::string class_name);
    jmethodID getJMethod(jclass clazz, std::string method_name, std::string signature,
                         bool is_static = false);

    std::string createSignature(std::string returnType, std::initializer_list<std::string> args);

    std::tuple<std::unordered_map<int, int>, std::string, std::string>
    getTypeProperties(NRSSL::Type type);
};

#include "NRSSL.tpp"

#endif // NRSSL_H
