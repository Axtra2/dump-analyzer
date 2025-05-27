#include <app/app.h>

#include <utils/fs_utils.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <unordered_set>

namespace {

void printDumpSummary(const DumpSummary& dumpSummary) {
    std::cout << std::format("Total number of records in dump: {}\n"
                             "Number of unique tags in dump:   {}\n\n",
                             dumpSummary.numRecords,
                             dumpSummary.tagCounts.size());

    size_t maxTagWidth = 3;
    size_t maxCount    = 1;
    for (const auto& [tag, count] : dumpSummary.tagCounts) {
        maxTagWidth = std::max(maxTagWidth, std::strlen(tagName(tag)));
        maxCount    = std::max(maxCount, count);
    }
    const size_t maxCountWidth = std::max(5ull, static_cast<size_t>(std::log10(maxCount)) + 1);
    std::cout << std::format("{:{}} | {:{}}\n", "tag", maxTagWidth + 7, "count", maxCountWidth + 1);
    std::cout << std::format("{:-<{}}+{:-<{}}\n", "", maxTagWidth + 8, "", maxCountWidth + 1);
    for (const auto& [tag, count] : dumpSummary.tagCounts) {
        std::cout << std::format(
            "{:{}} (0x{:02X}) | {:<{}}\n", tagName(tag), maxTagWidth, static_cast<uint8_t>(tag), count, maxCountWidth);
    }
    std::cout << '\n';

    size_t maxSubTagWidth = 7;
    size_t maxSubTagCount = 1;
    for (const auto& [subtag, count] : dumpSummary.subTagCounts) {
        maxSubTagWidth = std::max(maxSubTagWidth, std::strlen(subTagName(subtag)));
        maxSubTagCount = std::max(maxSubTagCount, count);
    }
    const size_t maxSubTagCountWidth = std::max(5ull, static_cast<size_t>(std::log10(maxSubTagCount)) + 1);
    std::cout << std::format("{:{}} | {:{}}\n", "sub-tag", maxSubTagWidth + 7, "count", maxSubTagCountWidth + 1);
    std::cout << std::format("{:-<{}}+{:-<{}}\n", "", maxSubTagWidth + 8, "", maxSubTagCountWidth + 1);
    for (const auto& [subtag, count] : dumpSummary.subTagCounts) {
        std::cout << std::format("{:{}} (0x{:02X}) | {:<{}}\n",
                                 subTagName(subtag),
                                 maxSubTagWidth,
                                 static_cast<uint8_t>(subtag),
                                 count,
                                 maxSubTagCountWidth);
    }
}

} // namespace

void App::run(const Args& args) {

    const auto bytes = readWholeFile(args.dumpFile);

    const std::string magic = "JAVA PROFILE 1.0.2";

    if (std::strncmp(magic.c_str(), reinterpret_cast<const char*>(bytes.data()), bytes.size()) != 0) {
        throw std::runtime_error("wrong dump format");
    }

    R r(bytes.data(), bytes.size());
    r.skip(magic.size() + 1);

    const auto dumpHeader = parseDumpHeader(r);
    identifierSize        = dumpHeader.identifierSize;

    if (identifierSize > sizeof(ID)) {
        throw std::runtime_error(std::format("unsupported identifier size {}", identifierSize));
    }
    const R dumpBodyReader = r;

    dumpSummary = summarizeDump(dumpBodyReader, identifierSize);

    // order of records is not guaranteed, so parse in separate passes
    strings             = parseStrings(dumpBodyReader, identifierSize);
    classDumps          = parseClassDumps(dumpBodyReader, identifierSize);
    classInstanceCount  = countInstances(dumpBodyReader, identifierSize);
    loadClasses         = parseLoadClasses(dumpBodyReader, dumpHeader);
    instances           = parseInstanceDumps(dumpBodyReader, identifierSize);
    objectArrayDumps    = parseObjectArrayDumps(dumpBodyReader, identifierSize);
    primitiveArrayDumps = parsePrimitiveArrayDumps(dumpBodyReader, identifierSize);
    stackFrames         = parseStackFrames(dumpBodyReader, identifierSize);
    stackTraces         = parseStackTraces(dumpBodyReader, identifierSize);

    std::cout << std::format("\n"
                             "Heap Dump Summary:\n\n"
                             "Size of identifiers: {}\n"
                             "Milliseconds since 0:00 GMT, 1/1/70: {}\n\n",
                             identifierSize,
                             dumpHeader.millis);

    printDumpSummary(dumpSummary);

#if 0
    for (const auto& [k, v] : stackTraces) {
        std::cout << std::format("\nstack trace {}:\n", static_cast<uint32_t>(v.stackTraceSerialNumber));
        for (const auto frameID : v.stackFrames) {
            printStackFrame(frameID, 2);
        }
    }
#endif

    const auto coroutineInstances = getCoroutineInstances();

    std::cout << "\nCoroutines summary:\n\n";

    printCoroutinesList(coroutineInstances);

#if 0
    for (const auto id : getCoroutineClasses()) {
        std::cout << '\n';
        printClass(id);
    }
#endif

    std::cout.flush();
}

