#ifndef NRSSL_H
#define NRSSL_H

#include <jni.h>
#include <string>
#include <unordered_map>

namespace JNI_TYPES {
inline constexpr char VOID[] = "V";
inline constexpr char BOOLEAN[] = "Z";
inline constexpr char INT[] = "I";
inline constexpr char SHORT[] = "S";
inline constexpr char BYTE[] = "B";
inline constexpr char LONG[] = "J";
inline constexpr char FLOAT[] = "F";
inline constexpr char DOUBLE[] = "D";

inline constexpr char STRING[] = "java/lang/String";
inline constexpr char OBJECT[] = "java/lang/Object";

inline constexpr char POSIT[] = "ro/upb/nrs/sl/Posit";
inline constexpr char POSIT_B[] = "ro/upb/nrs/sl/Posit_B";
inline constexpr char MORRIS[] = "ro/upb/nrs/sl/Morris";
inline constexpr char MORRIS_B[] = "ro/upb/nrs/sl/Morris_B";
inline constexpr char MORRIS_HEB[] = "ro/upb/nrs/sl/MorrisHEB";
inline constexpr char MORRIS_HEB_B[] = "ro/upb/nrs/sl/MorrisHEB_B";
inline constexpr char MORRIS_BIAS_HEB[] = "ro/upb/nrs/sl/MorrisBiasHEB";
inline constexpr char MORRIS_BIAS_HEB_B[] = "ro/upb/nrs/sl/MorrisBiasHEB_B";
inline constexpr char MORRIS_UNARY_HEB[] = "ro/upb/nrs/sl/MorrisUnaryHEB";
inline constexpr char MORRIS_UNARY_HEB_B[] = "ro/upb/nrs/sl/MorrisUnaryHEB_B";
inline constexpr char ROUNDING_TYPE[] = "ro/upb/nrs/sl/RoundingType";
} // namespace JNI_TYPES

namespace JNI_METHODS {
inline constexpr char APPLY[] = "apply";
inline constexpr char TO_BINARY_STRING[] = "toBinaryString";
inline constexpr char TO_DOUBLE[] = "toDouble";

inline constexpr char DEFAULTROUNDING[] = "default_rounding";
inline constexpr char DEFAULTSIZE[] = "default_size";
inline constexpr char DEFAULTEXPSIZE[] = "default_exponent_size";
} // namespace JNI_METHODS

class NRSSL {

    static JavaVM *jvm;
    JNIEnv *env = nullptr;

    bool shouldDetach = false;

    const std::unordered_map<int, int> sizeToExpSize = {{8, 2}, {16, 2}, {32, 2}, {64, 2}};
    const std::unordered_map<int, int> sizeToGSizeMorris = {{8, 2}, {16, 3}, {32, 4}, {64, 6}};

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

    std::string createSignature(std::string_view returnType,
                                std::initializer_list<std::string_view> args);

    template <const char *inputValueType, typename T>
    jobject callApplyMethod(jclass nrsClass, Type type, T value, int exponentSize, int size,
                            jobject roundingType);

    std::tuple<std::unordered_map<int, int>, std::string, std::string>
    getTypeProperties(NRSSL::Type type);
};

#include "NRSSL.tpp"

#endif // NRSSL_H
