# Bitcoin Core `streams.h` — Complete Deep Dive

This file defines Bitcoin Core's **serialization stream infrastructure**
the classes that converts typed C++ objects to/from raw bytes for storage, network transmission, and file I/O.

---

## General Conventions

| Operator / Method | Direction | Meaning |
|---|---|---|
| `operator<<` | IN | serialize (C++ object → bytes) |
| `operator>>` | OUT | deserialize (bytes → C++ object) |
| `write()` | IN | raw bytes append |
| `read()` | OUT | raw bytes consume |

Different classes = different **backing stores**:

```
VectorWriter   →  writes into std::vector<unsigned char>
SpanReader     →  reads from a fixed std::span (read-only view) exception free
SpanWriter     →  writes into a fixed std::span (bounded buffer) exception free
DataStream     →  dual-ended buffer (read + write, owns memory)
AutoFile       →  reads/writes a FILE* on disk
BufferedFile   →  FILE* with rewind capability
BufferedReader →  buffered wrapper around any stream
BufferedWriter →  buffered wrapper around any stream
```

---

## 1. `VectorWriter`

```cpp
class VectorWriter {
    std::vector<unsigned char>& vchData;  // reference — doesn't OWN the vector
    size_t nPos;                          // current write position
};
```

### What It Is

A **write-only stream that targets an existing vector**. It doesn't own the vector, it's given a reference and writes into it.

### Ctor — Position Only

```cpp
VectorWriter(std::vector<unsigned char>& vchDataIn, size_t nPosIn)
    : vchData{vchDataIn}, nPos{nPosIn}
{
    if(nPos > vchData.size())
        vchData.resize(nPos);   // grow vector if needed to reach start position
}
```

`nPos` is flexible:

```
nPos = 0              → overwrite from the beginning
nPos = vec.size()     → append to the end
nPos = 5              → overwrite starting at index 5
nPos > vec.size()     → resize first, then write (gap filled with zeros)
```

### Ctor 2 — Position + Variadic Serialize

```cpp
template <typename... Args>
VectorWriter(std::vector<unsigned char>& vchDataIn, size_t nPosIn, Args&&... args)
    : VectorWriter{vchDataIn, nPosIn}                          // delegate to ctor 1
{
    ::SerializeMany(*this, std::forward<Args>(args)...);       // serialize all args immediately
}
```

This is a **convenience ctor** — create the writer AND serialize objects in one shot:

```cpp
// Without convenience constructor:
std::vector<unsigned char> vec;
VectorWriter w(vec, 0);
w << key;
w << value;
w << timestamp;

// With convenience constructor:
std::vector<unsigned char> vec;
VectorWriter w(vec, 0, key, value, timestamp);  // all in one line
```

`std::forward<Args>` is **perfect forwarding** — preserves whether each argument is an lvalue or rvalue, avoiding unnecessary copies.

### `write()` — The Core Method

```cpp
void write(std::span<const std::byte> src)
{
    assert(nPos <= vchData.size());
    size_t nOverwrite = std::min(src.size(), vchData.size() - nPos);
    if (nOverwrite) {
        memcpy(vchData.data() + nPos, src.data(), nOverwrite);  // overwrite existing
    }
    if (nOverwrite < src.size()) {
        vchData.insert(vchData.end(), ...);  // append what doesn't fit
    }
    nPos += src.size();
}
```

Two-phase write:

```
Vector: [A][B][C][D][E]
nPos = 3, src = [X][Y][Z][W]

Phase 1 — overwrite existing bytes:
[A][B][C][X][Y]   (nOverwrite = 2, overwrites D and E)

Phase 2 — append bytes that don't fit:
[A][B][C][X][Y][Z][W]   (appends Z and W)

nPos advances to 7
```

### `operator<<`

```cpp
template <typename T>
VectorWriter& operator<<(const T& obj)
{
    ::Serialize(*this, obj);   // calls Bitcoin Core's serialization framework
    return (*this);            // returns *this for chaining: w << a << b << c
}
```

`::Serialize` is a free function template defined in `serialize.h`. It knows how to serialize every Bitcoin type — integers, strings, vectors, transaction objects, etc. — by calling `write()` on the stream.

---

## 2. `SpanReader`

```cpp
class SpanReader {
    std::span<const std::byte> m_data;  // view into existing memory — owns NOTHING
};
```

### What It Is

A **read-only, non-owning cursor over a byte span**. It advances through the span as you read, with no ability to go back.