void App::printInstance(ObjectID objectID, bool recurse, size_t indent) {
    std::unordered_set<ObjectID> visited;
    const auto                   printInstanceImpl =
        [&visited, recurse, this](ObjectID objectID_, size_t indent_, std::string_view name_, const auto& f_) {
            std::string indentStr(indent_, ' ');
            if (isNull(objectID_)) {
                std::cout << indentStr << "null object " << name_ << '\n';
                return;
            }
            const auto& instance  = instances.at(objectID_);
            const auto& loadClass = loadClasses.at(instance.classObjectID);
            const auto  className = getView(loadClass.nameStringID);
            std::cout << indentStr
                      << std::format("{} {} = {} (ST={})\n",
                                     className,
                                     name_,
                                     formatID(objectID_),
                                     instance.stackTraceSerialNumber);

            if (visited.contains(objectID_)) {
                return;
            }
            visited.insert(objectID_);

            if (!recurse) {
                return;
            }

            forEachField(objectID_, [&](ClassDump::Field f, Value v) {
                const auto fieldName = getView(f.nameStringID);
                if (f.type == BasicType::OBJECT) {
                    const ID id = static_cast<ID>(v);
                    if (isObjectID(id)) {
                        f_(static_cast<ObjectID>(id), indent_ + 2, fieldName, f_);
                        return;
                    }

                    std::cout << indentStr << "  ";
                    if (isNull(id)) {
                        std::cout << "null";
                    } else if (isClassObjectID(id)) {
                        std::cout << "class";
                    } else if (isObjectArrayID(id)) {
                        std::cout << "object array";
                    } else if (isPrimitiveArrayID(id)) {
                        std::cout << "primitive array";
                    } else {
                        throw std::runtime_error("unknown object");
                    }
                    std::cout << ' ';
                } else {
                    std::cout << indentStr << "  ";
                }
                std::cout << std::format("{} {} = {}\n", basicTypeName(f.type), fieldName, formatValue(v, f.type));
            });
        };
    printInstanceImpl(objectID, indent, "", printInstanceImpl);
}

void App::printStackFrame(StackFrameID frameID, size_t indent) {
    const auto& frame = stackFrames.at(frameID);

    std::cout << std::string(indent, ' ');

    const auto methodName      = getView(frame.methodNameStringID);
    const auto methodSignature = getView(frame.methodSignatureStringID);

    std::cout << methodName << methodSignature;

    if (!isNull(frame.sourceFileNameStringID)) {
        // if (static_cast<ID>(frame.sourceFileNameStringID) != 0) {
        const auto sourceFileName = getView(frame.sourceFileNameStringID);
        std::cout << " (" << sourceFileName;
        if (frame.lineNumber > 0) {
            std::cout << ':' << frame.lineNumber;
        }
        std::cout << ')';
    } else {
        std::cout << "no source information";
    }
    std::cout << '\n';
}

