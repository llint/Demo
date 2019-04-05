//
//  Variant.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_Variant_h
#define SerializationFramework_Variant_h

#include "Types.h"
#include "TypeList.h"
#include "Serialization.h"

template <typename T>
class RecursiveWrapper
{
public:
    typedef T Type;

    RecursiveWrapper()
        : m_value( new T() )
    {
    }

    RecursiveWrapper(const RecursiveWrapper& rhs)
        : m_value( new T(*rhs.m_value) )
    {
    }

    RecursiveWrapper(RecursiveWrapper&& rhs)
        : m_value( std::move(rhs.m_value) )
    {
    }

    RecursiveWrapper(const T& t)
        : m_value( new T(t) )
    {
    }

    RecursiveWrapper(T&& t)
        : m_value( new T( std::move(t) ) )
    {
    }

    RecursiveWrapper& operator=(const RecursiveWrapper& rhs)
    {
        m_value.reset( new T(*rhs.m_value) );
        return *this;
    }

    RecursiveWrapper& operator=(RecursiveWrapper&& rhs)
    {
        m_value = std::move(rhs.m_value);
        return *this;
    }

    RecursiveWrapper& operator=(const T& t)
    {
        m_value.reset( new T(t) );
        return *this;
    }

    RecursiveWrapper& operator=(T&& t)
    {
        m_value.reset( new T( std::move(t) ) );
        return *this;
    }

    operator const T&() const
    {
        return *m_value;
    }

    operator T&()
    {
        return *m_value;
    }

    bool Serialize(ISerializationType& s)
    {
        SERIALIZE(s, *m_value);
        return true;
    }

private:
    std::unique_ptr<T> m_value;
};

template <typename T>
struct Unwrap
{
    typedef T Type;
};

template <typename T>
struct Unwrap< RecursiveWrapper<T> >
{
    typedef T Type;
};

template <typename TL>
class Variant;

template <typename... Ts>
class Variant< TypeList<Ts...> >
{
    // NB: this effectively limits the maximum number of types to be 255, and the index range is from 0 to 254 -
    // leaving 255 as an invalid index (one exceeding the last valid index 254)!
    static_assert( sizeof...(Ts) < 256, "Exceeding maximum number of types supported" );

public:
    Variant()
        : m_index(TypeListCount< TypeList<Ts...> >::value)
    {
    }

    // Special syntax to default construct the Variant to be T
    template < typename T, typename = typename std::enable_if< TypeListContainsType< T, TypeList<Ts...> >::value >::type >
    Variant(const T*, const T*)
        : m_index(TypeListTypeIndex< T, TypeList<Ts...> >::value)
    {
        new( (T*)&m_storage ) T();
    }

    Variant(const Variant& rhs)
        : Variant()
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        Copier copier(*this, rhs);
        apply(rhs.m_index, copier);
    }

    Variant(Variant&& rhs)
        : Variant()
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        Mover mover( *this, std::move(rhs) );
        apply(rhs.m_index, mover);
    }

    //template <typename T>
    //Variant(const T& t)
    //    : Variant()
    //{
    //    typedef typename std::conditional< TypeListContainsType< T, TypeList<Ts...> >::value, T, typename TypeListFindConvertibleType< T, TypeList<Ts...> >::Type >::type TT;
    //    new( (TT*)&m_storage ) TT(t);
    //    m_index = TypeListTypeIndex< TT, TypeList<Ts...> >::value;
    //}

    // NB: the consructor below forms a "universal reference", so 't' doesn't always mean an "rvalue reference"!
    // http://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers
    // Also, overloading with universal reference is a bad idea:
    // http://stackoverflow.com/questions/18264829/universal-reference-vs-const-reference-priority
    template <typename T>
    Variant(T&& t)
        : Variant()
    {
        typedef typename std::remove_reference<T>::type T_; // T might be resolved to be an lvalue reference due to universal reference collapsing rules, so let's remove the reference!
        typedef typename std::conditional< TypeListContainsType< T_, TypeList<Ts...> >::value, T_, typename TypeListFindConvertibleType< T_, TypeList<Ts...> >::Type >::type TT;
        new( (TT*)&m_storage ) TT( std::forward<T>(t) );
        m_index = TypeListTypeIndex< TT, TypeList<Ts...> >::value;
    }

    ~Variant()
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        Destructor destructor(*this);
        apply(m_index, destructor);
    }

    Variant& operator=(const Variant& rhs)
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        Destructor destructor(*this);
        apply(m_index, destructor);

        Copier copier(*this, rhs);
        apply(rhs.m_index, copier);

        return *this;
    }

    Variant& operator=(Variant&& rhs)
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        Destructor destructor(*this);
        apply(m_index, destructor);

        Mover mover( *this, std::move(rhs) );
        apply(rhs.m_index, mover);

        return *this;
    }

    // template <typename T>
    // Variant& operator=(const T& t)
    // {
    //     TypeListApplyAt< TypeList<Ts...> > apply;

    //     Destructor destructor(*this);
    //     apply(m_index, destructor);

    //     typedef typename std::conditional< TypeListContainsType< T, TypeList<Ts...> >::value, T, typename TypeListFindConvertibleType< T, TypeList<Ts...> >::Type >::type TT;
    //     new( (TT*)&m_storage ) TT(t);
    //     m_index = TypeListTypeIndex< TT, TypeList<Ts...> >::value;

    //     return *this;
    // }

    template <typename T>
    Variant& operator=(T&& t)
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        Destructor destructor(*this);
        apply(m_index, destructor);

        typedef typename std::remove_reference<T>::type T_; // T might be resolved to be an lvalue reference due to universal reference collapsing rules, so let's remove the reference!
        typedef typename std::conditional< TypeListContainsType< T_, TypeList<Ts...> >::value, T_, typename TypeListFindConvertibleType< T_, TypeList<Ts...> >::Type >::type TT;
        new( (TT*)&m_storage ) TT( std::forward<T>(t) );
        m_index = TypeListTypeIndex< TT, TypeList<Ts...> >::value;

        return *this;
    }

    template <typename F>
    void Apply(F& f)
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        Visitor<F> visitor(f, *this);
        apply(m_index, visitor);
    }

    template <typename F>
    void ConstApply(F& f) const
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        ConstVisitor<F> visitor(f, *this);
        apply(m_index, visitor);
    }

    template <typename T>
    const T& Get() const
    {
        if (m_index != TypeListTypeIndex< T, TypeList<Ts...> >::value)
        {
            static const T t;
            return t;
        }

        return *(T*)&m_storage;
    }

    template <typename T>
    T& Get()
    {
        if (m_index != TypeListTypeIndex< T, TypeList<Ts...> >::value)
        {
            static T t;
            return t;
        }

        return *(T*)&m_storage;
    }

    template <typename T>
    bool IsType() const
    {
        return m_index == TypeListTypeIndex< T, TypeList<Ts...> >::value;
    }

    // NB: ISerialization relies on a different list of types!
    bool Serialize(ISerializationType& s)
    {
        TypeListApplyAt< TypeList<Ts...> > apply;

        if ( s.IsReading() )
        {
            Destructor destructor(*this);
            apply(m_index, destructor);
        }

        // NB: it would be insane if we support more than 255 types!
        U8 index = (U8)m_index;
        SERIALIZE(s, index);
        m_index = index;

        if ( s.IsReading() )
        {
            Constructor constructor(*this);
            apply(m_index, constructor);
        }

        Serializer serializer(s, *this);
        apply(m_index, serializer);

        return serializer.r;
    }

