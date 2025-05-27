#include <data/data.h>

#include <stdexcept>

Tag validateTag(uint8_t maybeTag) {
    Tag result = static_cast<Tag>(maybeTag);
    switch (result) {
        using enum Tag;
    case STRING_IN_UTF8:
    case LOAD_CLASS:
    case UNLOAD_CLASS:
    case STACK_FRAME:
    case STACK_TRACE:
    case ALLOC_SITES:
    case HEAP_SUMMARY:
    case START_THREAD:
    case END_THREAD:
    case HEAP_DUMP:
    case HEAP_DUMP_SEGMENT:
    case HEAP_DUMP_END:
    case CPU_SAMPLES:
    case CONTROL_SETTINGS:  return result;
    }
    throw std::runtime_error(std::format("unknown tag 0x{:02X}", maybeTag));
}

const char* tagName(Tag tag) {
    switch (tag) {
        using enum Tag;
    case STRING_IN_UTF8:    return "STRING IN UTF8";
    case LOAD_CLASS:        return "LOAD CLASS";
    case UNLOAD_CLASS:      return "UNLOAD CLASS";
    case STACK_FRAME:       return "STACK FRAME";
    case STACK_TRACE:       return "STACK TRACE";
    case ALLOC_SITES:       return "ALLOC SITES";
    case HEAP_SUMMARY:      return "HEAP SUMMARY";
    case START_THREAD:      return "START THREAD";
    case END_THREAD:        return "END THREAD";
    case HEAP_DUMP:         return "HEAP DUMP";
    case HEAP_DUMP_SEGMENT: return "HEAP DUMP SEGMENT";
    case HEAP_DUMP_END:     return "HEAP DUMP END";
    case CPU_SAMPLES:       return "CPU SAMPLES";
    case CONTROL_SETTINGS:  return "CONTROL SETTINGS";
    }
    throw std::runtime_error("unreachable code");
}

SubTag validateSubTag(uint8_t maybeSubTag) {
    SubTag result = static_cast<SubTag>(maybeSubTag);
    switch (result) {
        using enum SubTag;
    case ROOT_UNKNOWN:
    case ROOT_JNI_GLOBAL:
    case ROOT_JNI_LOCAL:
    case ROOT_JAVA_FRAME:
    case ROOT_NATIVE_STACK:
    case ROOT_STICKY_CLASS:
    case ROOT_THREAD_BLOCK:
    case ROOT_MONITOR_USED:
    case ROOT_THREAD_OBJECT:
    case CLASS_DUMP:
    case INSTANCE_DUMP:
    case OBJECT_ARRAY_DUMP:
    case PRIMITIVE_ARRAY_DUMP: return result;
    }
    throw std::runtime_error(std::format("unknown sub-tag 0x{:02X}", maybeSubTag));
}

const char* subTagName(SubTag subTag) {
    switch (subTag) {
        using enum SubTag;
    case ROOT_UNKNOWN:         return "ROOT UNKNOWN";
    case ROOT_JNI_GLOBAL:      return "ROOT JNI GLOBAL";
    case ROOT_JNI_LOCAL:       return "ROOT JNI LOCAL";
    case ROOT_JAVA_FRAME:      return "ROOT JAVA FRAME";
    case ROOT_NATIVE_STACK:    return "ROOT NATIVE STACK";
    case ROOT_STICKY_CLASS:    return "ROOT STICKY CLASS";
    case ROOT_THREAD_BLOCK:    return "ROOT THREAD BLOCK";
    case ROOT_MONITOR_USED:    return "ROOT MONITOR USED";
    case ROOT_THREAD_OBJECT:   return "ROOT THREAD OBJECT";
    case CLASS_DUMP:           return "CLASS DUMP";
    case INSTANCE_DUMP:        return "INSTANCE DUMP";
    case OBJECT_ARRAY_DUMP:    return "OBJECT ARRAY DUMP";
    case PRIMITIVE_ARRAY_DUMP: return "PRIMITIVE ARRAY DUMP";
    }
    throw std::runtime_error("unreachable code");
}

size_t subTagSize(SubTag subTag, size_t identifierSize) {
    switch (subTag) {
        using enum SubTag;
    case ROOT_UNKNOWN:         return identifierSize;
    case ROOT_JNI_GLOBAL:      return identifierSize * 2;
    case ROOT_JNI_LOCAL:       return identifierSize + 8;
    case ROOT_JAVA_FRAME:      return identifierSize + 8;
    case ROOT_NATIVE_STACK:    return identifierSize + 4;
    case ROOT_STICKY_CLASS:    return identifierSize;
    case ROOT_THREAD_BLOCK:    return identifierSize + 4;
    case ROOT_MONITOR_USED:    return identifierSize;
    case ROOT_THREAD_OBJECT:   return identifierSize + 8;
    case CLASS_DUMP:
    case INSTANCE_DUMP:
    case OBJECT_ARRAY_DUMP:
    case PRIMITIVE_ARRAY_DUMP: return DYNAMIC;
    }
    throw std::runtime_error("unreachable code");
}

BasicType validateBasicType(uint8_t maybeBasicType) {
    BasicType result = static_cast<BasicType>(maybeBasicType);
    switch (result) {
        using enum BasicType;
    case OBJECT:
    case BOOLEAN:
    case CHAR:
    case FLOAT:
    case DOUBLE:
    case BYTE:
    case SHORT:
    case INT:
    case LONG:    return result;
    }
    throw std::runtime_error(std::format("unknown basic type 0x{:02X}", maybeBasicType));
}

const char* basicTypeName(BasicType basicType) {
    switch (basicType) {
        using enum BasicType;
    case OBJECT:  return "object";
    case BOOLEAN: return "boolean";
    case CHAR:    return "char";
    case FLOAT:   return "float";
    case DOUBLE:  return "double";
    case BYTE:    return "byte";
    case SHORT:   return "short";
    case INT:     return "int";
    case LONG:    return "long";
    }
    throw std::runtime_error("unreachable code");
}

size_t basicTypeSize(BasicType basicType) {
    switch (basicType) {
        using enum BasicType;
    case OBJECT:  return 8;
    case BOOLEAN: return 1; // assume 1
    case CHAR:    return 2;
    case FLOAT:   return 4;
    case DOUBLE:  return 8;
    case BYTE:    return 1;
    case SHORT:   return 2;
    case INT:     return 4;
    case LONG:    return 8;
    }
    throw std::runtime_error("unreachable code");
}
