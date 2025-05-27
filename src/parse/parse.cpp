#include <parse/parse.h>

DumpSummary summarizeDump(R r, size_t identifierSize) {
    DumpSummary summary;
    while (!r.eof()) {
        const auto recordHeader = parseRecordHeader(r);
        switch (recordHeader.tag) {
        case Tag::HEAP_DUMP:
        case Tag::HEAP_DUMP_SEGMENT: {
            R dr(r.it(), recordHeader.bodyByteSize);
            while (!dr.eof()) {
                const SubTag subTag = validateSubTag(dr.read<uint8_t>());

                ++summary.subTagCounts[subTag];
                summary.numSubtags++;

                const size_t subRecordBodySizeBytes = subTagSize(subTag, identifierSize);

                if (subRecordBodySizeBytes != DYNAMIC) {
                    dr.skip(subRecordBodySizeBytes);
                    continue;
                }

                switch (subTag) {
                    using enum SubTag;
                case CLASS_DUMP: {
                    skipClassDump(dr, identifierSize);
                    break;
                }
                case INSTANCE_DUMP: {
                    skipInstanceDump(dr, identifierSize);
                    break;
                }
                case OBJECT_ARRAY_DUMP: {
                    skipObjectArrayDump(dr, identifierSize);
                    break;
                }
                case PRIMITIVE_ARRAY_DUMP: {
                    skipPrimitiveArrayDump(dr, identifierSize);
                    break;
                }
                default:
                    throw std::runtime_error(std::format(
                        "unexpected dynamic sub-tag {} (0x{:02X})", subTagName(subTag), static_cast<uint8_t>(subTag)));
                }
            }
            r.skip(recordHeader.bodyByteSize);
            if (dr.it() != r.it()) {
                throw std::runtime_error("specified and actual record body sizes differ");
            }
            break;
        }
        default: r.skip(recordHeader.bodyByteSize);
        }

        ++summary.tagCounts[recordHeader.tag];
        ++summary.numRecords;
    }
    return summary;
}

DumpHeader parseDumpHeader(R& r) {
    DumpHeader dumpHeader;
    r.read(dumpHeader.identifierSize);
    r.read(dumpHeader.millis);
    return dumpHeader;
}

RecordHeader parseRecordHeader(R& r) {
    RecordHeader recordHeader;
    recordHeader.tag = validateTag(r.read<uint8_t>());
    r.read(recordHeader.micros);
    r.read(recordHeader.bodyByteSize);
    return recordHeader;
}

void parseDumpBody(R r, const std::unordered_map<Tag, TagHandler>& tagHandlers) {
    while (!r.eof()) {
        const auto recordHeader = parseRecordHeader(r);
        const auto it           = tagHandlers.find(recordHeader.tag);
        if (it != tagHandlers.end()) {
            auto& handler = it->second;
            handler(r, recordHeader);
        } else {
            r.skip(recordHeader.bodyByteSize);
        }
    }
}

std::unordered_map<StringID, StringInUTF8> parseStrings(R r, size_t identifierSize) {
    std::unordered_map<StringID, StringInUTF8> strings;
    const std::unordered_map<Tag, TagHandler>  handlers = {
        {Tag::STRING_IN_UTF8,
         [&](R& r, const RecordHeader& recordHeader) {
             StringInUTF8 s;
             r.read(s.id, identifierSize);
             const auto data = r.skip(recordHeader.bodyByteSize - identifierSize);
             s.view          = {reinterpret_cast<const char*>(data.data()), data.size_bytes()};
             strings.insert({s.id, s});
         }},
    };
    parseDumpBody(r, handlers);
    return strings;
}

