//
//  BitStream.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_BitStream_h
#define SerializationFramework_BitStream_h

#include "Types.h"

// This is a reference counted, size-tracked and null-terminated, shared, immutable string implementation
// m_buffer is a pointer rather than an std::vector (for automatic memory management) because the shared
// buffer can probably be "pointed to" by multiple String instances - NO String instance actually "owns"
// the buffer, thus preventing usage of any ownership enforcing RAII scheme here - the buffer allocation
// and deallocation have to be explicit!
class String
{
public:
    String()
        : m_buffer(nullptr)
    {
    }

    String(const char* str, size_t len)
        : String()
    {
        if (str == nullptr || len == 0)
        {
            return;
        }

        m_buffer = AllocateBuffer(len);
        memcpy(&m_buffer[2], str, len);
    }

    String(const char* str)
        : String( str, strlen(str) )
    {
    }

    String(const String& rhs)
        : m_buffer(rhs.m_buffer)
    {
        if (m_buffer)
        {
            ++m_buffer[0];
        }
    }

    String(String&& rhs)
        : m_buffer(rhs.m_buffer)
    {
        rhs.m_buffer = nullptr;
    }

    void operator=(const char* str)
    {
        this->Release();
        new(this) String(str);
    }

    void operator=(const String& rhs)
    {
        this->Release();
        new(this) String(rhs);
    }

    void operator=(String&& rhs)
    {
        this->Release();
        new(this) String( std::move(rhs) );
    }

    bool operator==(const char* s) const
    {
        return std::strcmp( data(), s ) == 0;
    }

    bool operator==(const String& rhs) const
    {
        return m_buffer == rhs.m_buffer || strcmp( data(), rhs.data() ) == 0;
    }

    const char* data() const
    {
        return m_buffer ? (char*)&m_buffer[2] : "";
    }

    size_t size() const
    {
        return m_buffer ? m_buffer[1] : 0;
    }

    const char* c_str() const
    {
        return data();
    }

    bool empty() const
    {
        return m_buffer == nullptr;
    }

    ~String()
    {
        Release();
    }

protected:
    friend class BitStreamInput;
    inline bool BuildFrom(const class BitStreamInput&);

    void Release()
    {
        if (m_buffer && --m_buffer[0] == 0)
        {
            delete[] m_buffer;
        }

        m_buffer = nullptr;
    }

    static size_t* AllocateBuffer(size_t sl)
    {
        size_t n = 2 + (sl + 1 + sizeof(size_t)) / sizeof(size_t); // NB: we have to align at the boundary of size_t
        size_t* buffer = new size_t[n];
        buffer[0] = 1;
        buffer[1] = sl;
        char* p = (char*)&buffer[2];
        p[sl] = '\0';
        return buffer;
    }

private:
    size_t* m_buffer;
};

namespace std
{
    template <>
    struct hash<String>
    {
        size_t operator()(const String& s) const
        {
            static const size_t InitialFNV = 2166136261U;
            static const size_t FNVMultiple = 16777619;

            size_t hash = InitialFNV;

            const char* p = s.data();
            while (*p != '\0')
            {
                hash ^= *p++;
                hash *= FNVMultiple;
            }

            return hash;
        }
    };
}

#define BYTES2BITS(x) ( (x) << 3 )
#define BITS2BYTES(x) ( ((x) + 7) >> 3 )
#define BITS2BOUNDARY(x) ( ((x) + 7) & ~7 )

static inline bool IsBigEndian()
{
    static __U u = 0x80000000U;
    static bool r = *(U8*)&u == 0x80;
    return r;
}

static inline U8 ReverseByteOrder(U8 u8)
{
    return u8;
}

static inline U16 ReverseByteOrder(U16 u16)
{
    return u16 << 8 | u16 >> 8;
}

static inline U32 ReverseByteOrder(U32 u32)
{
    return u32 << 24 | (u32 & 0x0000ff00U) << 8 | (u32 & 0x00ff0000U) >> 8 | u32 >> 24;
}

static inline U64 ReverseByteOrder(U64 u64)
{
    return u64 << 56 | (u64 & 0x000000000000ff00LL) << 40 | (u64 & 0x0000000000ff0000LL) << 24 | (u64 & 0x00000000ff000000LL) << 8 |
        (u64 & 0x000000ff00000000LL) >> 8 | (u64 & 0x0000ff0000000000LL) >> 24 | (u64 & 0x00ff000000000000LL) >> 40 | u64 >> 56;
}

template <typename T>
struct Integral;

template <>
struct Integral<U8>
{
    static const size_t N_PREFIX_BITS = 3;

    static size_t GetNumEffectiveBits(U8 u)
    {
        static const size_t N[] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4}; // the lookup table for number of effective bits of 0 ~ 15

