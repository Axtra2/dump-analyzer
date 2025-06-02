#include <app/app.h>

#include <utils/forest.h>
#include <utils/fs_utils.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

namespace {

[[maybe_unused]]
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

#if 1
    dumpSummary = summarizeDump(dumpBodyReader, identifierSize);
    std::cout << std::format("\n"
                             "Heap Dump Summary:\n\n"
                             "Size of identifiers: {}\n"
                             "Milliseconds since 0:00 GMT, 1/1/70: {}\n\n",
                             identifierSize,
                             dumpHeader.millis);
    printDumpSummary(dumpSummary);
#endif

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

#if 0
    for (const auto& [k, v] : stackTraces) {
        std::cout << std::format("\nstack trace {}:\n", static_cast<uint32_t>(v.stackTraceSerialNumber));
        for (const auto frameID : v.stackFrames) {
            printStackFrame(frameID, 2);
        }
    }
#endif

#if 0
    const auto threads = parseRootThreads(dumpBodyReader, identifierSize);

    std::cout << "\nThreads:\n\n";

    for (const auto& [k, v] : threads) {
        std::string_view name = "no name";
        if (const auto nameV = getFieldValue(v.threadObjectID, "name"); isObjectID(nameV)) {
            if (const auto nameArr = getFieldValue(static_cast<ObjectID>(nameV), "value");
                isPrimitiveArrayID(nameArr)) {
                const auto nameBytes = primitiveArrayDumps.at(static_cast<ArrayObjectID>(nameArr)).elementsView;
                name = std::string_view(static_cast<const char*>((void*)nameBytes.data()), nameBytes.size());
            }
        }
        std::cout << std::format("\"{}\" (obj={}, serial={}, st={})\n",
                                 name,
                                 formatID(v.threadObjectID),
                                 static_cast<uint32_t>(v.threadSerialNumber),
                                 static_cast<uint32_t>(v.stackTraceSerialNumber));
    }
#endif

    const auto coroutineInstances = getCoroutineInstances();

#if 0
    std::cout << "\nCoroutines summary:\n\n";
    printCoroutinesList(coroutineInstances);
#endif

    std::cout << "\nHierarchy:\n\n";
    printCoroutinesHierarchy(coroutineInstances);

#if 0
    for (const auto id : getCoroutineClasses()) {
        std::cout << '\n';
        printClass(id);
    }
#endif

    std::cout.flush();
}