std::unordered_map<ClassObjectID, LoadClass> parseLoadClasses(R r, const DumpHeader& dumpHeader) {
    std::unordered_map<ClassObjectID, LoadClass> loadClasses;
    const std::unordered_map<Tag, TagHandler>    handlers = {
        {Tag::LOAD_CLASS,
         [&](R& r, const RecordHeader&) {
             LoadClass c;
             r.read(c.classSerialNumber);
             r.read(c.classObjectID, dumpHeader.identifierSize);
             r.read(c.stackTraceSerialNumber);
             r.read(c.nameStringID, dumpHeader.identifierSize);
             loadClasses.insert({c.classObjectID, c});
         }},
    };
    parseDumpBody(r, handlers);
    return loadClasses;
}

void skipClassDump(R& r, size_t identifierSize) {
    r.skip(identifierSize + 4 + identifierSize * 6 + 4);

    const auto nConstants = r.read<uint16_t>();
    for (size_t i = 0; i < nConstants; ++i) {
        r.skip(2);
        const auto type = validateBasicType(r.read<uint8_t>());
        r.skip(basicTypeSize(type));
    }

    const auto nStatics = r.read<uint16_t>();
    for (size_t i = 0; i < nStatics; ++i) {
        r.skip(identifierSize);
        const auto type = validateBasicType(r.read<uint8_t>());
        r.skip(basicTypeSize(type));
    }

    const auto nFields = r.read<uint16_t>();
    r.skip((identifierSize + 1) * nFields);
}

void skipInstanceDump(R& r, size_t identifierSize) {
    r.skip(identifierSize + 4 + identifierSize);
    const auto fieldsSizeBytes = r.read<uint32_t>();
    r.skip(fieldsSizeBytes);
}

void skipObjectArrayDump(R& r, size_t identifierSize) {
    r.skip(identifierSize + 4);
    const auto nElements = r.read<uint32_t>();
    r.skip(identifierSize + identifierSize * nElements);
}

void skipPrimitiveArrayDump(R& r, size_t identifierSize) {
    r.skip(identifierSize + 4);
    const auto nElements = r.read<uint32_t>();
    const auto type      = validateBasicType(r.read<uint8_t>());
    r.skip(basicTypeSize(type) * nElements);
}

void parseHeapDumpSegment(R& r, size_t identifierSize,
                          const std::unordered_map<SubTag, SubTagHandler>& subTagHandlers) {
    static const std::unordered_map<SubTag, SubTagHandler> defaultDynamicSubTagHandlers = {
        {          SubTag::CLASS_DUMP,          [=](R& r) { skipClassDump(r, identifierSize); }},
        {       SubTag::INSTANCE_DUMP,       [=](R& r) { skipInstanceDump(r, identifierSize); }},
        {   SubTag::OBJECT_ARRAY_DUMP,    [=](R& r) { skipObjectArrayDump(r, identifierSize); }},
        {SubTag::PRIMITIVE_ARRAY_DUMP, [=](R& r) { skipPrimitiveArrayDump(r, identifierSize); }},
    };
    while (!r.eof()) {
        SubTag subTag = validateSubTag(r.read<uint8_t>());
        if (const auto it = subTagHandlers.find(subTag); it != subTagHandlers.end()) {
            auto& handler = it->second;
            handler(r);
        } else {
            size_t subRecordBodySize = subTagSize(subTag, identifierSize);
            if (subRecordBodySize == DYNAMIC) {
                if (const auto it = defaultDynamicSubTagHandlers.find(subTag);
                    it != defaultDynamicSubTagHandlers.end()) {
                    auto& defaultHandler = it->second;
                    defaultHandler(r);
                } else {
                    throw std::runtime_error(std::format(
                        "unexpected dynamic sub-tag {} (0x{:02X})", subTagName(subTag), static_cast<uint8_t>(subTag)));
                }
            } else {
                r.skip(subRecordBodySize);
            }
        }
    }
}