        size_t r = 0;
        if (u >> 4)
        {
            r += 4;
            u >>= 4;
        }
        return r + N[u];
    }
};

template <>
struct Integral<U16>
{
    static const size_t N_PREFIX_BITS = 4;

    static size_t GetNumEffectiveBits(U16 u)
    {
        size_t r = 0;
        if (u >> 8)
        {
            r += 8;
            u >>= 8;
        }
        return r + Integral<U8>::GetNumEffectiveBits((U8)u);
    }
};

template <>
struct Integral<U32>
{
    static const size_t N_PREFIX_BITS = 5;

    static size_t GetNumEffectiveBits(U32 u)
    {
        size_t r = 0;
        if (u >> 16)
        {
            r += 16;
            u >>= 16;
        }
        return r + Integral<U16>::GetNumEffectiveBits((U16)u);
    }
};

template <>
struct Integral<U64>
{
    static const size_t N_PREFIX_BITS = 6;

    static size_t GetNumEffectiveBits(U64 u)
    {
        size_t r = 0;
        if (u >> 32)
        {
            r += 32;
            u >>= 32;
        }
        return r + Integral<U32>::GetNumEffectiveBits((U32)u);
    }
};

template <typename I, typename = void>
class SignedIntegralConversion;

template <typename I>
class SignedIntegralConversion< I, typename std::enable_if< std::is_integral<I>::value && !std::is_unsigned<I>::value && !std::is_same<I, bool>::value >::type >
{
public:
    SignedIntegralConversion(size_t nbits)
        : mx( (I)(((typename std::make_unsigned<I>::type)1 << (nbits - 1)) - 1) )
        , mn(-mx - 1)
    {}

    typename std::make_unsigned<I>::type SignedToUnsigned(I i) const
    {
        I t = i < mn ? mn : i > mx ? mx : i;
        return (typename std::make_unsigned<I>::type)(t - mn); // 2's complement subtraction
    }

    I UnsignedToSigned(typename std::make_unsigned<I>::type u) const
    {
        I i = (I)((typename std::make_unsigned<I>::type)mn + u); // 2's complement addition
        return i < mn ? mn : i > mx ? mx : i;
    }

private:
    const I mx;
    const I mn;
};

// NB: combine a sign and a value (all 2's complement)
template < typename U, typename = typename std::enable_if< std::is_integral<U>::value && std::is_unsigned<U>::value && !std::is_same<U, bool>::value >::type >
U Combine(U s, U u)
{
    U mask = ~(s & 1) + 1;
    return (u ^ mask) - mask;
}

template < typename U, typename = typename std::enable_if< std::is_integral<U>::value && std::is_unsigned<U>::value && !std::is_same<U, bool>::value >::type >
U Sign(U u)
{
    return u >> (sizeof(U) * 8 - 1);
}

template < typename U, typename = typename std::enable_if< std::is_integral<U>::value && std::is_unsigned<U>::value && !std::is_same<U, bool>::value >::type >
U Absolute(U u)
{
    return Combine( Sign(u), u );
}

class BitStreamOutput
{
public:
    BitStreamOutput(Buffer& output)
        : m_output(output)
        , m_nbits(0)
    {
        //
    }

    size_t GetBitOffset() const
    {
        return m_nbits;
    }

    void Write(bool value)
    {
        WriteBit(value ? 0x01 : 0x00);
    }

    template < typename I, typename = typename std::enable_if< std::is_integral<I>::value && !std::is_same<I, bool>::value >::type >
    void Write(I i)
    {
        Writer<I>::Write(*this, i);
    }

    void Write(const String& s)
    {
        U32 sz = (U32)s.size();
        Write(sz);
        WriteBytesAligned( (const Byte*)s.data(), sz );
    }

    void Write(const Buffer& b)
    {
        U32 sz = (U32)b.size();
        Write(sz);
        WriteBytesAligned( (const Byte*)b.data(), sz );
    }

    template < typename I, typename = typename std::enable_if< std::is_integral<I>::value && !std::is_same<I, bool>::value >::type >
    void Write(I i, size_t nbits)
    {
        Writer<I>::Write(*this, i, nbits);
    }

protected:
    void WriteBytesAligned(const Byte buffer[], size_t nbytes)
    {
        if (nbytes == 0) return;

        size_t sz = m_output.size();
        m_output.resize(sz + nbytes);
        memcpy(&m_output[sz], buffer, nbytes);

        m_nbits = BITS2BOUNDARY(m_nbits);
        m_nbits += BYTES2BITS(nbytes);
    }

