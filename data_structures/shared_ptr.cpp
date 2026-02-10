#include <cstddef>
#include <iostream>
#include <utility>


namespace yuvicc {

template <typename T>
class customSharedPtr
{
public:
    customSharedPtr(T* ptr)
    : m_ptr(ptr),
    m_ref_count(new size_t(1))
    {}

    customSharedPtr(const customSharedPtr& other)                   // copy constructor
    : m_ptr(other.m_ptr),
    m_ref_count(other.m_ref_count)
    {
        *m_ref_count++;
    }

    customSharedPtr& operator=(const customSharedPtr& other)        // copy assignment opreator
    {
        // todo: cleanup existing data

        this->m_ptr = other.m_ptr;
        this->m_ref_count = other.m_ref_count;
        
        if(other.m_ptr != nullptr) {
            (*this->m_ref_count)++;
        }
        return *this;
    }

    customSharedPtr(customSharedPtr&& other)                        // move constructor
    : m_ptr(std::exchange(other.m_ptr, nullptr)),
    m_ref_count(std::exchange(other.m_ref_count, nullptr))
    {}

    customSharedPtr& operator=(customSharedPtr&& other)              // move assignment
    {
        // todo: cleanup existing data

        this->m_ptr = other.m_ptr;
        this->m_ref_count = other.m_ref_count;
        other.m_ptr = other.m_ref_count = nullptr;
    }

    ~customSharedPtr()
    {
        if (m_ref_count) {
            (*m_ref_count)--;
            if (*m_ref_count == 0) {
                delete m_ptr;
                delete m_ref_count;
            }
        }
    }

    T* operator->() const
    {
        return *this->m_ptr;
    }

    T& operator*() const
    {
        return *this->m_ptr;
    }

    long use_count() const noexcept
    {
        return static_cast<long>(*this->m_ref_count);
    
    }
    
    T* get() const noexcept
    {
        return this->m_ptr;
    }

private:
    T* m_ptr;
    std::size_t* m_ref_count;

};
}


int main()
{
    using namespace yuvicc;
    customSharedPtr<int> yuvicc_ptr = new int(6);
    std::cout << *yuvicc_ptr << std::endl;
}
