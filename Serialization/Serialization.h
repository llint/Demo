//
//  Serialization.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_Serialization_h
#define SerializationFramework_Serialization_h

#include "BitStream.h"
#include "UniformQuantization.h"
#include "TypeList.h"
#include "MetadataProcessor.h"

////////////////////////////////////////////////////////////////////////////////
// Scatter class definitions
// It is useful to generate a combined interface from a collection of typed interface nodes

template < typename T, template <typename> class Node >
struct Scatter : public Node<T>
{
};

template < typename... Ts, template <typename> class Node >
struct Scatter< TypeList<Ts...>, Node > :
    public Scatter< typename TypeList<Ts...>::Head, Node >,
    public Scatter< typename TypeList<Ts...>::Tail, Node >
{
};

template < template <typename> class Node >
struct Scatter< TypeList<>, Node >
{
};

template < typename T, typename... Ts, template <typename> class Node, template < typename, template <typename> class > class C >
Node<T>& ScatterCast(C< TypeList<Ts...>, Node >& r)
{
    return r;
};

template < typename T, typename... Ts, template <typename> class Node, template < typename, template <typename> class > class C >
Node<T>* ScatterCast(C< TypeList<Ts...>, Node >* p)
{
    return p;
};

template < typename T, typename... Ts, template <typename> class Node, template < typename, template <typename> class > class C >
const Node<T>& ScatterCast(const C< TypeList<Ts...>, Node >& r)
{
    return r;
};

template < typename T, typename... Ts, template <typename> class Node, template < typename, template <typename> class > class C >
const Node<T>* ScatterCast(const C< TypeList<Ts...>, Node >* p)
{
    return p;
};

////////////////////////////////////////////////////////////////////////////////
// Inherit class definitions
// It is useful to generate a concrete class that contains typed implementations for the derived typed interface nodes

template < typename TL0, typename TL1, template <typename T, class TL, class B> class Node, class Root >
struct Inherit;

template < typename... Ts0, typename... Ts1, template <typename T, class TL, class B> class Node, class Root >
struct Inherit< TypeList<Ts0...>, TypeList<Ts1...>, Node, Root > :
    public Node< typename TypeList<Ts0...>::Head, TypeList<Ts1..., Ts0...>, Inherit< typename TypeList<Ts0...>::Tail, TypeList< Ts1..., typename TypeList<Ts0...>::Head >, Node, Root > >
{
};

template < typename... Ts, template <typename T, class TL, class B> class Node, class Root >
struct Inherit< TypeList<>, TypeList<Ts...>, Node, Root > : public Root
{
};

////////////////////////////////////////////////////////////////////////////////
// IDataPolicy is a typed interface used by the serialization framework to carry out actual data processing
// Individual DataPolicy classes should implement this interface based on their individual settings
// DataPolicyDefault classes implement a handful default data policies for basic data types

template <typename T>
struct IDataPolicy
{
    typedef std::unique_ptr< IDataPolicy<T> > ptr;

    virtual bool Read(const BitStreamInput& stream, T& v, const String& tag) = 0;
    virtual void Write(BitStreamOutput& stream, const T& v, const String& tag) = 0;

    virtual void Reset() {}

    virtual ~IDataPolicy() {}
};

template <typename T, typename = void>
struct DataPolicyDefault : public IDataPolicy<T>
{
    // NB: intentionally left empty, so we should get compiler errors if T is not proper supported
};

template <>
struct DataPolicyDefault<bool> : public IDataPolicy<bool>
{
    virtual bool Read(const BitStreamInput& stream, bool& b, const String& tag)
    {
        return stream.Read(b);
    }

    virtual void Write(BitStreamOutput& stream, const bool& b, const String& tag)
    {
        stream.Write(b);
    }
};

template <typename I>
struct DataPolicyDefault< I, typename std::enable_if< std::is_integral<I>::value && !std::is_same<I, bool>::value >::type > : public IDataPolicy<I>
{
    virtual bool Read(const BitStreamInput& stream, I& i, const String& tag)
    {
        return stream.Read(i);
    }

    virtual void Write(BitStreamOutput& stream, const I& i, const String& tag)
    {
        stream.Write(i);
    }
};

// NB: 32-bit floating point values by default are always quantized to 32-bit integers in the range of 16-bit integer, leaving 16-bit fixed point precision
template <>
struct DataPolicyDefault<F32> : public IDataPolicy<F32>
{
    const UniformQuantization<F32, U32> q;