    void WriteBits(Byte buffer[], size_t nbits)
    {
        if (nbits == 0) return;

        m_output.resize( BITS2BYTES(m_nbits + nbits) );

        size_t lastByteIndex = BITS2BYTES(nbits) - 1;
        buffer[lastByteIndex] <<= (8 - nbits % 8) % 8;

        size_t rsh = m_nbits % 8;
        size_t lsh = 8 - rsh;

        Byte* p = buffer;
        Byte* e = buffer + BITS2BYTES(nbits);

        Byte data = *p++;
        Byte mask = 0xff << lsh;

        size_t writeIndex = m_nbits >> 3;
        size_t endIndex = BITS2BYTES(m_nbits + nbits) - 1;

        m_output[writeIndex] = (m_output[writeIndex] & mask) | (data >> rsh);

        while (++writeIndex <= endIndex)
        {
            Byte next = p < e ? *p++ : 0;
            m_output[writeIndex] = (data << lsh) | (next >> rsh);
            data = next;
        }

        m_nbits += nbits;
    }

    void WriteBit(Byte bit)
    {
        if (m_nbits % 8 == 0)
        {
            m_output.resize( m_output.size() + 1 );
        }

        m_output.back() |= (bit & 0x01) << (8 - m_nbits % 8 - 1);
        
        ++m_nbits;
    }
    
    template <typename T, typename = void>
    struct Writer;

    template <typename U>
    struct Writer< U, typename std::enable_if< std::is_integral<U>::value && std::is_unsigned<U>::value && !std::is_same<U, bool>::value >::type >
    {
        static void Write(BitStreamOutput& stream, U u)
        {
            // NB: the number of effective bits ranges from 1 to sizeof(U) * 8; to represent it with N_PREFIX_BITS, the range has to be adjusted to 0 ~ sizeof(U) * 8 - 1!
            U8 nbits = u == 0 ? (U8)0 : (U8)Integral<U>::GetNumEffectiveBits(u) - 1; // NB: it costs 1 bit to write '0'!
            size_t n = nbits + 1;
            stream.WriteBits( (Byte*)&nbits, Integral<U>::N_PREFIX_BITS );

            Write(stream, u, n);
        }

        static void Write(BitStreamOutput& stream, U u, size_t nbits)
        {
            if ( nbits > sizeof(U) * 8 )
            {
                nbits = sizeof(U) * 8;
            }

            if ( IsBigEndian() )
            {
                u = ReverseByteOrder(u);
            }

            stream.WriteBits( (Byte*)&u, nbits );
        }
    };

    template <typename I>
    struct Writer< I, typename std::enable_if< std::is_integral<I>::value && !std::is_unsigned<I>::value && !std::is_same<I, bool>::value >::type >
    {
        static void Write(BitStreamOutput& stream, I i)
        {
            typename std::make_unsigned<I>::type u = i;
            stream.WriteBit( (Byte)Sign(u) );
            Writer< typename std::make_unsigned<I>::type >::Write( stream, Absolute(u) );
        }

        static void Write(BitStreamOutput& stream, I i, size_t nbits)
        {
            typename std::make_unsigned<I>::type u = SignedIntegralConversion<I>(nbits).SignedToUnsigned(i);

            Writer< typename std::make_unsigned<I>::type >::Write(stream, u, nbits);
        }
    };

    // NB: Non-integral types are supported through data policies

private:
    Buffer& m_output;
    size_t m_nbits;
};

class BitStreamInput
{
public:
    BitStreamInput(const Buffer& input)
        : m_input(input)
        , m_nbits(0)
    {
        //
    }

    size_t GetBitOffset() const
    {
        return m_nbits;
    }

    bool Read(bool& value) const
    {
        Byte bit = 0x00;
        bool r = ReadBit(bit);
        value = bit == 0x01;
        return r;
    }

    template < typename I, typename = typename std::enable_if< std::is_integral<I>::value && !std::is_same<I, bool>::value >::type >
    bool Read(I& i) const
    {
        return Reader<I>::Read(*this, i);
    }

    bool Read(String& s) const
    {
        return s.BuildFrom(*this);
    }

    bool Read(Buffer& b) const
    {
        U32 sz = 0;
        if ( Read(sz) )
        {
            b.resize(sz);
            return ReadBytesAligned( (Byte*)b.data(), sz );
        }
        return false;
    }

    template < typename I, typename = typename std::enable_if< std::is_integral<I>::value && !std::is_same<I, bool>::value >::type >
    bool Read(I& i, size_t nbits) const
    {
        return Reader<I>::Read(*this, i, nbits);
    }

protected:
    friend class String;
    friend class ScopedBitStreamInputOffset;

    void SetBitOffset(size_t offset) const
    {
        m_nbits = offset; // XXX: very dangerous operation, for internal use ONLY!
    }

