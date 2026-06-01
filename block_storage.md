# Here's a brief notes on Block Storage in Bitcoin Core till commit [1d69ac33925888120aa1e69e2c1537e11b1d8390](https://github.com/2140-dev/bitcoin/commit/1d69ac33925888120aa1e69e2c1537e11b1d8390)

Let's first understand the difference between the raw data (the actual block & undo data) and the metadata (the index used to find a particular block data)

In Bitcoin Core:
  - **Raw Block Data** (blk*.dat) and Undo Data (rev*.dat) are stored in Flat Files.
  - **Metadata/Index** are basically pointers to where the blocks are in those files which is stored in LevelDB


## CDBWrapper (in dbwrapper.h/cpp)
- This class is a generic wrapper around LevelDB.
- `DBWrapper_PREALLOC_KEY_SIZE / VALUE_SIZE` constants prevents excessive memory allocation during database reads/writes, Bitcoin Core pre-allocates buffer space
   for serialization. Keys get 64 bytes, values get 1024 bytes.
- `DBWRAPPER_MAX_FILE_SIZE{32_MiB}` caps individual `.ldb` size to 32 bytes but this doesn't mean the individual `.ldb` size will be 32, more precisely it will be
    around 33 MiB.
- `DBOptions & DBParams` are the initialization parameters before opening the database.
- `dbwrapper_error`: a class that inherits `std::runtime_error` throws exception whenever a fatal leveldb error occurs (e.g. disk corruption or failure to read).

### Class `CDBBatch`
If you are aware of LevelDB database then you might know that LevelDB allows only batch operation or basically Atomic operation (means you eighter do full operations or nothing)
which means if Bitcoin Core crashes, either all changes are written or none are.
- `WriteBatchImpl`: A forward declaration (PIMPL idiom) to hide the actual leveldb::WriteBatch type from exposing which reduces compilation time.
- `m_key_scratch / m_value_scratch`: `DataStream` objects used as temporary memory buffers to serialize data into bytes.
- `Write(const K& key, const V& value)`: takes K (key) & V (value) object, serializes to bytes and queues them for writing via `WriteImpl` method.
- `Erase(const K& key)`: Queues a K (key) for deletion.
- `Clear()` : Empties the pending batch

A const ref to `CDBWrapper` class (`const CDBWrapper &parent;`)  which means that it cannot be null and cannot be restated, you might be thinking
`CDBBatchWrite` class is for writing right? Correct. But writing is done by `WriteBatchImpl` struct not by `parent` ref which is just for reading obfuscation key:
```
struct WriteBatchImpl;
const std::unique_ptr<WriteBatchImpl> m_impl_batch;
```

WriteImpl struct:
```
struct CDBBatch::WriteBatchImpl {
    leveldb::WriteBatch batch;
};
```

Why `friend class cdbwrapper;` then? Because `CDBWrapper::WriteBatch()` needs to reach inside `CDBBatch` to grab the private `m_impl_batch` buffer and hand it to LevelDB object.

### Class CDBIterator
This custom class is used for scanning the leveldb database, it uses keys to iterate. LevelDB stores the keys in sorted order.
- `IteratorImpl`: PIMPL struct hiding the leveldb::Iterator
- `Seek(const K& key)` / `SeekToFirst()`: Moves the iterator to the specific key or very start.
- `Next()`: Moves to the next key.
- `Valid()`: Checks if the iterator has fallen off the edge of the databse.

```
void CDBIterator::SeekImpl(std::span<const std::byte> key)
{
    leveldb::Slice slKey(CharCast(key.data()), key.size());
    m_impl_iter->iter->Seek(slKey);
}
```
`CharCast` is a method which casts (using `reinterpret_cast`) from byte pointer to const char pointer:

`static auto CharCast(const std::byte* data) { return reinterpret_cast<const char*>(data); }`

Because `leveldb::Slice` type is a simple structure that contains a length and a pointer to an external byte array and is returned value when we call `Seek` to level db database.


### Class `LevelDBContext`
This class sets the contexts for LevelDB database such as WriteOptions, ReadOptions, Envs, etc.

## Class CDBWrapper
This is the main class representing an open LevelDB database.
- `m_db_context`: Holds all actual LevelDB pointers (like leveldb::DB*). Hidden via PIMPL to keep headers clean.
- `m_obfuscation`: The object handles the XOR encryption logic.
- `Read()`, `Write()`, `Exists()`, `Erase()`: High-level template methods for single operations. Write and Erase actually just create a CDBBatch of size 1 and submit it.
- `WriteBatch()`: Takes a `CDBBatch` and commits it to disk. `fSync=true` forces an immediate OS flush (fsync) to disk.


```
size_t CDBWrapper::DynamicMemoryUsage() const
{
    std::string memory;
    std::optional<size_t> parsed;
    if (!DBContext().pdb->GetProperty("leveldb.approximate-memory-usage", &memory) || !(parsed = ToIntegral<size_t>(memory))) {
        LogDebug(BCLog::LEVELDB, "Failed to get approximate-memory-usage property\n");
        return 0;
    }
    return parsed.value();
}
```
We retrieve the dynamic memory usage using `GetProperty` member method of class `DB` in leveldb, this is not documented in leveldb [docs] readme, but can be seen in the source
file [here](https://github.com/google/leveldb/blob/7ee830d02b623e8ffe0b95d59a74db1e58da04c5/include/leveldb/db.h#L114).











