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
















