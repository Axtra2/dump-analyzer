#pragma once

#include <data/data.h>
#include <utils/reader.h>

#include <cstddef>
#include <functional>
#include <map>
#include <unordered_map>

using TagHandler    = std::function<void(R&, const RecordHeader&)>;
using SubTagHandler = std::function<void(R&)>;

struct DumpSummary {
    size_t                   numRecords = 0;
    size_t                   numSubtags = 0;
    std::map<Tag, size_t>    tagCounts;
    std::map<SubTag, size_t> subTagCounts;
};

DumpSummary summarizeDump(R r, size_t identifierSize);

DumpHeader   parseDumpHeader(R& r);
RecordHeader parseRecordHeader(R& r);

void parseDumpBody(R r, const std::unordered_map<Tag, TagHandler>& tagHandlers);

std::unordered_map<StringID, StringInUTF8> parseStrings(R r, size_t identifierSize);

std::unordered_map<ClassObjectID, LoadClass> parseLoadClasses(R r, const DumpHeader& dumpHeader);

void skipClassDump(R& r, size_t identifierSize);
void skipInstanceDump(R& r, size_t identifierSize);
void skipObjectArrayDump(R& r, size_t identifierSize);
void skipPrimitiveArrayDump(R& r, size_t identifierSize);

void parseHeapDumpSegment(R& r, size_t identifierSize, const std::unordered_map<SubTag, SubTagHandler>& subTagHandlers);

std::function<void(R&, const RecordHeader&)>
createHeapDumpSegmentHandler(size_t identifierSize, const std::unordered_map<SubTag, SubTagHandler>& subTagHandlers);

std::unordered_map<ClassObjectID, ClassDump> parseClassDumps(R r, size_t identifierSize);

std::unordered_map<ClassObjectID, size_t> countInstances(R r, size_t identifierSize);

InstanceDump parseInstanceDump(R& r, size_t identifierSize);

std::unordered_map<ObjectID, const std::byte*> parseAllInstanceLocations(R r, size_t identifierSize);

std::unordered_map<ObjectID, InstanceDump> parseClassInstances(R r, size_t identifierSize, ClassObjectID target);

std::unordered_map<StackFrameID, StackFrame> parseStackFrames(R r, size_t identifierSize);

std::unordered_map<StackTraceSerialNumber, StackTrace> parseStackTraces(R r, size_t identifierSize);

std::unordered_map<ArrayObjectID, ObjectArrayDump> parseObjectArrayDumps(R r, size_t identifierSize);

std::unordered_map<ArrayObjectID, PrimitiveArrayDump> parsePrimitiveArrayDumps(R r, size_t identifierSize);

std::unordered_map<ObjectID, InstanceDump> parseInstanceDumps(R r, size_t identifierSize);