void App::printInstance(ObjectID objectID, bool recurse, size_t indent, std::string_view name) {
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
    printInstanceImpl(objectID, indent, name, printInstanceImpl);
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

std::unordered_set<ObjectID> App::getCoroutineInstances() {
    const auto&                  coroutineClasses = getCoroutineClasses();
    std::unordered_set<ObjectID> coroutineInstances;
    for (const auto& [id, i] : instances) {
        if (coroutineClasses.contains(i.classObjectID)) {
            coroutineInstances.insert(id);
        }
    }
    return coroutineInstances;
}

void App::printCoroutinesList(const std::unordered_set<ObjectID>& coroutinesList) {
    for (const auto& id : coroutinesList) {
        std::cout << formatCoroutine(id) << '\n';
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

    // clang-format off
    //    state class              public state
    //    ------------             ------------
    //    EmptyNew               : New         
    //    EmptyActive            : Active      
    //    JobNode                : Active      
    //    JobNode                : Active      
    //    InactiveNodeList       : New         
    //    NodeList               : Active      
    //    Finishing              : Completing  
    //    Finishing              : Cancelling  
    //    Cancelled              : Cancelled   
    //    <any>                  : Completed
    // clang-format on

    if (stateClassName == "kotlinx/coroutines/InactiveNodeList") {
        return "New";
    }

    if (stateClassName == "kotlinx/coroutines/NodeList") {
        return "ACTIVE";
    }

    if (stateClassName == "kotlinx/coroutines/Empty") {
        const bool isActive = static_cast<uint8_t>(getFieldValue(stateObjectID, "isActive"));
        if (isActive) {
            return "ACTIVE";
        } else {
            return "New";
        }
    }

    if (stateClassName == "kotlinx/coroutines/JobSupport$Finishing") {
        const bool isCompleting = static_cast<int32_t>(getFieldValue(stateObjectID, "_isCompleting$volatile"));
        if (isCompleting) {
            return "COMPLETING";
        } else {
            return "CANCELLING";
        }
    }

    { // JobNode
        bool ok = false;
        forEachSuperclass(stateClass.classObjectID, [&](ClassObjectID superclassID) {
            if (ok) {
                return;
            }
            const auto superclassName = getView(loadClasses.at(superclassID).nameStringID);
            if (superclassName == "kotlinx/coroutines/JobNode") {
                ok = true;
            }
        });
        if (ok) {
            return "ACTIVE";
        }
    }
    return "COMPLETED";
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

    const auto classInstances = getClassInstances(classObjectID);
    std::cout << std::format("{} (id={}, serial={}, {} instance(s)):\n",
                             name,
                             formatID(classObjectID),
                             c.classSerialNumber,
                             classInstances.size());

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

    if (classInstances.empty()) {
        return;
    }

    std::cout << "  instance(s):\n";
    for (const auto& objectID : classInstances) {
        printInstance(objectID, true, 4);
    }
}

std::string App::formatInstance(ObjectID id, std::string_view name) {
    const auto& instance  = instances.at(id);
    const auto& loadClass = loadClasses.at(instance.classObjectID);
    const auto  className = getView(loadClass.nameStringID);
    return std::format("{} {} = {}", className, name, formatID(id));
}

std::string App::formatCoroutine(ObjectID id) {
    const auto& instance  = instances.at(id);
    const auto& loadClass = loadClasses.at(instance.classObjectID);
    const auto  className = getView(loadClass.nameStringID).substr(19);
    return std::format("{}@{}, state: {}", className, formatID(id), getCoroutineState(id));
}

std::optional<ObjectID> App::getCoroutineParent(ObjectID coroutine) {
    const ID maybeParentHandleID = getFieldValue(coroutine, "_parentHandle$volatile");
    if (!isObjectID(maybeParentHandleID)) {
        return std::nullopt;
    }
    const auto parentHandleID = static_cast<ObjectID>(maybeParentHandleID);

    const auto& parentHandle          = instances.at(parentHandleID);
    const auto& parentHandleClass     = loadClasses.at(parentHandle.classObjectID);
    const auto  parentHandleClassName = getView(parentHandleClass.nameStringID);

    if (parentHandleClassName != "kotlinx/coroutines/ChildHandleNode") {
        return std::nullopt;
    }

    const auto maybeParentJobID = getFieldValue(parentHandleID, "job");
    if (!isObjectID(maybeParentJobID)) {
        return std::nullopt;
    }
    const auto parentJobID = static_cast<ObjectID>(maybeParentJobID);

    return parentJobID;
}

void App::printCoroutinesHierarchy(const std::unordered_set<ObjectID>& coroutines) {

    Forest<ObjectID> forest;

    std::unordered_map<ObjectID, Forest<ObjectID>::NodeHandle> IDToNode;

    for (const auto id : coroutines) {
        if (IDToNode.contains(id)) {
            continue;
        }

        std::stack<ObjectID>    path;
        std::optional<ObjectID> maybeParentID = getCoroutineParent(id);

        auto currID = id;

        while (true) {
            if (!maybeParentID.has_value()) {
                const auto node = forest.newRoot(currID);
                IDToNode.insert({currID, node});
                break;
            }
            path.push(currID);
            currID = maybeParentID.value();
            if (IDToNode.contains(currID)) {
                break;
            }
            maybeParentID = getCoroutineParent(currID);
        }

        auto prevNode = IDToNode.at(currID);

        while (!path.empty()) {
            currID = path.top();
            path.pop();
            const auto node = forest.newNode(currID, prevNode);
            IDToNode.insert({currID, node});
            prevNode = node;
        }
    }

    forest.forEachRoot([&](const auto root) {
        static constexpr size_t INDENT_STEP = 2;

        std::stack<std::pair<size_t, Forest<ObjectID>::NodeHandle>> toVisit;
        toVisit.push({0, root});
        while (!toVisit.empty()) {
            const auto [depth, node] = toVisit.top();
            toVisit.pop();
            const auto indent = depth * INDENT_STEP;
            std::cout << std::string(indent, ' ') << formatCoroutine(forest.getValue(node)) << '\n';
            for (const auto child : forest.getChildren(node)) {
                toVisit.push({depth + 1, child});
            }
        }
    });
}