std::function<void(R&, const RecordHeader&)>
createHeapDumpSegmentHandler(size_t identifierSize, const std::unordered_map<SubTag, SubTagHandler>& subTagHandlers) {
    return [identifierSize, &subTagHandlers](R& r, const RecordHeader& recordHeader) {
        R hdsr(r.it(), recordHeader.bodyByteSize);
        parseHeapDumpSegment(hdsr, identifierSize, subTagHandlers);
        r.skip(recordHeader.bodyByteSize);
        if (hdsr.it() != r.it()) {
            throw std::runtime_error("specified and actual record body sizes differ");
        }
    };
}

std::unordered_map<ClassObjectID, ClassDump> parseClassDumps(R r, size_t identifierSize) {
    std::unordered_map<ClassObjectID, ClassDump>    classDumps;
    const std::unordered_map<SubTag, SubTagHandler> subTagHandlers = {
        {SubTag::CLASS_DUMP, [&](R& r) {
             ClassDump cd;
             r.read(cd.classObjectID, identifierSize);
             r.read(cd.stackTrackeSerialNumber);
             r.read(cd.superclassObjectID, identifierSize);
             r.read(cd.classLoaderObjectID, identifierSize);
             r.read(cd.signersObjectID, identifierSize);
             r.read(cd.protectionDomainObjectID, identifierSize);
             r.skip(identifierSize * 2); // reserved
             r.read(cd.instanceSizeBytes);

             const auto nConstants = r.read<uint16_t>();
             for (size_t i = 0; i < nConstants; ++i) {
                 ClassDump::Constant c;
                 r.read(c.constantPoolIndex);
                 c.type = validateBasicType(r.read<uint8_t>());
                 r.read(c.value, basicTypeSize(c.type));
                 cd.constants.push_back(std::move(c));
             }
             const auto nStatics = r.read<uint16_t>();
             for (size_t i = 0; i < nStatics; ++i) {
                 ClassDump::Static s;
                 r.read(s.nameStringID, identifierSize);
                 s.type = validateBasicType(r.read<uint8_t>());
                 r.read(s.value, basicTypeSize(s.type));
                 cd.statics.push_back(std::move(s));
             }

             const auto nFields = r.read<uint16_t>();
             for (size_t i = 0; i < nFields; ++i) {
                 ClassDump::Field f;
                 r.read(f.nameStringID, identifierSize);
                 f.type = validateBasicType(r.read<uint8_t>());
                 cd.fields.push_back(std::move(f));
             }

             const auto id = cd.classObjectID;
             classDumps.insert({id, std::move(cd)});
         }}
    };
    const auto heapDumpSegmentTagHandler               = createHeapDumpSegmentHandler(identifierSize, subTagHandlers);
    const std::unordered_map<Tag, TagHandler> handlers = {
        {        Tag::HEAP_DUMP, heapDumpSegmentTagHandler},
        {Tag::HEAP_DUMP_SEGMENT, heapDumpSegmentTagHandler},
    };
    parseDumpBody(r, handlers);
    return classDumps;
}

std::unordered_map<ClassObjectID, size_t> countInstances(R r, size_t identifierSize) {
    std::unordered_map<ClassObjectID, size_t>       counts;
    const std::unordered_map<SubTag, SubTagHandler> subTagHandlers = {
        {SubTag::INSTANCE_DUMP,
         [&](R& r) {
             r.skip(identifierSize + 4);
             const auto classObjectID = r.read<ClassObjectID>(identifierSize);
             ++counts[classObjectID];
             const auto fieldsSizeBytes = r.read<uint32_t>();
             r.skip(fieldsSizeBytes);
         }},
    };
    const auto heapDumpSegmentTagHandler               = createHeapDumpSegmentHandler(identifierSize, subTagHandlers);
    const std::unordered_map<Tag, TagHandler> handlers = {
        {        Tag::HEAP_DUMP, heapDumpSegmentTagHandler},
        {Tag::HEAP_DUMP_SEGMENT, heapDumpSegmentTagHandler},
    };
    parseDumpBody(r, handlers);
    return counts;
}