### Two Constructors

```cpp
explicit SpanReader(std::span<const unsigned char> data) : m_data{std::as_bytes(data)} {}
explicit SpanReader(std::span<const std::byte> data)     : m_data{data} {}
```

Accepts both `unsigned char` spans and `std::byte` spans — `std::as_bytes` reinterprets the former as the latter. Same memory, different type label.

### `read()` — Consuming Bytes

```cpp
void read(std::span<std::byte> dst)
{
    if (dst.size() > m_data.size()) {
        throw std::ios_base::failure("SpanReader::read(): end of data");
    }
    memcpy(dst.data(), m_data.data(), dst.size());
    m_data = m_data.subspan(dst.size());   // ← advance the cursor
}
```

The key is `subspan` — it **shrinks the view** from the front:

```
Before read(3 bytes):
m_data: [A][B][C][D][E][F][G]
         ↑ start

After read(3 bytes):
dst gets: [A][B][C]
m_data: [D][E][F][G]
         ↑ start moved forward
```

The original memory is untouched — `subspan` just moves the start pointer and reduces the size. **Zero copy, zero allocation.**

### `ignore()`

```cpp
void ignore(size_t n) {
    if (n > m_data.size()) throw ...;
    m_data = m_data.subspan(n);   // just advance cursor, no copy
}
```

Skip bytes without reading them — used when deserializing formats with padding or fields you don't need.

### Why `SpanReader` vs `DataStream`?

| | SpanReader | DataStream |
|---|---|---|
| Allocation | zero | owns heap memory |
| Copy | zero (just a view) | copies data in |
| Use case | reading existing buffers | building up data |
| Direction | read-only | read + write |
| Rewind | no | more flexible |

`SpanReader` is used in `CDBIterator::GetKey()` — the LevelDB key bytes are already in memory, so there's no need to copy them into a new buffer just to deserialize.

---

## 3. `SpanWriter`

```cpp
class SpanWriter {
    std::span<std::byte> m_dest;   // view into a FIXED pre-allocated buffer
};
```

### What It Is

A **write-only stream into a fixed-size pre-allocated buffer**. The buffer must already exist and be large enough — `SpanWriter` doesn't grow it.

### `write()`

```cpp
void write(std::span<const std::byte> src)
{
    if (src.size() > m_dest.size()) {
        throw std::ios_base::failure("SpanWriter::write(): exceeded buffer size");
    }
    memcpy(m_dest.data(), src.data(), src.size());
    m_dest = m_dest.subspan(src.size());   // advance write cursor
}
```

Same `subspan` trick as `SpanReader` but for writing. The cursor advances as you write, preventing overwrites.

### When Is This Used?

When you know the **exact output size upfront** and want to write into a fixed buffer without heap allocation:

```cpp
std::array<std::byte, 32> buf;
SpanWriter w{buf};
w << txid;    // writes exactly 32 bytes into buf
              // no heap allocation at all
```

---

## 4. `DataStream` — The Core Class

```cpp
class DataStream {
protected:
    using vector_type = SerializeData;        // std::vector<std::byte> with zeroing allocator
    vector_type vch;                          // the actual buffer (owned)
    vector_type::size_type m_read_pos{0};     // read cursor position
};
```

### What It Is

A **dual-ended buffer** — you write to the end and read from a tracked position. The most versatile stream class in Bitcoin Core.

```
vch:  [already read][unread data    ]
       0..m_read_pos  m_read_pos..end
                      ↑
                      read cursor
```

### Why `SerializeData` Instead of `vector<unsigned char>`?

`SerializeData` uses `zero_after_free_allocator` — when the vector is deallocated, it **zeroes the memory first**. This prevents sensitive data (private keys, seeds) from lingering in freed heap memory where it could be read by another process or a memory dump.

### The Read/Write Split

```cpp
// Writing always appends to END:
void write(std::span<const value_type> src) {
    vch.insert(vch.end(), src.begin(), src.end());
}

// Reading always reads from m_read_pos:
void read(std::span<value_type> dst) {
    memcpy(dst.data(), &vch[m_read_pos], dst.size());
    m_read_pos += dst.size();
    // if fully consumed → clear()
}
```

