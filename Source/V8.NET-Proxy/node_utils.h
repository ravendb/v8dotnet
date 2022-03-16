#include "v8.h"

template <typename T, size_t N>
constexpr size_t arraysize(const T (&)[N]) {
  return N;
}

// These should be used in our code as opposed to the native
// versions as they abstract out some platform and or
// compiler version specific functionality
// malloc(0) and realloc(ptr, 0) have implementation-defined behavior in
// that the standard allows them to either return a unique pointer or a
// nullptr for zero-sized allocation requests.  Normalize by always using
// a nullptr.
template <typename T>
inline T* UncheckedRealloc(T* pointer, size_t n);
template <typename T>
inline T* UncheckedMalloc(size_t n);
template <typename T>
inline T* UncheckedCalloc(size_t n);

// Same things, but aborts immediately instead of returning nullptr when
// no memory is available.
template <typename T>
inline T* Realloc(T* pointer, size_t n);
template <typename T>
inline T* Malloc(size_t n);
template <typename T>
inline T* Calloc(size_t n);

inline char* Malloc(size_t n);
inline char* Calloc(size_t n);
inline char* UncheckedMalloc(size_t n);
inline char* UncheckedCalloc(size_t n);

template <typename T>
inline T MultiplyWithOverflowCheck(T a, T b);

namespace per_process {
// Tells whether the per-process V8::Initialize() is called and
// if it is safe to call v8::Isolate::TryGetCurrent().
extern bool v8_initialized;
}  // namespace per_process



// Allocates an array of member type T. For up to kStackStorageSize items,
// the stack is used, otherwise malloc().
template <typename T, size_t kStackStorageSize = 1024>
class MaybeStackBuffer {
 public:
  const T* out() const {
    return buf_;
  }

  T* out() {
    return buf_;
  }

  // operator* for compatibility with `v8::String::(Utf8)Value`
  T* operator*() {
    return buf_;
  }

  const T* operator*() const {
    return buf_;
  }

  T& operator[](size_t index) {
    //CHECK_LT(index, length());
    return buf_[index];
  }

  const T& operator[](size_t index) const {
    //CHECK_LT(index, length());
    return buf_[index];
  }

  size_t length() const {
    return length_;
  }

  // Current maximum capacity of the buffer with which SetLength() can be used
  // without first calling AllocateSufficientStorage().
  size_t capacity() const {
    return capacity_;
  }

  // Make sure enough space for `storage` entries is available.
  // This method can be called multiple times throughout the lifetime of the
  // buffer, but once this has been called Invalidate() cannot be used.
  // Content of the buffer in the range [0, length()) is preserved.
  void AllocateSufficientStorage(size_t storage) {
    //CHECK(!IsInvalidated());
    if (storage > capacity()) {
      bool was_allocated = IsAllocated();
      T* allocated_ptr = was_allocated ? buf_ : nullptr;
      buf_ = Realloc(allocated_ptr, storage);
      capacity_ = storage;
      if (!was_allocated && length_ > 0)
        memcpy(buf_, buf_st_, length_ * sizeof(buf_[0]));
    }

    length_ = storage;
  }

  void SetLength(size_t length) {
    // capacity() returns how much memory is actually available.
    //CHECK_LE(length, capacity());
    length_ = length;
  }

  void SetLengthAndZeroTerminate(size_t length) {
    // capacity() returns how much memory is actually available.
    //CHECK_LE(length + 1, capacity());
    SetLength(length);

    // T() is 0 for integer types, nullptr for pointers, etc.
    buf_[length] = T();
  }

  // Make dereferencing this object return nullptr.
  // This method can be called multiple times throughout the lifetime of the
  // buffer, but once this has been called AllocateSufficientStorage() cannot
  // be used.
  void Invalidate() {
    //CHECK(!IsAllocated());
    capacity_ = 0;
    length_ = 0;
    buf_ = nullptr;
  }

  // If the buffer is stored in the heap rather than on the stack.
  bool IsAllocated() const {
    return !IsInvalidated() && buf_ != buf_st_;
  }

  // If Invalidate() has been called.
  bool IsInvalidated() const {
    return buf_ == nullptr;
  }

  // Release ownership of the malloc'd buffer.
  // Note: This does not free the buffer.
  void Release() {
    //CHECK(IsAllocated());
    buf_ = buf_st_;
    length_ = 0;
    capacity_ = arraysize(buf_st_);
  }

  MaybeStackBuffer()
      : length_(0), capacity_(arraysize(buf_st_)), buf_(buf_st_) {
    // Default to a zero-length, null-terminated buffer.
    buf_[0] = T();
  }

  explicit MaybeStackBuffer(size_t storage) : MaybeStackBuffer() {
    AllocateSufficientStorage(storage);
  }

  ~MaybeStackBuffer() {
    if (IsAllocated())
      free(buf_);
  }

 private:
  size_t length_;
  // capacity of the malloc'ed buf_
  size_t capacity_;
  T* buf_;
  T buf_st_[kStackStorageSize];
};


