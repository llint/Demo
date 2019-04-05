//
//  UniformQuantization.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_UniformQuantization_h
#define SerializationFramework_UniformQuantization_h

#include "BitStream.h"

template <typename T, typename Q, typename = void, typename = void>
class UniformQuantization;

template <typename T, typename Q>
class UniformQuantization< T, Q,
    typename std::enable_if< std::is_floating_point<T>::value || (std::is_integral<T>::value && !std::is_same<T, bool>::value) >::type,
    typename std::enable_if< std::is_integral<Q>::value && std::is_unsigned<Q>::value && !std::is_same<Q, bool>::value >::type >
{
public:
    UniformQuantization(T mn, T mx, size_t nbits = sizeof(Q) * 8)
        : m_mn(mn)
        , m_mx(mx)
        , m_nbits(nbits)
        , m_qmx( ((Q)-1 >> (sizeof(Q) * 8 - m_nbits)) )
    {}

    bool Read(const BitStreamInput& stream, T& v, const String& tag) const
    {
        Q quantized = 0;
        if ( stream.Read(quantized, m_nbits) )
        {
            v = m_mn + (T)( (double)quantized / m_qmx * (m_mx - m_mn) );
            return true;
        }

        return false;
    }

    void Write(BitStreamOutput& stream, T v, const String& tag) const
    {
        if( v < m_mn)
        {
            v = m_mn;
        }
        else if( v > m_mx )
        {
            v = m_mx;
        }

        Q quantized = (Q)( (double)(v - m_mn) / (m_mx - m_mn) * m_qmx );
        stream.Write(quantized, m_nbits);
    }
    
private:
    const T m_mn;
    const T m_mx;
    const size_t m_nbits;
    const Q m_qmx;
};

#endif