// TODO
size_t App::calcRetainedHeapSize(isID auto root_) {
    const ID root = static_cast<ID>(root_);

    if (isNull(root)) {
        return 0;
    }

    size_t                 retainedHeapSize = basicTypeSize(BasicType::OBJECT);
    std::unordered_set<ID> visited;
    std::stack<ID>         toVisit;

    toVisit.push(root);
    visited.insert(root);
    while (!toVisit.empty()) {
        const auto id = toVisit.top();
        toVisit.pop();

        if (isObjectID(id)) {
            const auto objectID = static_cast<ObjectID>(id);
            retainedHeapSize += classDumps.at(instances.at(objectID).classObjectID).instanceSizeBytes;
            forEachField(objectID, [&](ClassDump::Field f, Value v) {
                if (f.type == BasicType::OBJECT) {
                    const ID fieldID = static_cast<ID>(v);
                    if (!isNull(fieldID) && !visited.contains(fieldID)) {
                        visited.insert(fieldID);
                        toVisit.push(fieldID);
                    }
                }
            });
            continue;
        }

        if (isObjectArrayID(id)) {
            const auto  objectArrayID = static_cast<ArrayObjectID>(id);
            const auto& array         = objectArrayDumps.at(objectArrayID);
            retainedHeapSize += identifierSize * array.numberOfElements;
            R r(array.elementsView.data(), array.elementsView.size_bytes());
            for (size_t i = 0; i < array.numberOfElements; ++i) {
                const ID elementID = r.read<ID>(identifierSize);
                if (!isNull(elementID) && !visited.contains(elementID)) {
                    visited.insert(elementID);
                    toVisit.push(elementID);
                }
            }
            continue;
        }

        if (isPrimitiveArrayID(id)) {
            const auto  primitiveArrayID = static_cast<ArrayObjectID>(id);
            const auto& array            = primitiveArrayDumps.at(primitiveArrayID);
            retainedHeapSize += basicTypeSize(array.elementType) * array.numberOfElements;
            continue;
        }

        if (isClassObjectID(id)) {
            // TODO
            retainedHeapSize += 0;
            continue;
        }

        throw std::runtime_error(std::format("could not resolve object ID {}", id));
    }

    return retainedHeapSize;
}

void App::forEachSuperclass(ClassObjectID classObjectID, std::function<void(ClassObjectID)> f) {
    while (!isNull(classObjectID)) {
        f(classObjectID);
        classObjectID = classDumps.at(classObjectID).superclassObjectID;
    }
}

void App::forEachField(ClassObjectID classObjectID, std::function<void(ClassDump::Field)> f) {
    forEachSuperclass(classObjectID, [&](ClassObjectID id) {
        for (const auto& field : classDumps.at(id).fields) {
            f(field);
        }
    });
}

void App::forEachField(ObjectID objectID, std::function<void(ClassDump::Field, Value)> f) {
    const auto& instance = instances.at(objectID);
    R           fieldsReader(instance.fieldsView.data(), instance.fieldsView.size_bytes());
    forEachSuperclass(instance.classObjectID, [&](ClassObjectID id) {
        for (const auto& field : classDumps.at(id).fields) {
            f(field, fieldsReader.read<Value>(basicTypeSize(field.type)));
        }
    });
}

bool App::isClassObjectID(ID id) {
    return classDumps.contains(static_cast<ClassObjectID>(id));
}

bool App::isObjectID(ID id) {
    return instances.contains(static_cast<ObjectID>(id));
}

bool App::isObjectArrayID(ID id) {
    return objectArrayDumps.contains(static_cast<ArrayObjectID>(id));
}

bool App::isPrimitiveArrayID(ID id) {
    return primitiveArrayDumps.contains(static_cast<ArrayObjectID>(id));
}

std::vector<ObjectID> App::getClassInstances(ClassObjectID classObjectID) {
    if (isNull(classObjectID)) {
        return {};
    }
    std::vector<ObjectID> classInstances;
    for (const auto& [id, i] : instances) {
        if (i.classObjectID == classObjectID) {
            classInstances.push_back(id);
        }
    }
    return classInstances;
}

std::unordered_set<ClassObjectID> App::getCoroutineClasses(bool internal) {
    ClassObjectID abstractCoroutineClassObjectID{0};
    for (const auto& [id, lc] : loadClasses) {
        if (getView(lc.nameStringID) == "kotlinx/coroutines/AbstractCoroutine") {
            abstractCoroutineClassObjectID = id;
            break;
        }
    }
    if (isNull(abstractCoroutineClassObjectID)) {
        return {};
    }
    std::unordered_set<ClassObjectID> coroutineClasses;
    for (const auto& [id, cd] : classDumps) {
        if (cd.superclassObjectID == abstractCoroutineClassObjectID) {
            const auto name = getView(loadClasses.at(id).nameStringID);
            if (internal || name.find("internal") == name.npos) {
                coroutineClasses.insert(id);
            }
        }
    }
    return coroutineClasses;
}

std::vector<ObjectID> App::getCoroutineInstances() {
    const auto&           coroutineClasses = getCoroutineClasses();
    std::vector<ObjectID> coroutineInstances;
    for (const auto& [id, i] : instances) {
        if (coroutineClasses.contains(i.classObjectID)) {
            coroutineInstances.push_back(id);
        }
    }
    return coroutineInstances;
}