    bool ReadBytesAligned(Byte buffer[], size_t nbytes) const
    {
        if (nbytes == 0) return true;

        size_t byteIndex = BITS2BYTES(m_nbits);

        if ( byteIndex + nbytes > m_input.size() )
        {
            return false;
        }

        memcpy(buffer, &m_input[byteIndex], nbytes);

        m_nbits = BITS2BOUNDARY(m_nbits);
        m_nbits += BYTES2BITS(nbytes);

        return true;
    }

    bool ReadBits(Byte buffer[], size_t nbits) const
    {
        if (nbits == 0) return true;

        if ( m_nbits + nbits > m_input.size() * 8 )
        {
            return false;
        }

        size_t lsh = m_nbits % 8;
        size_t rsh = 8 - lsh;

        size_t readIndex = m_nbits >> 3;
        size_t endIndex = BITS2BYTES(m_nbits + nbits) - 1;

        Byte* p = buffer;
        Byte* e = buffer + BITS2BYTES(nbits);

        Byte data = m_input[readIndex];

        while (p < e)
        {
            Byte next = ++readIndex <= endIndex ? m_input[readIndex] : 0;
            *p++ = (data << lsh) | (next >> rsh);
            data = next;
        }

        size_t lastByteIndex = BITS2BYTES(nbits) - 1;
        buffer[lastByteIndex] >>= (8 - nbits % 8) % 8;

        m_nbits += nbits;

        return true;
    }

    bool ReadBit(Byte& bit) const
    {
        if ( m_nbits + 1 > m_input.size() * 8 )
        {
            return false;
        }

        size_t readIndex = m_nbits >> 3;
        bit = (m_input[readIndex] >> (8 - m_nbits % 8 - 1)) & 0x01;
        
        ++m_nbits;
        
        return true;
    }
    
    template <typename T, typename = void>
    struct Reader;

    template <typename U>
    struct Reader< U, typename std::enable_if< std::is_integral<U>::value && std::is_unsigned<U>::value && !std::is_same<U, bool>::value >::type >
    {
        static bool Read(const BitStreamInput& stream, U& u)
        {
            U8 nbits = 0;
            if ( stream.ReadBits( (Byte*)&nbits, Integral<U>::N_PREFIX_BITS ) )
            {
                return Read(stream, u, (size_t)nbits + 1);
            }

            return false;
        }

        static bool Read(const BitStreamInput& stream, U& u, size_t nbits)
        {
            if ( nbits > sizeof(U) * 8 )
            {
                nbits = sizeof(U) * 8;
            }

            if ( stream.ReadBits( (Byte*)&u, nbits ) )
            {
                if ( IsBigEndian() )
                {
                    u = ReverseByteOrder(u);
                }

                return true;
            }

            return false;
        }
    };

    template <typename I>
    struct Reader< I, typename std::enable_if< std::is_integral<I>::value && !std::is_unsigned<I>::value && !std::is_same<I, bool>::value >::type >
    {
        static bool Read(const BitStreamInput& stream, I& i)
        {
            Byte sign = 0;
            if ( stream.ReadBit(sign) )
            {
                typename std::make_unsigned<I>::type u = 0;
                if ( Reader< typename std::make_unsigned<I>::type >::Read(stream, u) )
                {
                    typename std::make_unsigned<I>::type s = sign;
                    i = Combine(s, u);
                    return true;
                }
            }

            return false;
        }

        static bool Read(const BitStreamInput& stream, I& i, size_t nbits)
        {
            typename std::make_unsigned<I>::type u = 0;

            if ( Reader< typename std::make_unsigned<I>::type >::Read(stream, u, nbits) )
            {
                i = SignedIntegralConversion<I>(nbits).UnsignedToSigned(u);
                return true;
            }

            return false;
        }
    };

    // NB: Non-integral types are supported through data policies

private:
    const Buffer& m_input;
    mutable size_t m_nbits;
};

class ScopedBitStreamInputOffset
{
public:
    ScopedBitStreamInputOffset(const BitStreamInput& input, size_t offset)
        : m_input(input)
        , m_offset( input.GetBitOffset() )
    {
        m_input.SetBitOffset(offset);
    }

    ~ScopedBitStreamInputOffset()
    {
        m_input.SetBitOffset(m_offset);
    }

private:
    const BitStreamInput& m_input;
    size_t m_offset;
};

inline bool String::BuildFrom(const BitStreamInput& stream)
{
    U32 sl = 0;
    if ( stream.Read(sl) )
    {
        if (sl == 0)
        {
            this->Release();
            return true;
        }
        size_t* buffer = AllocateBuffer(sl);
        if ( stream.ReadBytesAligned( (Byte*)&buffer[2], sl ) )
        {
            this->Release();
            m_buffer = buffer;
            return true;
        }
        delete[] buffer;
    }
    return false;
}

#endif