InstanceDump parseInstanceDump(R& r, size_t identifierSize) {
    InstanceDump i;
    r.read(i.objectID, identifierSize);
    r.read(i.stackTraceSerialNumber);
    r.read(i.classObjectID, identifierSize);
    const auto fieldsSizeBytes = r.read<uint32_t>();
    i.fieldsView               = r.skip(fieldsSizeBytes);
    return i;
}

std::unordered_map<ObjectID, const std::byte*> parseAllInstanceLocations(R r, size_t identifierSize) {
    std::unordered_map<ObjectID, const std::byte*>  locations;
    const std::unordered_map<SubTag, SubTagHandler> subTagHandlers = {
        {SubTag::INSTANCE_DUMP,
         [&, identifierSize](R& r) {
             const std::byte* location = r.it();
             const ObjectID   objectID = r.read<ObjectID>(identifierSize);
             locations.insert({objectID, location});
             r.skip(4 + identifierSize);
             const auto fieldsSizeBytes = r.read<uint32_t>();
             r.skip(fieldsSizeBytes);
         }},
    };
    const auto heapDumpSegmentTagHandler               = createHeapDumpSegmentHandler(identifierSize, subTagHandlers);
    const std::unordered_map<Tag, TagHandler> handlers = {
        {        Tag::HEAP_DUMP, heapDumpSegmentTagHandler},
        {Tag::HEAP_DUMP_SEGMENT, heapDumpSegmentTagHandler},
    };
    parseDumpBody(r, handlers);
    return locations;
}

std::unordered_map<ObjectID, InstanceDump> parseClassInstances(R r, size_t identifierSize, ClassObjectID target) {
    std::unordered_map<ObjectID, InstanceDump>      instances;
    const std::unordered_map<SubTag, SubTagHandler> subTagHandlers = {
        {SubTag::INSTANCE_DUMP,
         [&, identifierSize, target](R& r) {
             const auto instance = parseInstanceDump(r, identifierSize);
             if (instance.classObjectID == target) {
                 auto const objectID = instance.objectID;
                 instances.insert({objectID, std::move(instance)});
             }
         }},
    };
    const auto heapDumpSegmentTagHandler               = createHeapDumpSegmentHandler(identifierSize, subTagHandlers);
    const std::unordered_map<Tag, TagHandler> handlers = {
        {        Tag::HEAP_DUMP, heapDumpSegmentTagHandler},
        {Tag::HEAP_DUMP_SEGMENT, heapDumpSegmentTagHandler},
    };
    parseDumpBody(r, handlers);
    return instances;
}

std::unordered_map<StackFrameID, StackFrame> parseStackFrames(R r, size_t identifierSize) {
    std::unordered_map<StackFrameID, StackFrame> frames;
    std::unordered_map<Tag, TagHandler>          tagHandlers = {
        {Tag::STACK_FRAME,
         [&, identifierSize](R& r, const RecordHeader&) {
             StackFrame frame;
             r.read(frame.stackFrameID, identifierSize);
             r.read(frame.methodNameStringID, identifierSize);
             r.read(frame.methodSignatureStringID, identifierSize);
             r.read(frame.sourceFileNameStringID, identifierSize);
             r.read(frame.classSerialNumber);
             r.read(frame.lineNumber);
             frames.insert({frame.stackFrameID, frame});
         }},
    };
    parseDumpBody(r, tagHandlers);
    return frames;
}

std::unordered_map<StackTraceSerialNumber, StackTrace> parseStackTraces(R r, size_t identifierSize) {
    std::unordered_map<StackTraceSerialNumber, StackTrace> traces;
    std::unordered_map<Tag, TagHandler>                    tagHandlers = {
        {Tag::STACK_TRACE,
         [&, identifierSize](R& r, const RecordHeader&) {
             StackTrace trace;
             r.read(trace.stackTraceSerialNumber);
             r.read(trace.threadSerialNumber);
             r.read(trace.numberOfFrames);
             for (int64_t i = 0; i < trace.numberOfFrames; ++i) {
                 trace.stackFrames.push_back(r.read<StackFrameID>(identifierSize));
             }
             const auto serialNumber = trace.stackTraceSerialNumber;
             traces.insert({serialNumber, std::move(trace)});
         }},
    };
    parseDumpBody(r, tagHandlers);
    return traces;
}