void App::printCoroutinesList(const std::vector<ObjectID>& coroutinesList) {
    for (const auto& id : coroutinesList) {
        const auto& instance         = instances.at(id);
        const auto& loadClass        = loadClasses.at(instance.classObjectID);
        const auto  className        = getView(loadClass.nameStringID).substr(19);
        const auto  retainedHeapSize = calcRetainedHeapSize(id);
        std::cout << std::format(
            "{}@{}, state: {}, retained heap: {}\n", className, formatID(id), getCoroutineState(id), retainedHeapSize);
    }
}

Value App::getFieldValue(ObjectID id, std::string_view fieldName) {
    Value value;
    bool  found = false;
    forEachField(id, [&](ClassDump::Field f, Value v) {
        if (!found && getView(f.nameStringID) == fieldName) {
            found = true;
            value = v;
        }
    });
    if (!found) {
        throw std::runtime_error(std::format("could not find field {}", fieldName));
    }
    return value;
}

std::string App::getCoroutineState(ObjectID id) {
    const auto  stateObjectID  = static_cast<ObjectID>(getFieldValue(id, "_state$volatile"));
    const auto& stateInstance  = instances.at(stateObjectID);
    const auto& stateClass     = loadClasses.at(stateInstance.classObjectID);
    const auto  stateClassName = getView(stateClass.nameStringID);

    // TODO: handle more states
    if (stateClassName == "kotlinx/coroutines/JobSupport$Finishing") {
        const bool isCompleting = static_cast<int32_t>(getFieldValue(stateObjectID, "_isCompleting$volatile"));
        if (isCompleting) {
            return "COMPLETING";
        } else {
            return "CANCELLING";
        }
    } else if (stateClassName == "kotlinx/coroutines/NodeList") {
        return "ACTIVE";
    } else {
        return "UNKNOWN";
    }
}

std::string_view App::getView(StringID stringID) {
    return strings.at(stringID).view;
}

void App::printClass(ClassObjectID classObjectID) {
    if (isNull(classObjectID)) {
        std::cout << "null\n";
        return;
    }

    const auto& c    = loadClasses.at(classObjectID);
    const auto& name = getView(c.nameStringID);

    if (!classDumps.contains(c.classObjectID)) {
        return;
    }

    const auto instances = getClassInstances(classObjectID);
    std::cout << std::format("{} (id={}, serial={}, {} instance(s)):\n",
                             name,
                             formatID(classObjectID),
                             c.classSerialNumber,
                             instances.size());

    const auto& dump = classDumps.at(c.classObjectID);

    size_t maxTypeWidth = 0;
    for (const auto& f : dump.constants) {
        maxTypeWidth = std::max(maxTypeWidth, 6 + std::strlen(basicTypeName(f.type)));
    }
    for (const auto& f : dump.statics) {
        maxTypeWidth = std::max(maxTypeWidth, 7 + std::strlen(basicTypeName(f.type)));
    }
    forEachField(classObjectID,
                 [&](const auto& f) { maxTypeWidth = std::max(maxTypeWidth, std::strlen(basicTypeName(f.type))); });

    for (const auto& f : dump.constants) {
        std::cout << std::format(
            "    const {:{}} = {}\n", basicTypeName(f.type), maxTypeWidth - 6, formatValue(f.value, f.type));
    }
    for (const auto& f : dump.statics) {
        std::cout << std::format("    static {:{}} {} = {}\n",
                                 basicTypeName(f.type),
                                 maxTypeWidth - 7,
                                 getView(f.nameStringID),
                                 formatValue(f.value, f.type));
    }

    forEachField(classObjectID, [&](const auto& f) {
        std::cout << std::format("    {:{}} {}\n", basicTypeName(f.type), maxTypeWidth, getView(f.nameStringID));
    });

    std::cout << "  superclasses:\n";
    forEachSuperclass(classObjectID, [&](ClassObjectID superclassObjectID) {
        if (superclassObjectID != classObjectID) {
            const auto& superclass     = loadClasses.at(superclassObjectID);
            const auto  superclassName = getView(superclass.nameStringID);
            std::cout << "    " << superclassName << '\n';
        }
    });

    if (instances.empty()) {
        return;
    }

    std::cout << "  instance(s):\n";
    for (const auto& objectID : instances) {
        printInstance(objectID, false, 4);
    }
}