```
Initial state:
vch:        []
m_read_pos:  0

After write([A][B][C]):
vch:        [A][B][C]
m_read_pos:  0

After write([D][E]):
vch:        [A][B][C][D][E]
m_read_pos:  0

After read(2 bytes) → returns [A][B]:
vch:        [A][B][C][D][E]
m_read_pos:        2   ↑ cursor here

After read(3 bytes) → returns [C][D][E]:
vch:        []     ← fully consumed → clear() called
m_read_pos:  0
```

### Why `size()` Subtracts `m_read_pos`

```cpp
size_type size() const { return vch.size() - m_read_pos; }
```

`size()` returns the **number of unread bytes**, not total allocated bytes:

```
vch:        [A][B][C][D][E]
m_read_pos:           2

vch.size()    = 5
m_read_pos    = 2
size()        = 3  ← "3 bytes left to read"
```

Same logic applies to `begin()`, `data()`, `operator[]` — they all offset by `m_read_pos` to present only the unread portion.

### `ignore()` — Safe Skip with Overflow Protection

```cpp
void ignore(size_t num_ignore) {
    auto next_read_pos{CheckedAdd(m_read_pos, num_ignore)};
    if (!next_read_pos.has_value() || next_read_pos.value() > vch.size()) {
        throw std::ios_base::failure("DataStream::ignore(): end of data");
    }
    m_read_pos = next_read_pos.value();
}
```

`CheckedAdd` is Bitcoin Core's overflow-safe addition — without it, a maliciously crafted `num_ignore` could overflow `m_read_pos` and wrap around to 0, bypassing the bounds check. This is **integer overflow protection**.

---

## 5. `ScopedDataStreamUsage`

```cpp
class ScopedDataStreamUsage {
    DataStream& m_stream;
public:
    explicit ScopedDataStreamUsage(DataStream& stream) : m_stream{stream} {
        assert(m_stream.empty());   // must be empty on entry
    }
    ~ScopedDataStreamUsage() { m_stream.clear(); }  // always clean up on exit
};
```

### What It Is

A **RAII guard for scratch `DataStream` buffers**. Ensures the stream is empty when you borrow it, and clears it when you're done — regardless of exceptions.

### The Scratch Buffer Pattern

Without `ScopedDataStreamUsage`:

```cpp
// WRONG — what if Seek throws?
m_scratch << key;
SeekImpl(m_scratch);
m_scratch.clear();        // never reached if exception thrown!
// next call: m_scratch still has old data → assert fires
```

With `ScopedDataStreamUsage`:

```cpp
// CORRECT — clear() is guaranteed via RAII
{
    ScopedDataStreamUsage guard{m_scratch};  // assert: empty on entry
    m_scratch << key;
    SeekImpl(m_scratch);
}  // ← ~ScopedDataStreamUsage() calls m_scratch.clear() here
   //   EVEN IF SeekImpl threw an exception
```

### Why `assert(m_stream.empty())`?

If the stream **isn't empty on entry**, it means a previous usage didn't clean up — a programming error. The assert catches this in debug builds immediately, rather than letting stale data silently corrupt the next operation.

---

## 6. `BitStreamReader` / `BitStreamWriter`

```cpp
template <typename IStream>
class BitStreamReader {
    IStream& m_istream;
    uint8_t m_buffer{0};    // currently buffered byte
    int m_offset{8};        // how many bits of buffer already consumed (8 = empty)
};
```

### What They Are

**Bit-level I/O** over any byte stream. Used for Golomb-Rice coding in Bitcoin's **compact block filters** (BIP 158).

Normal streams read/write in bytes (8 bits). These read/write in **arbitrary bit counts**.

### `BitStreamReader::Read(int nbits)`

```cpp
uint64_t Read(int nbits) {
    uint64_t data = 0;
    while (nbits > 0) {
        if (m_offset == 8) {        // buffer exhausted — load next byte
            m_istream >> m_buffer;
            m_offset = 0;
        }
        int bits = std::min(8 - m_offset, nbits);   // bits available in buffer
        data <<= bits;
        data |= static_cast<uint8_t>(m_buffer << m_offset) >> (8 - bits);
        m_offset += bits;
        nbits -= bits;
    }
    return data;
}
```

Example — reading 3 bits then 5 bits from byte `0b10110100`:

```
Byte: 1 0 1 1 0 1 0 0
      ↑ m_offset = 0

Read(3):
  bits = min(8-0, 3) = 3
  extract top 3 bits: 1 0 1
  m_offset = 3, returns 0b101 = 5

Read(5):
  bits = min(8-3, 5) = 5
  extract next 5 bits: 1 0 1 0 0
  m_offset = 8, returns 0b10100 = 20
```