    DataPolicyDefault() : q( std::numeric_limits<I16>::min(), std::numeric_limits<I16>::max() ) {}

    virtual bool Read(const BitStreamInput& stream, F32& f, const String& tag)
    {
        return q.Read(stream, f, tag);
    }

    virtual void Write(BitStreamOutput& stream, const F32& f, const String& tag)
    {
        q.Write(stream, f, tag);
    }
};

// NB: 64-bit floating point values by default are always quantized to 64-bit integers in the range of 32-bit integer, leaving 32-bit fixed point precision
template <>
struct DataPolicyDefault<F64> : public IDataPolicy<F64>
{
    const UniformQuantization<F64, U64> q;

    DataPolicyDefault() : q( std::numeric_limits<I32>::min(), std::numeric_limits<I32>::max() ) {}

    virtual bool Read(const BitStreamInput& stream, F64& f, const String& tag)
    {
        return q.Read(stream, f, tag);
    }

    virtual void Write(BitStreamOutput& stream, const F64& f, const String& tag)
    {
        q.Write(stream, f, tag);
    }
};

template <>
struct DataPolicyDefault<String> : public IDataPolicy<String>
{
    virtual bool Read(const BitStreamInput& stream, String& s, const String& tag)
    {
        return stream.Read(s);
    }

    virtual void Write(BitStreamOutput& stream, const String& s, const String& tag)
    {
        stream.Write(s);
    }
};

template <>
struct DataPolicyDefault<Buffer> : public IDataPolicy<Buffer>
{
    virtual bool Read(const BitStreamInput& stream, Buffer& b, const String& tag)
    {
        return stream.Read(b);
    }

    virtual void Write(BitStreamOutput& stream, const Buffer& b, const String& tag)
    {
        stream.Write(b);
    }
};

template < typename T, typename... Xs, template <typename...> class C >
struct DataPolicyDefault< C<T, Xs...>, typename std::enable_if< std::is_integral<T>::value || std::is_floating_point<T>::value >::type > : public IDataPolicy< C<T, Xs...> >
{
    DataPolicyDefault<T> policy;
    DataPolicyDefault<U32> policy_sz;

    DataPolicyDefault() : policy(), policy_sz() {}

    virtual bool Read(const BitStreamInput& stream, C<T, Xs...>& c, const String& tag)
    {
        U32 sz = 0;
        if ( policy_sz.Read(stream, sz, tag) )
        {
            c.resize(sz);
            for (U32 i = 0; i < sz; ++i)
            {
                if ( !policy.Read(stream, c[i], tag) )
                {
                    return false;
                }
            }
        }
        return true;
    }

