#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <gsl/gsl>
#include <nonstd/optional.hpp>

#include "xchainer/array_repr.h"
#include "xchainer/dtype.h"
#include "xchainer/scalar.h"
#include "xchainer/shape.h"

namespace xchainer {

class Array;
class ArrayNode;

struct ArrayNodeProperty {
    ArrayNodeProperty(std::shared_ptr<ArrayNode> node, bool requires_grad) : node(std::move(node)), requires_grad(requires_grad) {}

    std::shared_ptr<ArrayNode> node;
    bool requires_grad;
};

namespace internal {

// Data holder of Array.
//
// C++ Array and Python bindings both share ArrayBody through shared_ptr. C++ Array provides the value-based semantics of Array in C++,
// while Python Array provides the reference-based semantics, which is more natural in Python.
//
// The current design requires a subtle overhead on converting between C++ Array and Python Array (due to reference counting), which is
// currently considered to be ignorable compared to other Python operations.
//
// NOTE: This class should not be instantiated by any functions except those defined in array.cc. This class is still defined
// here so that
// the code is made simple and we can use inline access to each member from member accessor functions of Array.
class ArrayBody {
public:
    ArrayBody(const Shape& shape, Dtype dtype, bool is_contiguous, std::shared_ptr<void> data, int64_t offset);

    // Generates a new ArrayNode if if does not exist for this graph, otherwise return the existing one
    ArrayNode& GetNode(const std::string& graph_name, bool requires_grad);

private:
    friend class ::xchainer::Array;

    // TODO(hvy): Clean up intermediate accessors
    // Find ArrayNodeProperty assuming it exists
    const ArrayNodeProperty& GetNodeProperty(const std::string& graph_name) const;

    // TODO(hvy): Clean up intermediate accessors
    ArrayNodeProperty& GetMutableNodeProperty(const std::string& graph_name);

    std::shared_ptr<const ArrayNode> node(const std::string& graph_name) {
        return std::const_pointer_cast<const ArrayNode>(GetNodeProperty(graph_name).node);
    }

    const std::vector<std::pair<std::string, ArrayNodeProperty>>& nodes() const { return nodes_; };

    bool requires_grad(const std::string& graph_name) const { return GetNodeProperty(graph_name).requires_grad; }

    void set_requires_grad(bool requires_grad, const std::string& graph_name) {
        GetMutableNodeProperty(graph_name).requires_grad = requires_grad;
    }

    const std::shared_ptr<ArrayNode>& mutable_node(const std::string& graph_name) { return GetNodeProperty(graph_name).node; }

    Shape shape_;
    Dtype dtype_;
    bool is_contiguous_;
    std::shared_ptr<void> data_;
    int64_t offset_;
    std::vector<std::pair<std::string, ArrayNodeProperty>> nodes_;
};

}  // namespace internal

// The main data structure of multi-dimensional array.
class Array {
public:
    // Deep copy ctor and copy assignment
    Array(const Array& other);

    Array(Array&& other) = default;
    Array& operator=(Array&& other) = delete;

    // TODO(hvy): Copy assignment operator is deleted to avoid performance drops due to possible unwanted copies and heavy refactorings
    // later on until the behavior is better agreed upon
    Array& operator=(const Array&) = delete;

    explicit Array(gsl::not_null<std::shared_ptr<internal::ArrayBody>> body) : body_(std::move(body)) {}

    static Array FromBuffer(const Shape& shape, Dtype dtype, std::shared_ptr<void> data);

    static Array Empty(const Shape& shape, Dtype dtype);
    static Array Full(const Shape& shape, Scalar scalar, Dtype dtype);
    static Array Full(const Shape& shape, Scalar scalar);
    static Array Zeros(const Shape& shape, Dtype dtype);
    static Array Ones(const Shape& shape, Dtype dtype);

    // Creates an array which has the same shape and dtype as the other array.
    // The new array is allocated in the current device. The device of the other array
    // is ignored.
    static Array EmptyLike(const Array& array);
    static Array FullLike(const Array& array, Scalar scalar);
    static Array ZerosLike(const Array& array);
    static Array OnesLike(const Array& array);

    const std::shared_ptr<internal::ArrayBody>& body() { return body_; }
    std::shared_ptr<const internal::ArrayBody> body() const { return body_; }

    Dtype dtype() const { return body_->dtype_; }

    int8_t ndim() const { return shape().ndim(); }

    const Shape& shape() const { return body_->shape_; }

    int64_t total_size() const { return shape().total_size(); }

    int64_t element_bytes() const { return GetElementSize(dtype()); }

    int64_t total_bytes() const { return total_size() * element_bytes(); }

    const std::shared_ptr<void>& data() { return body_->data_; }

    std::shared_ptr<const void> data() const { return body_->data_; }

    bool requires_grad(const std::string& graph_name = "") const { return body_->requires_grad(graph_name); }

    void set_requires_grad(bool requires_grad, const std::string& graph_name = "") { body_->set_requires_grad(requires_grad, graph_name); }

    bool is_contiguous() const { return body_->is_contiguous_; }

    int64_t offset() const { return body_->offset_; }

    const std::shared_ptr<ArrayNode>& mutable_node(const std::string& graph_name = "") const { return body_->mutable_node(graph_name); }

    std::shared_ptr<const ArrayNode> node(const std::string& graph_name = "") const { return body_->node(graph_name); }

    const std::vector<std::pair<std::string, ArrayNodeProperty>>& nodes() const { return body_->nodes(); };

    Array& GetNode(const std::string& graph_name, bool requires_grad = true) {
        body_->GetNode(graph_name, requires_grad);
        return *this;
    }

    const nonstd::optional<Array>& grad(const std::string& graph_name = "") const;

    void set_grad(Array grad, const std::string& graph_name = "");

    void ClearGrad(const std::string& graph_name = "");

    Array MakeView() const { return Array{body_}; }

    Array& operator+=(const Array& rhs);
    Array& operator*=(const Array& rhs);
    Array operator+(const Array& rhs) const;
    Array operator*(const Array& rhs) const;

    Array Copy() const;

    void Fill(Scalar value);

    std::string ToString() const;

private:
    Array(const Shape& shape, Dtype dtype, std::shared_ptr<void> data, bool is_contiguous = true, int64_t offset = 0);

    void CopyTo(Array& out) const;
    void Add(const Array& rhs, Array& out) const;
    void Mul(const Array& rhs, Array& out) const;

    std::shared_ptr<internal::ArrayBody> body_;
};

void DebugDumpComputationalGraph(std::ostream& os, const Array& array, int indent = 0);

}  // namespace xchainer
