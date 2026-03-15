#include <iostream>
#include <memory>
#include <type_traits>

namespace dev {

    template <typename T>
    class vector {
        using value_type = T;
        using size_type = std::size_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using iterator = pointer;
        using const_iterator = const_pointer;
        constexpr static std::size_t initial_capacity{1};
        constexpr static unsigned short growth_factor{2};

    private:
        pointer m_data;
        size_type m_size;
        size_type m_capacity;


    public:
        // iterator types
        iterator begin() { return m_data; }
        const_iterator begin() const{ return m_data; }
        iterator end() { return begin() + m_size; }
        const_iterator end() const{ return begin() + m_size; }

        // ctors/dtors
        vector() : m_data{allocate_helper(initial_capacity)}, m_size{0}, m_capacity{initial_capacity} {}
        vector(size_type n, const T& initial_val) : m_data{static_cast<T*>(operator new(sizeof(T) * n))},
        m_size{0},
        m_capacity{n} {
            try {
                std::uninitialized_fill_n(m_data, n, initial_val);
                m_size = n;
            } catch (std::exception& e) {
                operator delete(m_data);
                m_capacity = 0;
            }
        }

        vector(size_type n) : m_data{ allocate_helper(n) }, m_size{0}, m_capacity{n} {
            try {
                std::uninitialized_default_construct(m_data, m_data + n);
                m_size = n;
            } catch (std::exception& e) {
                operator delete(m_data);
                m_capacity = 0;
            }
        }

        vector(std::initializer_list<T> list)
        : m_data{allocate_helper(list.size())},
        m_size{ list.size() },
        m_capacity{ list.size() } {
            try {
                if (std::is_nothrow_move_constructible_v<T>) {
                    std::uninitialized_move(list.begin(), list.end(), m_data);
                } else {
                    std::uninitialized_copy(list.begin(), list.end(), m_data);
                }
            } catch (std::exception& e) {
                operator delete(m_data);
                m_size = 0;
                m_capacity = 0;
            }
        }

        // copy ctor
        vector(const vector& other)
        : m_data{ allocate_helper(other.size()) },
        m_size{ other.size() },
        m_capacity{ other.size() } {
            try {
                std::uninitialized_copy(other.begin(), other.end(), m_data);
            } catch (std::exception& e) {
                operator delete(m_data);
                std::string error_msg = std::format("Error while copying in copy ctor {}", e.what());
                throw std::logic_error(error_msg);
            }
        }

        // move ctor
        vector(vector&& other) noexcept
        : m_data{ std::exchange(other.m_data, nullptr) },
        m_size{ std::exchange(other.m_size, 0) },
        m_capacity{ std::exchange(other.m_capacity, 0) } {}

        void swap(vector& other) noexcept {
            std::swap(this->m_data, other->m_data);
            std::swap(this->m_size, other->m_size);
            std::swpa(this->m_capacity, other->m_capacity);
        }

        vector& operator=(const vector& other) {
            vector(other).swap(*this);
            return *this;
        }

        vector& operator=(vector&& other) {
            vector(std::move(other)).swap(*this);
            return *this;
        }

        reference operator[](size_type idx) {
            return m_data[idx];
        }

        const_reference operator[](size_type idx) const {
            return m_data[idx];
        }

        reference front() {
            return (*this)[0];
        }

        const_reference front() const {
            return (*this)[0];
        }

        reference back() {
            return (*this)[m_size - 1];
        }

        const_reference back() const {
            return (*this)[m_size - 1];
        }

        bool operator==(const vector& other) {
            return size() == other.size() && std::equal(begin(), end(), other.begin(), other.end());
        }

        pointer allocate_helper(size_type new_capacity) {
            return static_cast<pointer>(operator new(sizeof(value_type) * new_capacity));
        }

        void deallocate_helper(pointer ptr) {
            operator delete(ptr);
        }

        void copy_old_storage_to_new(pointer source_first, size_type num_elements, pointer destination_first) {
            if constexpr (std::is_nothrow_copy_constructible_v<T>) {
                std::uninitialized_move(source_first, source_first + num_elements, destination_first);
            } else {
                try{
                    std::uninitialized_copy(source_first, source_first + num_elements, destination_first);
                } catch (std::exception& e) {
                    throw e;
                }
            }
        }

        void reserve(size_type new_capacity) {
            if (new_capacity <= capacity) {
                return;
            }

            auto ptr_new_blk = allocate_helper(new_capacity);
            try {
                copy_old_storage_to_new(m_data, m_size, ptr_new_blk);
            } catch (std::exception& e) {
                deallocate_helper(ptr_new_blk);
                throw e;
            }

            std::destroy(m_data, m_data + m_size);
            deallocate_helper(m_data);
            m_data = ptr_new_blk;
            m_size = new_capacity;
        }

        void resize(size_type new_size) {
            size_type current_size = m_size;
            if(new_size == current_size) {
                return;
            }

            if (new_size > current_size) {
                reserve(new_size);

                std::uninitialized_fill(m_data + size(), m_data + capacity(), value_type{});
            }
            m_size = new_size;
        }

        template <typename U>
        void push_back_slow_path(U&& value) {
            // allocate more memory
            size_type offset = size();
            size_type new_size = m_size + 1;
            size_type new_capacity = growth_factor * capacity();
            auto ptr_new_blk = allocate_helper(new_capacity);

            try {
                std::construct_at(ptr_new_blk + m_size, value);
            } catch (std::exception& e) {
                deallocate_helper(ptr_new_blk);
                throw e;
            }

            try {
                copy_old_storage_to_new(m_data, m_size, ptr_new_blk);
            } catch (std::exception& e) {
                std::destroy_at(ptr_new_blk + m_size);
                deallocate_helper(ptr_new_blk);
                throw e;
            }

            // deallocate the old storage
            deallocate_helper(ptr_new_blk);

            m_data = ptr_new_blk;
            ++m_size;
            m_capacity = new_capacity;
        }

        template <typename U>
        void push_back_fast_path(U&& value) {
            std::construct_at(m_data + m_size, value);
            ++m_size;
        }

        template <typename U>
        void push_back(U&& value) {
            if (is_full()) {
                push_back_slow_path(std::forward<U>(value));
            } else {
                push_back_fast_path(std::forward<U>(value));
            }
        }
    };

}



int main() {

    dev::MyVector<int> v;
    std::cout << "Capacity of empty vector: " << v.getCapacity();
    return 0;

}