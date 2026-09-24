#include <iostream>
#include <memory>
#include <utility>

namespace yuvicc
{
template <typename T> class vector
{
    using value_type = T;
    using size_type = std::size_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;

private:
    pointer m_data{nullptr};
    size_type m_size{0uz};
    size_type m_capacity{0uz};
    constexpr static unsigned short growth_factor{2};
    struct init_capacity_tag
    {
        size_type cap;
    };

public:
    iterator begin()
    {
        return m_data;
    }
    const_iterator begin() const
    {
        return m_data;
    }
    iterator end()
    {
        return begin() + m_size;
    }
    const_iterator end() const
    {
        return begin() + m_size;
    }

    // ctors & dtors
    vector() noexcept {}

    explicit vector(init_capacity_tag cap)
        : m_data{allocate_helper(cap.cap).release()}, m_capacity{cap.cap}
    {
    }

    vector(size_t n, const T& initial_value) : vector(init_capacity_tag(n))
    {
        std::uninitialized_fill_n(m_data, n, initial_value);
        m_size = n;
    }

    vector(std::initializer_list<T> list) : vector(init_capacity_tag(list.size()))
    {
        std::uninitialized_copy(list.begin(), list.end(), m_data);
        m_size = list.size();
    }

    vector(const_iterator begin, const_iterator end)
        : vector(init_capacity_tag(std::distance(begin, end)))
    {
        if constexpr (std::is_throw_move_constructible_v<T>)
        {
            std::uninitialized_move(begin, end, m_data);
        }
        else
        {
            std::uninitialized_copy(begin, end, m_data);
        }
        m_size = std::distance(begin, end);
    }

    vector(const vector& other) : vector(init_capacity_tag(other.capacity()))
    {
        std::uninitialized_copy(other.m_data, other.m_data + other.m_size, m_data);
        m_size = other.size();
    }

    vector(vector&& other) noexcept
        : m_data{std::exchange(other.m_data, nullptr)}, m_size{std::exchange(other.m_size, 0)},
          m_capacity{std::exchange(other.m_capacity, 0)}
    {
    }

    void swap(vector& other) noexcept
    {
        std::swap(this->m_data, other.m_data);
        std::swap(this->m_size, other.m_size);
        std::swap(this->m_capacity, other.m_capacity);
    }

    vector& operator=(const vector& other)
    {
        vector(other).swap(*this);
        return *this;
    }

    vector& operator=(vector&& other)
    {
        vector(std::move(other).swap(*this));
        return *this;
    }

    ~vector()
    {
        std::destroy(begin(), end());
        raw_deleter{}(m_data);
    }
}
} // namespace yuvicc

int main() {}
