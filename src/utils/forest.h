#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

template <typename T>
class Forest {

public:
    Forest() = default;

private:
    Forest(const Forest&)            = delete;
    Forest& operator=(const Forest&) = delete;
    Forest(Forest&&)                 = delete;
    Forest& operator=(Forest&&)      = delete;

public:
    enum class NodeHandle : uint32_t { NONE = std::numeric_limits<uint32_t>::max() };

    NodeHandle newNode() {
        const auto handle = static_cast<NodeHandle>(nodes_.size());
        nodes_.emplace_back();
        return handle;
    }

    template <typename U>
    NodeHandle newRoot(U&& value) {
        const auto handle = static_cast<NodeHandle>(nodes_.size());
        nodes_.emplace_back(std::forward<U>(value));
        return handle;
    }

    template <typename U>
    NodeHandle newNode(U&& value, NodeHandle parent) {
        const auto handle = static_cast<NodeHandle>(nodes_.size());
        get_(parent).children.insert(handle);
        nodes_.emplace_back(std::forward<U>(value), parent);
        return handle;
    }

    // void removeParent(NodeHandle node) {
    //     auto& ref = get_(node);
    //     if (ref.parent != NodeHandle::NONE) {
    //         get_(ref.parent).children.erase(node);
    //         ref.parent = NodeHandle::NONE;
    //     }
    // }

    // void setParent(NodeHandle child, NodeHandle parent) {
    //     removeParent(child);
    //     get_(child).parent = parent;
    //     get_(parent).children.insert(child);
    // }

    const T& getValue(NodeHandle node) const {
        return get_(node).value;
    }

    NodeHandle getParent(NodeHandle node) const {
        return get_(node).parent;
    }

    const std::unordered_set<NodeHandle>& getChildren(NodeHandle node) const {
        return get_(node).children;
    }

    void forEachRoot(std::function<void(NodeHandle)> f) {
        for (uint32_t i = 0; i < std::ssize(nodes_); ++i) {
            if (nodes_[i].parent == NodeHandle::NONE) {
                f(static_cast<NodeHandle>(i));
            }
        }
    }

private:
    struct Node_ {
        const T                        value;
        const NodeHandle               parent = NodeHandle::NONE;
        std::unordered_set<NodeHandle> children;
    };

    const Node_& get_(NodeHandle handle) const {
        if (handle == NodeHandle::NONE) {
            throw std::runtime_error("null node handle dereference");
        }
        const auto idx = static_cast<std::underlying_type_t<NodeHandle>>(handle);
        return nodes_.at(idx);
    }

    Node_& get_(NodeHandle handle) {
        return const_cast<Node_&>(static_cast<const Forest*>(this)->get_(handle));
    }

private:
    std::vector<Node_> nodes_;
};
