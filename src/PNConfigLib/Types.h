/*****************************************************************************/
/*  PNConfigLib - PROFINET Configuration Library                            */
/*  Type definitions for pure C++ implementation                             */
/*****************************************************************************/

#ifndef PNCONFIGLIB_TYPES_H
#define PNCONFIGLIB_TYPES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <cstdint>
#include <memory>
#include <optional>

namespace PNConfigLib {

// String types
using String = std::string;

// Container types
template<typename T>
using List = std::vector<T>;

template<typename K, typename V>
using Map = std::unordered_map<K, V>;

// Byte array type
using ByteArray = std::vector<uint8_t>;

// Variant type for flexible values
using Variant = std::variant<
    std::monostate,
    bool,
    int32_t,
    uint32_t,
    int64_t,
    uint64_t,
    double,
    std::string,
    std::vector<uint8_t>
>;

// Helper to check if variant holds a value
template<typename T>
bool holds(const Variant& v) {
    return std::holds_alternative<T>(v);
}

// Helper to get value from variant
template<typename T>
T get(const Variant& v) {
    return std::get<T>(v);
}

// Helper to get value with default
template<typename T>
T getOr(const Variant& v, T defaultValue) {
    if (std::holds_alternative<T>(v)) {
        return std::get<T>(v);
    }
    return defaultValue;
}

} // namespace PNConfigLib

#endif // PNCONFIGLIB_TYPES_H