    virtual void Write(BitStreamOutput& stream, const C<T, Xs...>& c, const String& tag)
    {
        U32 sz = (U32)c.size();
        policy_sz.Write(stream, sz, tag);
        for (U32 i = 0; i < sz; ++i)
        {
            policy.Write(stream, c[i], tag);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
// IDataPolicyContainerNode and DataPolicyContainerNode are typed container node interface and implementation

template <typename T>
struct IDataPolicyContainerNode
{
    typedef std::function< typename IDataPolicy<T>::ptr (const IMetadataProcessor::Elements&) > Creator;
    virtual bool RegisterCreator(const String& name, const Creator& creator, T* = 0) = 0;

    virtual void LoadPolicies(const IMetadataProcessor::Elements& policies, T* = 0) = 0;

    virtual void ResetPolicies(T* = 0) = 0;

    virtual const typename IDataPolicy<T>::ptr& GetPolicy(const String& name, T* = 0) const = 0;

    virtual void Setup(const IDataPolicyContainerNode& rhs) = 0;
};

template <typename T, class TL, class Base>
class DataPolicyContainerNode;

template <typename T, class Base, typename... Ts>
class DataPolicyContainerNode< T, TypeList<Ts...>, Base > : public Base
{
public:
    DataPolicyContainerNode()
        : m_default( new DataPolicyDefault<T>() ) // C++14: make_unique!
    {}

    virtual void Setup(const IDataPolicyContainerNode<T>& rhs_)
    {
        const DataPolicyContainerNode& rhs = static_cast<const DataPolicyContainerNode&>(rhs_);

        // m_default should have been created!
        m_creators = rhs.m_creators;

        LoadPolicies(rhs.m_elements);
    }

    virtual bool RegisterCreator(const String& name, const typename IDataPolicyContainerNode<T>::Creator& creator, T* = 0)
    {
        m_creators[name] = creator;
        return true;
    }

    virtual void LoadPolicies(const IMetadataProcessor::Elements& elements, T* = 0)
    {
        for (const auto& element : elements)
        {
            if (element.name == "policy")
            {
                auto it_name = element.attributes.find("name");
                if (it_name == element.attributes.cend())
                {
                    continue;
                }
                String policyName = it_name->second;

                auto it_class = element.attributes.find("class");
                if (it_class == element.attributes.cend())
                {
                    continue;
                }
                String className = it_class->second;

                if ( m_creators.find(className) != m_creators.end() )
                {
                    m_policies[policyName] = m_creators[className](element.children);
                }
            }
            else if (element.name == "alias")
            {
                auto it_name = element.attributes.find("name");
                if (it_name == element.attributes.cend())
                {
                    continue;
                }
                String aliasName = it_name->second;

                auto it_policy = element.attributes.find("policy");
                if (it_policy == element.attributes.cend())
                {
                    continue;
                }
                String policyName = it_policy->second;

                if ( m_policies.find(policyName) != m_policies.end() )
                {
                    m_aliases[aliasName] = policyName;
                }
            }
        }

        m_elements.insert( m_elements.end(), elements.begin(), elements.end() ); // NB: duplicates?
    }

    virtual void ResetPolicies(T* = 0)
    {
        for (auto& policy : m_policies)
        {
            policy.second->Reset();
        }

        m_default->Reset();
    }

    virtual const typename IDataPolicy<T>::ptr& GetPolicy(const String& policyName, T* = 0) const
    {
        if ( !policyName.empty() )
        {
            String actualName = policyName;
            auto ita = m_aliases.find(policyName);
            if ( ita != m_aliases.end() )
                actualName = ita->second;
            auto itp = m_policies.find(actualName);
            if ( itp != m_policies.end() )
                return itp->second;
        }

        return m_default;
    }

private:
    typedef std::unordered_map< String, typename IDataPolicyContainerNode<T>::Creator > CreatorsRegistry;
    CreatorsRegistry m_creators;

    typedef std::unordered_map< String, typename IDataPolicy<T>::ptr > DataPolicies;
    DataPolicies m_policies;

    typedef std::unordered_map<String, String> Aliases;
    Aliases m_aliases;

    IMetadataProcessor::Elements m_elements;

    typename IDataPolicy<T>::ptr m_default;
};

////////////////////////////////////////////////////////////////////////////////
// IDataPolicyContainer and DataPolicyContainer are aggregated interface and class that contains all the typed interfaces and implementations given a list of types

template < typename TL, template <typename> class Node = IDataPolicyContainerNode >
struct IDataPolicyContainer;

template <typename... Ts>
struct IDataPolicyContainer< TypeList<Ts...> > : public Scatter< TypeList<Ts...>, IDataPolicyContainerNode >
{
};

template < typename TL, template <typename> class Node = IDataPolicyContainerNode >
struct DataPolicyContainer;

template <typename... Ts>
struct DataPolicyContainer< TypeList<Ts...> > : public Inherit< TypeList<Ts...>, TypeList<>, DataPolicyContainerNode, IDataPolicyContainer< TypeList<Ts...> > >
{
    void Setup(const DataPolicyContainer& rhs)
    {
        TypeList<Ts...>::template Apply<Setupper>(*this, rhs);
    }

    void LoadPolicies(const IMetadataProcessor& processor)
    {
        const IMetadataProcessor::Elements& elements = processor.Retrieve();

        TypeList<Ts...>::template Apply<Loader>(*this, elements);
    }

    void ResetPolicies()
    {
        TypeList<Ts...>::template Apply<Resetter>(*this);
    }

    template <typename T>
    struct Setupper
    {
        static void Apply(DataPolicyContainer& lhs, const DataPolicyContainer& rhs)
        {
            ScatterCast<T>(lhs).Setup( ScatterCast<T>(rhs) );
        }
    };

    template <typename T>
    struct Loader
    {
        static void Apply(DataPolicyContainer& manager, const IMetadataProcessor::Elements& elements)
        {
            ScatterCast<T>(manager).LoadPolicies(elements);
        }
    };

    template <typename T>
    struct Resetter
    {
        static void Apply(DataPolicyContainer& manager)
        {
            ScatterCast<T>(manager).ResetPolicies();
        }
    };
};

template <typename TL>
class DataPolicyContainerPreload;

template <typename... Ts>
class DataPolicyContainerPreload< TypeList<Ts...> >
{
public:
    static DataPolicyContainerPreload& Singleton()
    {
        static DataPolicyContainerPreload s_singleton;
        return s_singleton;
    }

    template <typename T>
    bool RegisterCreator(const String& name, const typename IDataPolicyContainerNode<T>::Creator& creator)
    {
        return ScatterCast<T>(m_preload).RegisterCreator(name, creator);
    }

    void LoadPolicies(const IMetadataProcessor& processor)
    {
        m_preload.LoadPolicies(processor);
    }

    const DataPolicyContainer< TypeList<Ts...> >& Retrieve() const
    {
        return m_preload;
    }

private:
    DataPolicyContainerPreload() {}
    ~DataPolicyContainerPreload() {}

    DataPolicyContainer< TypeList<Ts...> > m_preload;
};

////////////////////////////////////////////////////////////////////////////////
// Finally, the actual serialization support; note that TypeList is used explicitly, so application logic can
// define a concrete TypeList<...> and pass it in to be used in the generic code

template <typename T>
struct ISerializationNode
{
    virtual bool Serialize(T& v, const String& policy, const String& tag) = 0;
};

// The base ISerialization interface which contains all the typed interface nodes
template < typename TL, template <typename> class Node = ISerializationNode >
struct ISerialization;

template <typename... Ts>
struct ISerialization< TypeList<Ts...> > : public Scatter< TypeList<Ts...>, ISerializationNode >
{
    virtual bool IsReading() const = 0;

protected:
    virtual BitStreamOutput& GetBitStreamOutputImpl() const = 0;
    virtual const BitStreamInput& GetBitStreamInputImpl() const = 0;

    virtual DataPolicyContainer< TypeList<Ts...> >& GetDataPolicyContainerImpl() const = 0;

    BitStreamOutput& GetBitStreamOutput() const
    {
        return GetBitStreamOutputImpl();
    }

    const BitStreamInput& GetBitStreamInput() const
    {
        return GetBitStreamInputImpl();
    }

    DataPolicyContainer< TypeList<Ts...> >& GetDataPolicyContainer() const
    {
        return GetDataPolicyContainerImpl();
    }
};

// Typed SerializationOutput implementation node (Inherit)
template <typename T, class TL, class Base>
struct SerializationNodeOutput;

template <typename T, class Base, typename... Ts>
struct SerializationNodeOutput< T, TypeList<Ts...>, Base > : public Base
{
    virtual bool Serialize(T& v, const String& policy, const String& tag)
    {
        ScatterCast<T>( Base::GetDataPolicyContainer() ).GetPolicy(policy)->Write( Base::GetBitStreamOutput(), v, tag );
        return true;
    }
};

// Typed SerializationInput implementation node (Inherit)
template <typename T, class TL, class Base>
struct SerializationNodeInput;

template <typename T, class Base, typename... Ts>
struct SerializationNodeInput< T, TypeList<Ts...>, Base > : public Base
{
    virtual bool Serialize(T& v, const String& policy, const String& tag)
    {
        return ScatterCast<T>( Base::GetDataPolicyContainer() ).GetPolicy(policy)->Read( Base::GetBitStreamInput(), v, tag );
    }
};

// The concrete SerializationOutput class which implements all the typed interfaces contained within the ISerialization interface, using SerializationNodeOutput
template <typename TL>
class SerializationOutput;

template <typename... Ts>
class SerializationOutput< TypeList<Ts...> > : public Inherit< TypeList<Ts...>, TypeList<>, SerializationNodeOutput, ISerialization< TypeList<Ts...> > >
{
public:
    SerializationOutput(DataPolicyContainer< TypeList<Ts...> >& container, BitStreamOutput& output, bool reset = true)
        : m_container(container)
        , m_output(output)
        , m_input(nullptr)
    {
        if (reset)
        {
            container.ResetPolicies();
        }
    }

    virtual bool IsReading() const { return false; }

protected:
    virtual BitStreamOutput& GetBitStreamOutputImpl() const { return m_output; }
    virtual const BitStreamInput& GetBitStreamInputImpl() const { return *m_input; }

    virtual DataPolicyContainer< TypeList<Ts...> >& GetDataPolicyContainerImpl() const { return m_container; }

    BitStreamOutput& m_output;
    BitStreamInput* const m_input; // unused

    DataPolicyContainer< TypeList<Ts...> >& m_container;
};

// The concrete SerializationInput class which implements all the typed interfaces contained within the ISerialization interface, using SerializationNodeInput
template <typename TL>
class SerializationInput;

template <typename... Ts>
class SerializationInput< TypeList<Ts...> > : public Inherit< TypeList<Ts...>, TypeList<>, SerializationNodeInput, ISerialization< TypeList<Ts...> > >
{
public:
    SerializationInput(DataPolicyContainer< TypeList<Ts...> >& container, const BitStreamInput& input, bool reset = true)
        : m_container(container)
        , m_input(input)
        , m_output(nullptr)
    {
        if (reset)
        {
            container.ResetPolicies();
        }
    }

    virtual bool IsReading() const { return true; }

protected:
    virtual BitStreamOutput& GetBitStreamOutputImpl() const { return *m_output; }
    virtual const BitStreamInput& GetBitStreamInputImpl() const { return m_input; }

    virtual DataPolicyContainer< TypeList<Ts...> >& GetDataPolicyContainerImpl() const { return m_container; }

    BitStreamOutput* const m_output; // unused
    const BitStreamInput& m_input;

    DataPolicyContainer< TypeList<Ts...> >& m_container;
};

// Helper function which hides the need for user code to explicitly do ScatterCast
template <typename T, typename... Ts, typename = typename std::enable_if< TypeListContainsType< T, TypeList<Ts...> >::value >::type >
static inline bool Serialize(ISerialization< TypeList<Ts...> >& s, T& v, const String& policy = "", const String& tag = "")
{
    return ScatterCast<T>(s).Serialize(v, policy, tag);
}

template <typename T, typename TL>
struct HasSerializeMemberFunction;

template <typename T, typename... Ts>
struct HasSerializeMemberFunction< T, TypeList<Ts...> >
{
    template < class C, bool (C::*)(ISerialization< TypeList<Ts...> >& ) > struct SFINAE {};

    template <class C> static char Test(SFINAE<C, &C::Serialize>*);
    template <class C> static long Test(...);

    static const bool value = sizeof( Test<T>(0) ) == sizeof(char);
};

template < class C, typename... Ts, typename = typename std::enable_if< !TypeListContainsType< C, TypeList<Ts...> >::value && HasSerializeMemberFunction< C, TypeList<Ts...> >::value >::type >
static inline bool Serialize(ISerialization< TypeList<Ts...> >& s, C& o)
{
    return o.Serialize(s);
}

template < class C, typename... Ts, template <typename...> class V, typename... Xs, typename = typename std::enable_if< !TypeListContainsType< C, TypeList<Ts...> >::value && !HasSerializeMemberFunction< V<C, Xs...>, TypeList<Ts...> >::value && HasSerializeMemberFunction< C, TypeList<Ts...> >::value >::type >
static inline bool Serialize(ISerialization< TypeList<Ts...> >& s, V<C, Xs...>& elements)
{
    U32 sz = (U32)elements.size();
    if ( !::Serialize(s, sz) )
    {
        return false;
    }

    if ( s.IsReading() )
    {
        elements.resize(sz);
    }

    for ( size_t i = 0; i < elements.size(); ++i )
    {
        if ( !elements[i].Serialize(s) )
        {
            return false;
        }
    }

    return true;
}

// The wrapper class for SerializationOutput
template <typename TL>
class SerializationOutputWrapper;

template <typename... Ts>
class SerializationOutputWrapper< TypeList<Ts...> >
{
public:
    SerializationOutputWrapper(DataPolicyContainer< TypeList<Ts...> >& container, Buffer& buffer, bool reset = true)
        : m_stream(buffer)
        , m_output(container, m_stream, reset)
    {}

    ~SerializationOutputWrapper() {}

    operator SerializationOutput< TypeList<Ts...> >&()
    {
        return m_output;
    }

private:
    BitStreamOutput m_stream;
    SerializationOutput< TypeList<Ts...> > m_output;
};

// The wrapper class for SerializationInput
template <typename TL>
class SerializationInputWrapper;

template <typename... Ts>
class SerializationInputWrapper< TypeList<Ts...> >
{
public:
    SerializationInputWrapper(DataPolicyContainer< TypeList<Ts...> >& container, const Buffer& buffer, bool reset = true)
        : m_stream(buffer)
        , m_input(container, m_stream, reset)
    {}

    ~SerializationInputWrapper() {}

    operator SerializationInput< TypeList<Ts...> >&()
    {
        return m_input;
    }

private:
    BitStreamInput m_stream;
    SerializationInput< TypeList<Ts...> > m_input;
};

////////////////////////////////////////////////////////////////////////////////
// Typedefs ...
typedef TypeList<String, Buffer, F64, F32, I64, U64, I32, U32, I16, U16, I8, U8, bool> CoreSerializationTypes;

typedef ISerialization<CoreSerializationTypes> ISerializationType;

typedef DataPolicyContainer<CoreSerializationTypes> DataPolicyContainerType;
typedef DataPolicyContainerPreload<CoreSerializationTypes> DataPolicyContainerPreloadType;

typedef SerializationOutputWrapper<CoreSerializationTypes> SerializationOutputWrapperType;
typedef SerializationInputWrapper<CoreSerializationTypes> SerializationInputWrapperType;

////////////////////////////////////////////////////////////////////////////////
// Macros ...

#define SERIALIZE(s, value) do { if ( !::Serialize(s, value) ) return false; } while (0)
#define CONDITIONAL_SERIALIZE(s, cond, value) do { SERIALIZE(s, cond); if (cond) { SERIALIZE(s, value); } } while (0)

#define SERIALIZE_P(s, value, policy) do { if ( !::Serialize(s, value, policy) ) return false; } while (0)
#define CONDITIONAL_SERIALIZE_P(s, cond, value, policy) do { SERIALIZE(s, cond); if (cond) { SERIALIZE(s, value, policy); } } while (0)

#define CAT(a, b) CAT_I(a ## b)
#define CAT_I(x) x

#define STRINGIZE(a) STRINGIZE_I(#a)
#define STRINGIZE_I(x) x

// This macro needs to be placed inside <DataPolicyClass>.cpp to have the DataPolicyClass register itself
#define DEFINE_DATA_POLICY_CREATOR(T, DataPolicyClass)                                                          \
static IDataPolicy<T>::ptr CAT(StaticCreate_, DataPolicyClass)(const IMetadataProcessor::Elements& elements)    \
{                                                                                                               \
    return IDataPolicy<T>::ptr( new DataPolicyClass(elements) );                                                \
}                                                                                                               \
bool CAT(g_registered_, DataPolicyClass) = DataPolicyContainerPreloadType::Singleton().RegisterCreator<T>( STRINGIZE(DataPolicyClass), CAT(StaticCreate_, DataPolicyClass) )

// This macro needs to be placed inside one of the normal compilation unit other than <DataPolicyClass>.cpp, so the data policy class definition symbols can be properly linked
#define FORCE_LINK_DATA_POLICY_CLASS(DataPolicyClass)   \
extern bool CAT(g_registered_, DataPolicyClass);        \
static const bool CAT(s_forcelink_, DataPolicyClass) = CAT(g_registered_, DataPolicyClass)

/*
Element children[] = {
    { "X", { {"min", "-1"}, {"max", "1"}, {"nbits", "10"} } },
    { "Y", { {"min", "-1"}, {"max", "1"}, {"nbits", "10"} } },
    { "Z", { {"min", "-1"}, {"max", "1"}, {"nbits", "10"} } },
};

Element elements[] = { { "policy", { {"name", policyName}, {"class", className} }, {&children[0], &children[sizeof(children)/sizeof(Element)]} } };
*/

#define DEFINE_DATA_POLICY(policyName, className)                                                           \
static bool StaticLoad##_##className##_##policyName()                                                       \
{                                                                                                           \
    IMetadataProcessor::Element element = { "policy", { {"name", #policyName}, {"class", #className} } };   \
    struct Processor : IMetadataProcessor                                                                   \
    {                                                                                                       \
        Processor(const Element& element)                                                                   \
            : elements({element})                                                                           \
        {}                                                                                                  \
        const Elements& Retrieve() const { return elements; }                                               \
        Elements elements;                                                                                  \
    };                                                                                                      \
    Processor processor(element);                                                                           \
    DataPolicyContainerPreloadType::Singleton().LoadPolicies(processor);                                    \
    return true;                                                                                            \
}                                                                                                           \
static bool s_loaded##_className##_##policyName = StaticLoad##_##className##_##policyName()

#endif
