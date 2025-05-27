#pragma once

#include <concepts>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

using ID    = uint64_t;
using Value = uint64_t;

template <typename T>
concept isID = std::is_same_v<T, ID> || std::is_enum_v<T> && std::is_same_v<std::underlying_type_t<T>, ID>;

enum class Tag : uint8_t {
    STRING_IN_UTF8    = 0x01,
    LOAD_CLASS        = 0x02,
    UNLOAD_CLASS      = 0x03,
    STACK_FRAME       = 0x04,
    STACK_TRACE       = 0x05,
    ALLOC_SITES       = 0x06,
    HEAP_SUMMARY      = 0x07,
    START_THREAD      = 0x0A,
    END_THREAD        = 0x0B,
    HEAP_DUMP         = 0x0C,
    HEAP_DUMP_SEGMENT = 0x1C,
    HEAP_DUMP_END     = 0x2C,
    CPU_SAMPLES       = 0x0D,
    CONTROL_SETTINGS  = 0x0E,
};

Tag         validateTag(uint8_t maybeTag);
const char* tagName(Tag tag);

enum class SubTag : uint8_t {
    ROOT_UNKNOWN         = 0xFF,
    ROOT_JNI_GLOBAL      = 0x01,
    ROOT_JNI_LOCAL       = 0x02,
    ROOT_JAVA_FRAME      = 0x03,
    ROOT_NATIVE_STACK    = 0x04,
    ROOT_STICKY_CLASS    = 0x05,
    ROOT_THREAD_BLOCK    = 0x06,
    ROOT_MONITOR_USED    = 0x07,
    ROOT_THREAD_OBJECT   = 0x08,
    CLASS_DUMP           = 0x20,
    INSTANCE_DUMP        = 0x21,
    OBJECT_ARRAY_DUMP    = 0x22,
    PRIMITIVE_ARRAY_DUMP = 0x23,
};

static constexpr size_t DYNAMIC = std::numeric_limits<size_t>::max();

SubTag      validateSubTag(uint8_t maybeSubTag);
const char* subTagName(SubTag subTag);
size_t      subTagSize(SubTag subTag, size_t identifierSize);

enum class BasicType : uint8_t {
    OBJECT  = 0x02,
    BOOLEAN = 0x04,
    CHAR    = 0x05,
    FLOAT   = 0x06,
    DOUBLE  = 0x07,
    BYTE    = 0x08,
    SHORT   = 0x09,
    INT     = 0x0A,
    LONG    = 0x0B,
};

BasicType   validateBasicType(uint8_t maybeBasicType);
const char* basicTypeName(BasicType basicType);
size_t      basicTypeSize(BasicType basicType);

struct DumpHeader {
    uint32_t identifierSize;
    uint64_t millis;
};

struct RecordHeader {
    Tag      tag;
    uint32_t micros;
    uint32_t bodyByteSize;
};

enum class StringID : ID {};

struct StringInUTF8 {
    StringID         id;
    std::string_view view;
};

enum class ClassObjectID : ID {};

struct LoadClass {
    uint32_t      classSerialNumber;
    ClassObjectID classObjectID;
    uint32_t      stackTraceSerialNumber;
    StringID      nameStringID;
};

struct ClassDump {

    struct Constant {
        uint16_t  constantPoolIndex;
        BasicType type;
        Value     value;
    };
    struct Static {
        StringID  nameStringID;
        BasicType type;
        Value     value;
    };
    struct Field {
        StringID  nameStringID;
        BasicType type;
    };

    ClassObjectID         classObjectID;
    uint32_t              stackTrackeSerialNumber;
    ClassObjectID         superclassObjectID;
    ID                    classLoaderObjectID;
    ID                    signersObjectID;
    ID                    protectionDomainObjectID;
    ID                    reserved[2];
    uint32_t              instanceSizeBytes;
    std::vector<Constant> constants;
    std::vector<Static>   statics;
    std::vector<Field>    fields;
};

enum class ObjectID : ID {};

struct InstanceDump {
    ObjectID                   objectID;
    uint32_t                   stackTraceSerialNumber;
    ClassObjectID              classObjectID;
    std::span<const std::byte> fieldsView;
};

enum class StackFrameID : ID {};

struct StackFrame {
    StackFrameID stackFrameID;
    StringID     methodNameStringID;
    StringID     methodSignatureStringID;
    StringID     sourceFileNameStringID;
    uint32_t     classSerialNumber;
    int32_t      lineNumber;
};

enum class StackTraceSerialNumber : uint32_t {};

struct StackTrace {
    StackTraceSerialNumber    stackTraceSerialNumber;
    uint32_t                  threadSerialNumber;
    uint32_t                  numberOfFrames;
    std::vector<StackFrameID> stackFrames;
};

enum class ArrayObjectID : ID {};
enum class ArrayClassObjectID : ID {};

struct ObjectArrayDump {
    ArrayObjectID              arrayObjectID;
    StackTraceSerialNumber     stackTraceSerialNumber;
    uint32_t                   numberOfElements;
    ArrayClassObjectID         arrayClassObjectID;
    std::span<const std::byte> elementsView;
};

struct PrimitiveArrayDump {
    ArrayObjectID              arrayObjectID;
    StackTraceSerialNumber     stackTraceSerialNumber;
    uint32_t                   numberOfElements;
    BasicType                  elementType;
    std::span<const std::byte> elementsView;
};

inline bool isNull(isID auto id) {
    return static_cast<ID>(id) == 0;
}

inline std::string formatID(isID auto id) {
    return std::format("{:02x}", static_cast<ID>(id));
}

inline std::string formatValue(Value value, BasicType basicType) {
    // TODO: format according to type
    (void)basicType;
    return std::format("{0} (0x{0:0X})", value);
}
