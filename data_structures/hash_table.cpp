#include <iostream>
#include <vector>
#include <string>
#include <optional>

// initial capacity for table - 16
constexpr std::size_t INITIAL_CAPACITY = 16;

// Hash table entry struct
template <typename T>
struct HashTableEntry {
    enum class SlotState {
        Empty,
        Occupied,
        Dead
    };

    std::string key;
    T value;
    SlotState state = SlotState::Empty;
};

namespace hash_function {

constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;

// Fowler–Noll–Vo hash function - FNV1A type
std::size_t fnv1a(std::string_view key) {
    uint64_t hash = FNV_OFFSET;
    for (auto p : key) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(p));
        hash *= FNV_PRIME;
    }

    return static_cast<size_t>(hash);
}
} // namespace hash_function

template <typename T>
class HashTable {
public:
    HashTable()
    : length(0), capacity(INITIAL_CAPACITY) {
        entries.resize(INITIAL_CAPACITY);
    }

    std::optional<T> get(std::string_view key) {
        std::size_t hash = hash_function::fnv1a(key);
        std::size_t index = hash & (capacity - 1);
        auto start = index;

        // loop until we find an empty array
        while (entries[index].state != HashTableEntry<T>::SlotState::Empty) {
            if (entries[index].state == HashTableEntry<T>::SlotState::Dead) {
                index = (index+1) >= capacity ? 0 : index + 1;
                continue;
            }

            if (entries[index].key == key) {
                return entries[index].value;
            }

            index = (index+1) >= capacity ? 0 : index + 1;
            
            if (index == start) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    void set(std::string_view key, T value) {
        
        if (loadFactor() > 0.7f) {
            rehash();
        }

        set_entry(key, value);
    }

    void remove(std::string_view key) {
        
        std::size_t hash = hash_function::fnv1a(key);
        std::size_t index = hash & (capacity - 1);
        auto start = index;

        while (entries[index].state != HashTableEntry<T>::SlotState::Empty) {
            if (entries[index].state == HashTableEntry<T>::SlotState::Dead) {
                index = (index+1) >= capacity ? 0 : index + 1;
                continue;
            }

            if (entries[index].key == key) {
                entries[index].state = HashTableEntry<T>::SlotState::Dead;
                length--;
                return;
            }

            index = (index+1) >= capacity ? 0 : index + 1;
            
            if (index == start) {
                return;
            }
        }
    }

    ~HashTable() = default;

private:
    std::vector<HashTableEntry<T>> entries;
    std::size_t capacity;
    std::size_t length;

    void set_entry(std::string_view key, T value) {
        std::size_t hash = hash_function::fnv1a(key);
        std::size_t index = hash & (capacity - 1);
        auto start = index;
        std::size_t dead_index = SIZE_MAX;

        while (entries[index].state != HashTableEntry<T>::SlotState::Empty) {

            if (entries[index].state == HashTableEntry<T>::SlotState::Occupied && entries[index].key == key) {
                entries[index].value = value;
                return;
            }

            if (entries[index].state == HashTableEntry<T>::SlotState::Dead && dead_index == SIZE_MAX) {
                dead_index = index;
            }

            index = (index + 1) >= capacity ? 0 : index + 1;

            if (index == start) {
                break;
            }
        }

        std::size_t insert_at = dead_index != SIZE_MAX ? dead_index : index;

        if (entries[insert_at].state != HashTableEntry<T>::SlotState::Dead) {
            length++;
        }

        entries[insert_at].state = HashTableEntry<T>::SlotState::Occupied;
        entries[insert_at].key = key;
        entries[insert_at].value = value;
    }

    void rehash() {
        // First move old entries somewhere
        auto old_entries = std::move(entries);
        std::size_t new_capacity = 2 * capacity;
        capacity = new_capacity;
        length = 0;
        entries.resize(capacity);

        for (auto entry : old_entries) {
            if (entry.state == HashTableEntry<T>::SlotState::Occupied) {
                set_entry(entry.key, entry.value);
            }
        }
    }

    float loadFactor() const {
        return static_cast<float> (length) / capacity;
    }
};