### `BitStreamWriter::Flush()`

```cpp
~BitStreamWriter() { Flush(); }  // auto-flush in destructor

void Flush() {
    if (m_offset == 0) return;
    m_ostream << m_buffer;   // write partial byte padded with zeros
    m_buffer = 0;
    m_offset = 0;
}
```

Critical: the destructor calls `Flush()` — if you write 13 bits total (1 full byte + 5 bits), the second partial byte is automatically flushed with 3 zero-padding bits when the writer is destroyed.

---

## 7. `AutoFile`

```cpp
class AutoFile {
protected:
    std::FILE*              m_file;           // the OS file handle
    Obfuscation             m_obfuscation;    // optional XOR obfuscation
    std::optional<int64_t>  m_position;       // tracked file position
    bool                    m_was_written{false}; // tracks if writes happened
};
```

### What It Is

An **RAII wrapper around `FILE*`** with built-in obfuscation support. Automatically closes the file and handles the tricky error semantics of `fclose()` after writes.

### The Tricky Destructor

```cpp
~AutoFile() {
    if (m_was_written) {
        Assume(IsNull());   // MUST have been explicitly closed if written to
    }
    if (fclose() != 0) {
        LogError("Failed to close file: %s", SysErrorString(errno));
    }
}
```

Why this complexity? Because **`fclose()` after a write can fail** — it flushes OS buffers to disk, and that flush can fail (disk full, hardware error). A destructor can't throw or return an error, so:

```
If you wrote to the file:
  → You MUST call fclose() explicitly and CHECK its return value
  → The destructor verifies you did this via Assume(IsNull())
  → If you forgot → Assume fires → crash in debug, logged in release

If you only read:
  → destructor fclose() is fine — read failures already handled
```

This is a **deliberate design pattern** to prevent silent data corruption.

### `detail_fread()` — The Core Read

```cpp
std::size_t AutoFile::detail_fread(std::span<std::byte> dst)
{
    const size_t ret = std::fread(dst.data(), 1, dst.size(), m_file);
    if (m_obfuscation) {
        if (!m_position) throw ...;
        m_obfuscation(dst.subspan(0, ret), *m_position);  // XOR in-place
    }
    if (m_position) *m_position += ret;
    return ret;
}
```

It's called `detail_` — an **implementation detail** that returns actual bytes read (possibly less than requested, like raw `fread`). The public `read()` wraps it and throws if not all bytes were read.

Obfuscation is applied **immediately after reading**, using the current file position as the XOR offset — the same key byte is always applied to the same file position regardless of how many bytes you read at a time.

### `write()` vs `write_buffer()`

```cpp
// write() — const input, can't obfuscate in-place:
void AutoFile::write(std::span<const std::byte> src)
{
    if (!m_obfuscation) {
        std::fwrite(src.data(), ...);   // direct write
    } else {
        // must copy into mutable buffer first, then obfuscate
        std::array<std::byte, 4096> buf;
        // copy chunks into buf, call write_buffer on each
    }
}

// write_buffer() — mutable input, obfuscates IN-PLACE:
void AutoFile::write_buffer(std::span<std::byte> src)
{
    if (m_obfuscation) {
        m_obfuscation(src, *m_position);  // XOR src in-place
    }
    std::fwrite(src.data(), ...);         // write obfuscated bytes
}
```

The split exists because:

```
write()        takes const span   → can't modify input → must copy if obfuscating
write_buffer() takes mutable span → modifies in-place  → more efficient
               used by BufferedWriter for large sequential writes
```

---

## 8. `BufferedFile`

```cpp
class BufferedFile {
    AutoFile& m_src;          // underlying file — not owned
    uint64_t nSrcPos{0};      // how far the file has been read
    uint64_t m_read_pos{0};   // how far the CALLER has consumed
    uint64_t nReadLimit;      // caller can't read past this
    uint64_t nRewind;         // guaranteed rewind distance
    DataBuffer vchBuf;        // ring buffer
};
```

### What It Is

A **ring buffer over a file** that guarantees the ability to rewind by `nRewind` bytes. Used for Bitcoin block deserialization where you sometimes need to backtrack.

### The Ring Buffer

