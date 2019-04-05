//
//  MetaStruct.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_MetaStruct_h
#define SerializationFramework_MetaStruct_h

#include "Variant.h"
#include "MetadataProcessor.h"

template <typename TL>
class Struct;

template <typename TL>
class Field;

template <typename... Ts>
class Field< TypeList<Ts...> >
{
public:
    struct ValueType : Variant< TypeList< Ts..., RecursiveWrapper< Struct< TypeList<Ts...> > >, std::deque< RecursiveWrapper<ValueType> > > >
    {
        using Variant< TypeList< Ts..., RecursiveWrapper< Struct< TypeList<Ts...> > >, std::deque< RecursiveWrapper<ValueType> > > >::Variant;

        typedef std::deque< RecursiveWrapper<ValueType> > ArrayType;
        typedef Struct< TypeList<Ts...> > StructType;
        
        bool Serialize(ISerializationType& s)
        {
            return Variant< TypeList< Ts..., RecursiveWrapper< Struct< TypeList<Ts...> > >, std::deque< RecursiveWrapper<ValueType> > > >::Serialize(s);
        }
    };

    Field(const String& name = "")
        : m_name(name)
    {
    }

    Field(Field&& rhs)
        : m_name( std::move(rhs.m_name) )
        , m_value( std::move(rhs.m_value) )
    {
    }

    Field& operator=(Field&& rhs)
    {
        m_name = std::move(rhs.m_name);
        m_value = std::move(rhs.m_value);
        return *this;
    }

    const String& GetName() const
    {
        return m_name;
    }

    bool HasValue() const
    {
        return (bool)m_value;
    }

    const ValueType& GetValue() const
    {
        return *m_value;
    }

    void SetValue(const ValueType& value)
    {
        m_value.reset( new ValueType(value) );
    }

    void SetValue(ValueType&& value)
    {
        m_value.reset( new ValueType( std::move(value) ) );
    }

    template <typename... Xs>
    void SetValue(Xs&&... xs)
    {
        m_value.reset( new ValueType( std::forward<Xs>(xs)... ) );
    }

    template <typename T>
    T& SetValue()
    {
        typedef typename std::conditional< TypeListContainsType< T, TypeList<Ts...> >::value || std::is_same<T, typename ValueType::ArrayType>::value, T,
            typename std::conditional< std::is_same<T, typename ValueType::StructType>::value, RecursiveWrapper<typename ValueType::StructType>, void >::type >::type TT;

        static_assert( !std::is_same<TT, void>::value, "Invalid type" );
        m_value.reset( new ValueType( (TT*)nullptr, (TT*)nullptr ) );
        return m_value->template Get<TT>();
    }

    bool Serialize(ISerializationType& s)
    {
        SERIALIZE_P(s, m_name, "unique");

        bool hasValue = (bool)m_value;
        SERIALIZE(s, hasValue);
        if (hasValue)
        {
            if ( s.IsReading() )
            {
                m_value.reset( new ValueType() );
            }
            SERIALIZE(s, *m_value);
        }

        return true;
    }

private:
    String m_name;
    std::unique_ptr<ValueType> m_value;
};

template <typename... Ts>
class Struct< TypeList<Ts...> >
{
public:
    typedef Field< TypeList<Ts...> > FieldType;

    Struct(const String& name = "")
        : m_name(name)
    {
    }

    Struct(Struct&& rhs)
        : m_name( std::move(rhs.m_name) )
        , m_fields( std::move(rhs.m_fields) )
        , m_mappings( std::move(rhs.m_mappings) )
    {
    }

    Struct& operator=(Struct&& rhs)
    {
        m_name = std::move(rhs.m_name);
        m_fields = std::move(rhs.m_fields);
        m_mappings = std::move(rhs.m_mappings);
        return *this;
    }

    void SetName(const String& name)
    {
        m_name = name;
    }

    const String& GetName() const
    {
        return m_name;
    }

    const FieldType& GetField(const String& name) const
    {
        auto it = m_mappings.find(name);
        if ( it != m_mappings.end() )
        {
            return m_fields[it->second];
        }

        throw -1;
    }

    FieldType& GetField(const String& name)
    {
        auto it = m_mappings.find(name);
        if ( it != m_mappings.end() )
        {
            return m_fields[it->second];
        }

        throw -1;
    }

    bool HasField(const String& name) const
    {
        return m_mappings.find(name) != m_mappings.end();
    }

    FieldType& AddField(const String& name)
    {
        auto it = m_mappings.find(name);
        if ( it != m_mappings.end() )
        {
            return m_fields[it->second];
        }

        m_fields.emplace_back(name);
        m_mappings[name] = m_fields.size() - 1;
        return m_fields.back();
    }

    bool Serialize(ISerializationType& s)
    {
        SERIALIZE_P(s, m_name, "unique");
        SERIALIZE(s, m_fields);

        if ( s.IsReading() )
        {
            for ( size_t i = 0, sz = m_fields.size(); i < sz; ++i )
            {
                m_mappings[m_fields[i].GetName()] = i;
            }
        }

        return true;
    }

    const std::deque<FieldType>& GetFields() const
    {
        return m_fields;
    }

private:
    String m_name; // NB: name of the Struct (type/id), is actually different from a Field name (variable)
    std::unordered_map<String, size_t> m_mappings; // field name to field index mapping
    std::deque<FieldType> m_fields; // fields following the order of metadata definition
};

#endif