std::unordered_map<ArrayObjectID, ObjectArrayDump> parseObjectArrayDumps(R r, size_t identifierSize) {
    std::unordered_map<ArrayObjectID, ObjectArrayDump> objectArrays;
    const std::unordered_map<SubTag, SubTagHandler>    subTagHandlers = {
        {SubTag::OBJECT_ARRAY_DUMP,
         [&, identifierSize](R& r) {
             ObjectArrayDump array;
             r.read(array.arrayObjectID, identifierSize);
             r.read(array.stackTraceSerialNumber);
             r.read(array.numberOfElements);
             r.read(array.arrayClassObjectID);
             array.elementsView = r.skip(identifierSize * array.numberOfElements);
             const auto id      = array.arrayObjectID;
             objectArrays.insert({id, std::move(array)});
         }},
    };
    const auto heapDumpSegmentTagHandler               = createHeapDumpSegmentHandler(identifierSize, subTagHandlers);
    const std::unordered_map<Tag, TagHandler> handlers = {
        {        Tag::HEAP_DUMP, heapDumpSegmentTagHandler},
        {Tag::HEAP_DUMP_SEGMENT, heapDumpSegmentTagHandler},
    };
    parseDumpBody(r, handlers);
    return objectArrays;
}

std::unordered_map<ArrayObjectID, PrimitiveArrayDump> parsePrimitiveArrayDumps(R r, size_t identifierSize) {
    std::unordered_map<ArrayObjectID, PrimitiveArrayDump> primitiveArrays;
    const std::unordered_map<SubTag, SubTagHandler>       subTagHandlers = {
        {SubTag::PRIMITIVE_ARRAY_DUMP,
         [&, identifierSize](R& r) {
             PrimitiveArrayDump array;
             r.read(array.arrayObjectID, identifierSize);
             r.read(array.stackTraceSerialNumber);
             r.read(array.numberOfElements);
             array.elementType  = validateBasicType(r.read<uint8_t>());
             array.elementsView = r.skip(basicTypeSize(array.elementType) * array.numberOfElements);
             const auto id      = array.arrayObjectID;
             primitiveArrays.insert({id, std::move(array)});
         }},
    };
    const auto heapDumpSegmentTagHandler               = createHeapDumpSegmentHandler(identifierSize, subTagHandlers);
    const std::unordered_map<Tag, TagHandler> handlers = {
        {        Tag::HEAP_DUMP, heapDumpSegmentTagHandler},
        {Tag::HEAP_DUMP_SEGMENT, heapDumpSegmentTagHandler},
    };
    parseDumpBody(r, handlers);
    return primitiveArrays;
}

std::unordered_map<ObjectID, InstanceDump> parseInstanceDumps(R r, size_t identifierSize) {
    std::unordered_map<ObjectID, InstanceDump>      instances;
    const std::unordered_map<SubTag, SubTagHandler> subTagHandlers = {
        {SubTag::INSTANCE_DUMP,
         [&, identifierSize](R& r) {
             const auto instance = parseInstanceDump(r, identifierSize);
             const auto objectID = instance.objectID;
             instances.insert({objectID, std::move(instance)});
         }},
    };
    const auto heapDumpSegmentTagHandler               = createHeapDumpSegmentHandler(identifierSize, subTagHandlers);
    const std::unordered_map<Tag, TagHandler> handlers = {
        {        Tag::HEAP_DUMP, heapDumpSegmentTagHandler},
        {Tag::HEAP_DUMP_SEGMENT, heapDumpSegmentTagHandler},
    };
    parseDumpBody(r, handlers);
    return instances;
}