```
vchBuf is a circular buffer:
position in file % vchBuf.size() = index in vchBuf

nSrcPos    = 1000  (file read up to here)
m_read_pos =  900  (caller consumed up to here)
nRewind    =  200  (guarantee 200 bytes of rewind)

Can rewind to:      1000 - 200 = 800 (at minimum)
Available to read:  1000 - 900 = 100 bytes buffered
```

### Why Not Just `fseek()`?

`fseek` on compressed or network streams isn't always possible. The ring buffer approach works on **any sequential stream** including those that don't support seeking.

### `SetPos()` — Rewind

```cpp
bool SetPos(uint64_t nPos) {
    if (nPos + bufsize < nSrcPos) {
        // rewinding too far — buffer has been overwritten
        m_read_pos = nSrcPos - bufsize;
        return false;   // partial rewind
    }
    m_read_pos = nPos;
    return true;
}
```

Returns `false` if you try to rewind further than the buffer allows — caller knows rewind failed and can handle it.

---

## 9. `BufferedReader` / `BufferedWriter`

These are **generic buffering wrappers** over any stream type.

### `BufferedReader`

```cpp
template <typename S>
class BufferedReader {
    S& m_src;
    DataBuffer m_buf;
    size_t m_buf_pos;
public:
    explicit BufferedReader(S&& stream LIFETIMEBOUND, size_t size = 1 << 16)
        requires std::is_rvalue_reference_v<S&&>   // ← requires rvalue ref
```

The `requires std::is_rvalue_reference_v<S&&>` constraint **requires the stream to be passed as an rvalue**:

```cpp
AutoFile file(...);

// won't compile — lvalue:
BufferedReader r(file);

// compiles — rvalue:
BufferedReader r(std::move(file));
```

Why? To prevent the caller from accidentally using the underlying stream directly after buffered reads — the buffer and the stream would be out of sync. Forcing `std::move` signals intent and discourages direct use after wrapping.

Default buffer size is `1 << 16 = 65536 bytes = 64KB` — chosen to match typical OS I/O block sizes for efficiency.

### `BufferedWriter`

```cpp
template <typename S>
class BufferedWriter {
    S& m_dst;
    DataBuffer m_buf;
    size_t m_buf_pos{0};
public:
    ~BufferedWriter() { flush(); }  // auto-flush on destruction
```

Accumulates writes in a 64KB buffer and flushes to the underlying stream either when full or on destruction. Dramatically reduces the number of `fwrite` syscalls:

```
Without buffering (1000 small writes):
  write(3 bytes) → fwrite → syscall
  write(3 bytes) → fwrite → syscall
  ... × 1000 = 1000 syscalls

With BufferedWriter (1000 small writes):
  write(3 bytes) → buf[0..2]
  write(3 bytes) → buf[3..5]
  ... fills 64KB buffer ...
  flush() → fwrite → syscall   ← 1 syscall for ~21000 writes
```

---

## The Full Hierarchy

```
All streams implement some subset of:
  write(span)     ← bytes in
  read(span)      ← bytes out
  operator<<      ← typed object in  (calls Serialize  → write)
  operator>>      ← typed object out (calls Unserialize → read)
```

| Class | Backing Store | Owns Memory | Read | Write | Purpose |
|---|---|---|---|---|---|
| `VectorWriter` | `vector&` ref | ❌ | ❌ | ✅ | serialize into existing vector |
| `SpanReader` | `span` view | ❌ | ✅ | ❌ | deserialize from byte span |
| `SpanWriter` | `span` view | ❌ | ❌ | ✅ | serialize into fixed buffer |
| `DataStream` | owned vector | ✅ | ✅ | ✅ | general purpose buffer |
| `AutoFile` | `FILE*` | ❌ | ✅ | ✅ | file I/O with obfuscation |
| `BufferedFile` | `AutoFile&` | ❌ | ✅ | ❌ | file I/O with rewind |
| `BufferedReader` | any stream | ❌ | ✅ | ❌ | read-buffering layer |
| `BufferedWriter` | any stream | ❌ | ❌ | ✅ | write-buffering layer |
| `BitStreamReader` | any stream | ❌ | ✅ | ❌ | bit-level reading |
| `BitStreamWriter` | any stream | ❌ | ❌ | ✅ | bit-level writing |

---

> The entire system is built around one principle: **every stream class implements the same `read()`/`write()`/`<<`/`>>` interface**, so `::Serialize` and `::Unserialize` work identically whether you're writing to memory, a file, or a network buffer — the serialization logic never needs to know where its bytes are going.
