//
//  TypeList.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_TypeList_h
#define SerializationFramework_TypeList_h

////////////////////////////////////////////////////////////////////////////////
// TypeList contains a list of types; extraction of individual types is recursive on Head and Tail
// TypeList also provides facility to invoke an external function for each contained type
// TypeList is also a bridge for the application code to define a concrete list of types for the generic code here

template <typename... Ts>
struct TypeList;

template <typename T, typename... Ts>
struct TypeList<T, Ts...>
{
    typedef T Head;
    typedef TypeList<Ts...> Tail;

    template < template <typename> class C, typename... Args>
    static void Apply(Args&&... args)
    {
        C<T>::Apply( std::forward<Args>(args)... );
        TypeList<Ts...>::template Apply<C>( std::forward<Args>(args)... );
    }
};

template <>
struct TypeList<>
{
    template < template <typename> class C, typename... Args>
    static void Apply(Args&&... args) {}
};

template <typename T, typename TL>
struct TypeListContainsType
{
    static const bool value = false;
};

template <typename T, typename... Ts>
struct TypeListContainsType< T, TypeList<Ts...> >
{
    static const bool value = std::is_same< T, typename TypeList<Ts...>::Head >::value || TypeListContainsType< T, typename TypeList<Ts...>::Tail >::value;
};

template <typename T>
struct TypeListContainsType< T, TypeList<> >
{
    static const bool value = false;
};

template <typename TL, typename T>
struct TypeListAddType;

template <typename... Ts, typename T>
struct TypeListAddType< TypeList<Ts...>, T >
{
    typedef typename std::conditional< TypeListContainsType< T, TypeList<Ts...> >::value, TypeList<Ts...>, TypeList<Ts..., T> >::type Type;
};

template <typename T, typename TL, size_t I = 0>
struct TypeListTypeIndex;

template <typename T, typename... Ts, size_t I>
struct TypeListTypeIndex< T, TypeList<Ts...>, I >
{
    static const size_t value = std::is_same< T, typename TypeList<Ts...>::Head >::value ? I : TypeListTypeIndex< T, typename TypeList<Ts...>::Tail, I + 1 >::value;
};

template <typename T, size_t I>
struct TypeListTypeIndex< T, TypeList<>, I >
{
    static const size_t value = I;
};

template <typename TL>
struct TypeListCount;

template <typename... Ts>
struct TypeListCount< TypeList<Ts...> >
{
    static const size_t value = sizeof...(Ts);
};

template <typename TL, size_t I>
struct TypeListTypeAt;

template <typename... Ts, size_t I>
struct TypeListTypeAt< TypeList<Ts...>, I >
{
    typedef typename TypeListTypeAt< typename TypeList<Ts...>::Tail, I - 1 >::Type Type;
};

template <typename... Ts>
struct TypeListTypeAt< TypeList<Ts...>, 0 >
{
    typedef typename TypeList<Ts...>::Head Type;
};

template <typename T, typename TL>
struct TypeListFindConvertibleType;

template <typename T, typename... Ts>
struct TypeListFindConvertibleType< T, TypeList<Ts...> >
{
    typedef typename std::conditional<
        std::is_convertible< T, typename TypeList<Ts...>::Head >::value,
        typename TypeList<Ts...>::Head,
        typename TypeListFindConvertibleType< T, typename TypeList<Ts...>::Tail >::Type
    >::type Type;
};

template <typename T>
struct TypeListFindConvertibleType< T, TypeList<> >
{
    typedef void Type;
};

template <typename TL>
struct TypeListApplyAt;

template <typename... Ts>
struct TypeListApplyAt< TypeList<Ts...> >
{
    template <typename F>
    void operator()(size_t i, F& f)
    {
        if (i == 0)
        {
#ifdef _MSC_VER
            f.operator()< typename TypeList<Ts...>::Head >(); // NB: MSVC seems to be non-standard conforming here - "template" keyword is required!
#else
            f.template operator()< typename TypeList<Ts...>::Head >();
#endif
        }
        else
        {
            TypeListApplyAt< typename TypeList<Ts...>::Tail > next;
            next(i - 1, f);
        }
    }
};

template <>
struct TypeListApplyAt< TypeList<> >
{
    template <typename F>
    void operator()(size_t i, F& f)
    {
    }
};

template <size_t... ss>
struct Max;

template <size_t s, size_t... ss>
struct Max<s, ss...>
{
    static const size_t head = s;
    static const size_t tail = Max<ss...>::value;
    static const size_t value = head >= tail ? head : tail;
};

template<>
struct Max<>
{
    static const size_t value = 0;
};

template <typename... Ts>
struct MaxSize
{
    static const size_t value = Max< sizeof(Ts)... >::value;
};

template <typename... Ts>
struct MaxAlign
{
    static const size_t value = Max< std::alignment_of<Ts>::value... >::value;
};

#endif
