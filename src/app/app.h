#pragma once

#include <app/args.h>
#include <data/data.h>
#include <parse/parse.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class App {
public:
    void run(const Args& args);

private:
    void printInstance(ObjectID objectID, bool recurse = false, size_t indent = 0, std::string_view name = "");

    void printStackFrame(StackFrameID frameID, size_t indent = 0);

    size_t calcRetainedHeapSize(isID auto root_);

    void forEachSuperclass(ClassObjectID classObjectID, std::function<void(ClassObjectID)> f);

    void forEachField(ClassObjectID classObjectID, std::function<void(ClassDump::Field)> f);

    void forEachField(ObjectID objectID, std::function<void(ClassDump::Field, Value)> f);

    bool isClassObjectID(ID id);

    bool isObjectID(ID id);

    bool isObjectArrayID(ID id);

    bool isPrimitiveArrayID(ID id);

    std::vector<ObjectID> getClassInstances(ClassObjectID classObjectID);

    std::unordered_set<ClassObjectID> getCoroutineClasses(bool internal = true);

    std::unordered_set<ObjectID> getCoroutineInstances();

    void printCoroutinesList(const std::unordered_set<ObjectID>& coroutineInstances);

    Value getFieldValue(ObjectID id, std::string_view fieldName);

    std::string getCoroutineState(ObjectID id);

    std::string_view getView(StringID stringID);

    void printClass(ClassObjectID classObjectID);

    std::optional<ObjectID> getCoroutineParent(ObjectID coroutine);

    std::string formatInstance(ObjectID id, std::string_view name = "");

    std::string formatCoroutine(ObjectID id);

    void printCoroutinesHierarchy(const std::unordered_set<ObjectID>& coroutines);

private:
    size_t                                                 identifierSize;
    DumpSummary                                            dumpSummary;
    std::unordered_map<StringID, StringInUTF8>             strings;
    std::unordered_map<ClassObjectID, LoadClass>           loadClasses;
    std::unordered_map<ClassObjectID, ClassDump>           classDumps;
    std::unordered_map<ClassObjectID, size_t>              classInstanceCount;
    std::unordered_map<ObjectID, InstanceDump>             instances;
    std::unordered_map<ArrayObjectID, ObjectArrayDump>     objectArrayDumps;
    std::unordered_map<ArrayObjectID, PrimitiveArrayDump>  primitiveArrayDumps;
    std::unordered_map<StackFrameID, StackFrame>           stackFrames;
    std::unordered_map<StackTraceSerialNumber, StackTrace> stackTraces;
};