private:
    typename std::aligned_storage< MaxSize<Ts...>::value, MaxAlign<Ts...>::value >::type m_storage;

    size_t m_index;

    struct Constructor
    {
        Variant< TypeList<Ts...> >& v;

        Constructor(Variant< TypeList<Ts...> >& v_)
            : v(v_)
        {
        }

        template <typename T>
        void operator()()
        {
            new( (T*)&v.m_storage ) T();
        }
    };

    struct Destructor
    {
        Variant< TypeList<Ts...> >& v;

        Destructor(Variant< TypeList<Ts...> >& v_)
            : v(v_)
        {
        }

        template <typename T>
        void operator()()
        {
            ( (T*)&v.m_storage )->~T();
        }
    };

    struct Copier
    {
        Variant< TypeList<Ts...> >& dst;
        const Variant< TypeList<Ts...> >& src;

        Copier(Variant< TypeList<Ts...> >& dst_, const Variant< TypeList<Ts...> >& src_)
            : dst(dst_)
            , src(src_)
        {
        }

        template <typename T>
        void operator()()
        {
            new( (T*)&dst.m_storage ) T( *(T*)&src.m_storage );
            dst.m_index = src.m_index;
        }
    };

    struct Mover
    {
        Variant< TypeList<Ts...> >& dst;
        Variant< TypeList<Ts...> >&& src;

        Mover(Variant< TypeList<Ts...> >& dst_, Variant< TypeList<Ts...> >&& src_)
            : dst(dst_)
            , src( std::move(src_) )
        {
        }

        template <typename T>
        void operator()()
        {
            new( (T*)&dst.m_storage ) T( std::move( *(T*)&src.m_storage ) );
            dst.m_index = src.m_index;
        }
    };

    template <typename F>
    struct Visitor
    {
        F& f;
        Variant< TypeList<Ts...> >& v;

        Visitor(F& f_, Variant< TypeList<Ts...> >& v_)
            : f(f_)
            , v(v_)
        {
        }

        template <typename T>
        void operator()()
        {
            f( (typename Unwrap<T>::Type&)*(T*)&v.m_storage );
        }
    };

    template <typename F>
    struct ConstVisitor
    {
        F& f;
        const Variant< TypeList<Ts...> >& v;

        ConstVisitor(F& f_, const Variant< TypeList<Ts...> >& v_)
            : f(f_)
            , v(v_)
        {
        }

        template <typename T>
        void operator()()
        {
            f( (const typename Unwrap<T>::Type&)*(const T*)&v.m_storage );
        }
    };

    struct Serializer
    {
        ISerializationType& s;
        Variant< TypeList<Ts...> >& v;
        bool r;

        Serializer(ISerializationType& s_, Variant< TypeList<Ts...> >& v_)
            : s(s_)
            , v(v_)
            , r(false)
        {
        }

        template <typename T>
        bool Serialize()
        {
            // NB: index has to be serializd earlier, because we use it to determine which type to use here (for reading)
            SERIALIZE( s, *(T*)&v.m_storage );
            return true;
        }

        template <typename T>
        void operator()()
        {
            r = Serialize<T>();
        }
    };
};

#endif
