//
//  DP_UniqueString.cpp
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#include "Serialization.h"

class UniqueStringPolicy : public IDataPolicy<String>
{
public:
    UniqueStringPolicy(const IMetadataProcessor::Elements& elements)
    {
    }

    virtual bool Read(const BitStreamInput& stream, String& s, const String& tag) override
    {
        bool cached = false;
        if ( stream.Read(cached) )
        {
            if (cached)
            {
                U32 offset = 0;
                if ( stream.Read(offset) )
                {
                    auto it = m_readCache.find(offset);
                    if ( it != m_readCache.end() )
                    {
                        s = it->second;
                        return true;
                    }
                }
            }
            else
            {
                size_t offset = stream.GetBitOffset();
                if ( stream.Read(s) )
                {
                    m_readCache.emplace(offset, s);
                    return true;
                }
            }
        }

        return false;
    }

    virtual void Write(BitStreamOutput& stream, const String& s, const String& tag) override
    {
        auto it = m_writeCache.find(s);
        bool cached = it != m_writeCache.end();
        stream.Write(cached);
        if (cached)
        {
            U32 offset = (U32)it->second;
            stream.Write(offset);
        }
        else
        {
            m_writeCache[s] = stream.GetBitOffset();
            stream.Write(s);
        }
    }

    virtual void Reset() override // NB: 'override' used here since Reset was not a pure virtual function, with this keyword, the compiler complains if I made a typo
    {
        m_readCache.clear();
        m_writeCache.clear();
    }

private:
    std::unordered_map<size_t, String> m_readCache;
    std::unordered_map<String, size_t> m_writeCache;
};

DEFINE_DATA_POLICY_CREATOR(String, UniqueStringPolicy);

DEFINE_DATA_POLICY(unique, UniqueStringPolicy);

