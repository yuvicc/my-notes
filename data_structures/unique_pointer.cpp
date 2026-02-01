#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>
#include <print>
#include <utility>

namespace yuvicc {

template <typename T, typename Deleter = std::default_delete<T>>
class customUniquePointer
{
public:
    using pointer = T*;

    constexpr customUniquePointer() noexcept {}     //ctor

    constexpr customUniquePointer(std::nullptr_t) noexcept {}

    explicit customUniquePointer(pointer ptr) noexcept
    : mPtr{ptr}
    {}

    customUniquePointer(pointer ptr, const Deleter& d) noexcept
    : mPtr{ptr}
    , mDeleter{d}
    {}
    
    customUniquePointer(pointer ptr, Deleter&& d) noexcept
    : mPtr{ptr}
    , mDeleter{std::move(d)}
    {}

    constexpr ~customUniquePointer()      //dtor
    {
        mDeleter(std::exchange(mPtr, nullptr));
    }

    customUniquePointer(customUniquePointer const& other) = delete;                 // copy ctor
    customUniquePointer& operator=(customUniquePointer const& other) = delete;      // copy assignment

    customUniquePointer(customUniquePointer&& other) noexcept                   // move ctor
    : mPtr{std::exchange(other.mPtr, nullptr)}
    , mDeleter{std::move(other.mDeleter)}
    {}
    
    template < typename U, typename E >
    customUniquePointer(customUniquePointer<U, E>&& u) noexcept requires
        std::is_convertible_v<typename customUniquePointer<U, E>::pointer, pointer>
        && (not std::is_array_v<U>)
    : mPtr{u.release()}
    , mDeleter{std::forward<E>(u.get_deleter())}
    {}

    customUniquePointer& operator=(customUniquePointer&& other)        // move assignment
    {
        mDeleter(mPtr);
        mPtr = std::exchange(other.mPtr, nullptr);
        mDeleter = std::move(other.mDeleter);
        return *this;
    }

    customUniquePointer& operator=(std::nullptr_t) noexcept
    {
        mDeleter(mPtr);
        mPtr = nullptr;
        return *this;
    }

    pointer release()
    {
        return std::exchange(mPtr, nullptr);
    }
    
    friend void swap(customUniquePointer& ptr1, customUniquePointer& ptr2) noexcept
    {
        std::swap(ptr1.mPtr, ptr2.mPtr);
        std::swap(ptr1.mDeleter, ptr2.mDeleter);
    }

    std::add_lvalue_reference<T>::type operator*() const
    {
        return *mPtr;
    }

    pointer operator->() const noexcept
    {
        return mPtr;
    }

    pointer get()
    {
        return mPtr;
    }
    
    Deleter& get_deleter() noexcept
    {
        return mDeleter;
    }

    // checks whether this pointer is null
    explicit operator bool() const
    {
        return mPtr != nullptr;
    }

private:
    pointer mPtr = nullptr;
    Deleter mDeleter;
};

template <typename T, typename... Args>
std::enable_if_t<!std::is_array<T>::value, customUniquePointer<T>>
make_unique(Args&&... args)
{
    return customUniquePointer<T>(new T(std::forward<Args>(args)...));
}

}

int main()
{
    using namespace yuvicc;

    customUniquePointer<int> cup1;
    assert(!cup1);


    cup1 = customUniquePointer<int>(new int(21));
    assert(*cup1 == 21);


    *cup1 = 22;
    assert(*cup1 == 22);

    customUniquePointer<int> cup2{new int(31)};
    assert(*cup2 == 31);


    cup2 = std::move(cup1);
    assert(*cup2 == 22);
    assert(!cup1);
        
    std::print("Hello Custom Unique pointer\n");
    
    auto mDeleter = [](int* ptr) {
        delete ptr; 
    };

    customUniquePointer<int, decltype(mDeleter)> cup4{new int{6}, mDeleter}; 
    std::println("Hello from custom unique pointer using custom deleter", *cup4);
    return 0;
}
