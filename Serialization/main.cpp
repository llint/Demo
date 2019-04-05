//
//  main.cpp
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#include <iostream>
#include <cassert>

#include <chrono>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <lua.hpp>

#include "Serialization.h"
#include "Variant.h"
#include "MetaStruct.h"

FORCE_LINK_DATA_POLICY_CLASS(UniqueStringPolicy);

static void TestBitStream()
{
    static const U8  C_U8  = 0x12;
    static const U16 C_U16 = 0x1234;
    static const U32 C_U32 = 0x12345678;
    static const U64 C_U64 = 0x1234567812345678;

    static const size_t NBITS_U8  = 5;
    static const size_t NBITS_U16 = 13;
    static const size_t NBITS_U32 = 29;
    static const size_t NBITS_U64 = 61;

    static const I8  C_I8  = (I8)0xf2;
    static const I16 C_I16 = (I16)0xf234;
    static const I32 C_I32 = 0xf2345678;
    static const I64 C_I64 = 0xf234567812345678;

    static const size_t NBITS_I8  = NBITS_U8 + 1;
    static const size_t NBITS_I16 = NBITS_U16 + 1;
    static const size_t NBITS_I32 = NBITS_U32 + 1;
    static const size_t NBITS_I64 = NBITS_U64 + 1;
    
    bool pattern[] = { false, true, false, true, true, false, true };

    Buffer buffer;
    BitStreamOutput os(buffer);

    for (size_t i = 0; i < 1000; ++i)
    {
        os.Write( pattern[i % (sizeof(pattern) / sizeof(bool))] );

        U8 u8 = C_U8;
        os.Write(u8);

        I8 i8 = C_I8;
        os.Write(i8);

        U16 u16 = C_U16;
        os.Write(u16);

        I16 i16 = C_I16;
        os.Write(i16);

        U32 u32 = C_U32;
        os.Write(u32);

        I32 i32 = C_I32;
        os.Write(i32);

        U64 u64 = C_U64;
        os.Write(u64);

        I64 i64 = C_I64;
        os.Write(i64);
    }

    BitStreamInput is(buffer);

    for (size_t i = 0; i < 1000; ++i)
    {
        bool r = false;
        assert( is.Read(r) && r == pattern[i % (sizeof(pattern) / sizeof(bool))] );

        U8 u8 = 0;
        assert( is.Read(u8) && u8 == C_U8 );

        I8 i8 = 0;
        assert( is.Read(i8) && i8 == C_I8 );

        U16 u16 = 0;
        assert( is.Read(u16) && u16 == C_U16 );

        I16 i16 = 0;
        assert( is.Read(i16) && i16 == C_I16 );

        U32 u32 = 0;
        assert( is.Read(u32) && u32 == C_U32 );

        I32 i32 = 0;
        assert( is.Read(i32) && i32 == C_I32 );

        U64 u64 = 0;
        assert( is.Read(u64) && u64 == C_U64 );
        
        I64 i64 = 0;
        assert( is.Read(i64) && i64 == C_I64 );
    }
}

struct TestMetaDataProcessor : public IMetadataProcessor
{
    const Elements& Retrieve() const
    {
        return elements;
    }

    Elements elements;
};

struct ST
{
    bool b;
    I8 i8;
    U8 u8;
    I16 i16;
    U16 u16;
    I32 i32;
    U32 u32;
    I64 i64;
    U64 u64;
    F32 f32;
    F64 f64;
    Buffer buffer;
    String string;

    ST()
    : b(true)
    , i8(-8)
    , u8(+8)
    , i16(-16)
    , u16(+16)
    , i32(-32)
    , u32(+32)
    , i64(-64)
    , u64(+64)
    , f32(-0.32f)
    , f64(-0.32f)
    , buffer({1,2,3,4,5})
    , string("hello")
    {}

    void Reset()
    {
        b = false;
        i8 = 0;
        u8 = 0;
        i16 = 0;
        u16 = 0;
        i32 = 0;
        u32 = 0;
        i64 = 0;
        u64 = 0;
        f32 = 0;
        f64 = 0;
        buffer.resize(0);
        //string.resize(0);
    }

    bool Serialize(ISerializationType& s)
    {
        SERIALIZE(s, b);
        SERIALIZE(s, i8);
        SERIALIZE(s, u8);
        SERIALIZE(s, i16);
        SERIALIZE(s, u16);
        SERIALIZE(s, i32);
        SERIALIZE(s, u32);
        SERIALIZE(s, i64);
        SERIALIZE(s, u64);
        SERIALIZE(s, f32);
        SERIALIZE(s, f64);
        SERIALIZE(s, buffer);
        SERIALIZE(s, string);

        return true;
    }
};

static void TestSerialization()
{
    ST s;

    Buffer buffer;

    DataPolicyContainerType container;
    SerializationOutputWrapperType output(container, buffer);
    s.Serialize(output);

    s.Reset();

    SerializationInputWrapperType input(container, buffer);
    s.Serialize(input);
}

////////////////////////////////////////////////////////////////////////////////////
// MapData below is the C++ serializable representation
// In real world use case, this is constructed by parsing a PHP zval (zend_parse_parameter), Python PyObject (PyArg_ParseTuple), or Lua State on their respective C API side
// There should be a PHP/Python/Lua version of the MapData represented in their respective native (PHP/Python/Lua/Etc.) format, and translated to the C++ MapData here
//
// Taking PHP as an example:
//
// A PHP C (zend) extension is created which expose two functions to the PHP code (this is probably *not* legal PHP code):
//
// $BinaryBlob = EncodeMapData($MapData);
// $MapData = DecodeMapData($BinaryBlob);
//
// where $MapData is a native PHP construct which follows the MapData C++ definition (this is the translation protocol between PHP and C++)
// The implementation of the C zend extension would carry the form similar to:
// PHP_FUNCTION(EncodeMapData): which parses the PHP zval and convert it to C++ MapData for serialization
// PHP_FUNCTION(DecodeMapData): which constructs a C++ MapData from a binary stream, and convert to a PHP variable.
// And note that there is only one shared serialization code path, which means that the binary stream is conmpact and efficient to encode/decode

// For demonstration purposes below, we just use the Setup function to feed some fake data in
struct MapData
{
    struct March
    {
        U64 user_id;
        U32 empire_id;
        U32 city_id;
        U32 army_id;

        U32 dest_province_id;
        U32 dest_chunk_id;
        U32 dest_tile_id;
        U32 from_province_id;
        U32 from_chunk_id;
        U32 from_tile_id;
        U32 state;
        U32 start_time;
        U32 dest_time;
        U32 type;
        U32 alliance_id;

        bool has_from_name;
        String from_name;

        bool has_dest_name;
        String dest_name;

        bool has_color;
        U32 color;

        bool has_target_alliance_id;
        U32 target_alliance_id;

        March()
        : user_id(0)
        , empire_id(0)
        , city_id(0)
        , army_id(0)
        , dest_province_id(0)
        , dest_chunk_id(0)
        , dest_tile_id(0)
        , from_province_id(0)
        , from_chunk_id(0)
        , from_tile_id(0)
        , state(0)
        , start_time(0)
        , dest_time(0)
        , type(0)
        , alliance_id(0)
        , has_from_name(false)
        , has_dest_name(false)
        , has_color(false), color(0)
        , has_target_alliance_id(false), target_alliance_id(0)
        {}

        void Setup()
        {
            user_id = 999;
            empire_id = 888;
            city_id = 777;
            army_id = 666;

            dest_province_id = 555;
            dest_chunk_id = 444;
            dest_tile_id = 333;

            from_province_id = 222;
            from_chunk_id = 111;
            from_tile_id = 999;

            state = 23;
            start_time = 2013;
            dest_time = 2013;
            type = 42;
            alliance_id = 888;

            has_from_name = true;
            from_name = "luolin";

            has_dest_name = true;
            dest_name = "linluo";

            has_color = true;
            color = 111;

            has_target_alliance_id = true;
            target_alliance_id = 789;
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, user_id);
            SERIALIZE(s, empire_id);
            SERIALIZE(s, city_id);
            SERIALIZE(s, army_id);

            SERIALIZE(s, dest_province_id);
            SERIALIZE(s, dest_chunk_id);
            SERIALIZE(s, dest_tile_id);
            SERIALIZE(s, from_province_id);
            SERIALIZE(s, from_chunk_id);
            SERIALIZE(s, from_tile_id);
            SERIALIZE(s, state);
            SERIALIZE(s, start_time);
            SERIALIZE(s, dest_time);
            SERIALIZE(s, type);
            SERIALIZE(s, alliance_id);

            CONDITIONAL_SERIALIZE(s, has_from_name, from_name);
            CONDITIONAL_SERIALIZE(s, has_dest_name, dest_name);
            CONDITIONAL_SERIALIZE(s, has_color, color);
            CONDITIONAL_SERIALIZE(s, has_target_alliance_id, target_alliance_id);

            return true;
        }
    };

    struct Alliance
    {
        U32 alliance_id;

        bool has_alliance_name;
        String alliance_name;

        bool has_alliance_tag;
        String alliance_tag;

        bool has_alliance_rank;
        U32 alliance_rank;

        Alliance()
        : alliance_id(0)
        , has_alliance_name(false)
        , has_alliance_tag(false)
        , has_alliance_rank(false), alliance_rank(0)
        {}

        void Setup()
        {
            alliance_id = 456;

            has_alliance_name = true;
            alliance_name = "alliance_name";

            has_alliance_tag = true;
            alliance_tag = "alliance_tag";

            has_alliance_rank = true;
            alliance_rank = 1234;
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, alliance_id);

            CONDITIONAL_SERIALIZE(s, has_alliance_name, alliance_name);
            CONDITIONAL_SERIALIZE(s, has_alliance_tag, alliance_tag);
            CONDITIONAL_SERIALIZE(s, has_alliance_rank, alliance_rank);

            return true;
        }
    };

    struct Empire
    {
        U64 user_id;
        U32 empire_id;

        bool has_empire_name;
        String empire_name;

        bool has_empire_owner;
        String empire_owner;

        bool has_empire_portrait;
        U32 empire_portrait;

        bool has_power;
        U32 power;

        bool has_alliance_id;
        U64 alliance_id;

        bool has_title_id;
        U32 title_id;

        Empire()
        : user_id(0)
        , empire_id(0)
        , has_empire_name(false)
        , has_empire_owner(false)
        , has_empire_portrait(false), empire_portrait(0)
        , has_power(false), power(0)
        , has_alliance_id(false), alliance_id(0)
        , has_title_id(false), title_id(0)
        {}

        void Setup()
        {
            user_id = 666;
            empire_id = 888;

            has_empire_name = true;
            empire_name = "empire_name";

            has_empire_owner = true;
            empire_owner = "empire_owner";

            has_empire_portrait = true;
            empire_portrait = 4545;

            has_power = true;
            power = 4567;

            has_alliance_id = true;
            alliance_id = 1234;

            has_title_id = true;
            title_id = 444;
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, user_id);
            SERIALIZE(s, empire_id);

            CONDITIONAL_SERIALIZE(s, has_empire_name, empire_name);
            CONDITIONAL_SERIALIZE(s, has_empire_owner, empire_owner);
            CONDITIONAL_SERIALIZE(s, has_empire_portrait, empire_portrait);
            CONDITIONAL_SERIALIZE(s, has_power, power);
            CONDITIONAL_SERIALIZE(s, has_alliance_id, alliance_id);
            CONDITIONAL_SERIALIZE(s, has_title_id, title_id);

            return true;
        }
    };

    struct Bounty
    {
        String username;
        U32 bounty;
        String heroname;

        Bounty()
        : bounty(0)
        {}

        void Setup()
        {
            username = "luolin";
            bounty = 1000;
            heroname = "dejavu";
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, username);
            SERIALIZE(s, bounty);
            SERIALIZE(s, heroname);

            return true;
        }
    };

    struct Wonder
    {
        bool has_wonder_name;
        String wonder_name;

        bool has_wonder_name_id;
        U32 wonder_name_id;

        bool has_king_name;
        String king_name;

        bool has_alliance_id;
        U64 alliance_id;

        bool has_scout_cost;
        U32 scout_cost;

        bool has_protection_start_time;
        U32 protection_start_time;

        Wonder()
        : has_wonder_name(false)
        , has_wonder_name_id(false), wonder_name_id(0)
        , has_king_name(false)
        , has_alliance_id(false), alliance_id(0)
        , has_scout_cost(false), scout_cost(0)
        , has_protection_start_time(false), protection_start_time(0)
        {}

        void Setup()
        {
            has_wonder_name = true;
            wonder_name = "abc";

            has_wonder_name_id = true;
            wonder_name_id = 1212;

            has_king_name = true;
            king_name = "Lin";

            has_alliance_id = true;
            alliance_id = 12345678;

            has_scout_cost = true;
            scout_cost = 6789;

            has_protection_start_time = true;
            protection_start_time = 2013;
        }

        bool Serialize(ISerializationType& s)
        {
            CONDITIONAL_SERIALIZE(s, has_wonder_name, wonder_name);
            CONDITIONAL_SERIALIZE(s, has_wonder_name_id, wonder_name_id);
            CONDITIONAL_SERIALIZE(s, has_king_name, king_name);
            CONDITIONAL_SERIALIZE(s, has_alliance_id, alliance_id);
            CONDITIONAL_SERIALIZE(s, has_scout_cost, scout_cost);
            CONDITIONAL_SERIALIZE(s, has_protection_start_time, protection_start_time);

            return true;
        }
    };

    struct Army
    {
        U64 user_id;
        U32 empire_id;
        U32 city_id;
        U32 army_id;

        bool has_scout_cost;
        U32 scout_cost;

        bool has_army_load;
        U32 army_load;

        Army()
        : user_id(0)
        , empire_id(0)
        , city_id(0)
        , army_id(0)
        , has_scout_cost(false), scout_cost(0)
        , has_army_load(false), army_load(0)
        {}

        void Setup()
        {
            user_id = 12345678;
            empire_id = 1234;
            city_id = 4321;
            army_id = 1122;

            has_scout_cost = true;
            scout_cost = 9999;

            has_army_load = true;
            army_load = 7777;
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, user_id);
            SERIALIZE(s, empire_id);
            SERIALIZE(s, city_id);
            SERIALIZE(s, army_id);

            CONDITIONAL_SERIALIZE(s, has_scout_cost, scout_cost);
            CONDITIONAL_SERIALIZE(s, has_army_load, army_load);

            return true;
        }
    };

    struct City
    {
        U64 user_id;
        U32 empire_id;
        U32 city_id;

        bool has_scout_cost;
        U32 scout_cost;

        bool has_city_name;
        String city_name;

        bool has_city_level;
        U32 city_level;

        bool has_truce;
        bool truce;

        bool has_last_state;
        U32 last_state;

        bool has_state_timestamp;
        U32 state_timestamp;

        bool has_bounties;
        std::deque<Bounty> bounties;

        City()
        : user_id(0)
        , empire_id(0)
        , city_id(0)
        , has_scout_cost(false), scout_cost(0)
        , has_city_name(false)
        , has_city_level(false), city_level(0)
        , has_truce(false), truce(false)
        , has_last_state(false), last_state(0)
        , has_state_timestamp(false), state_timestamp(0)
        , has_bounties(false)
        {}

        void Setup()
        {
            user_id = 1234;
            empire_id = 8888;
            city_id = 4567;

            has_scout_cost = true;
            scout_cost = 55;

            has_city_level = true;
            city_level = 33;

            has_truce = true;
            truce = false;

            has_last_state = true;
            last_state = 666;

            has_state_timestamp = true;
            state_timestamp = 5656;

            has_bounties = true;
            bounties.resize(10);
            for (auto& bounty : bounties)
            {
                bounty.Setup();
            }
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, user_id);
            SERIALIZE(s, empire_id);
            SERIALIZE(s, city_id);

            CONDITIONAL_SERIALIZE(s, has_scout_cost, scout_cost);
            CONDITIONAL_SERIALIZE(s, has_city_name, city_name);
            CONDITIONAL_SERIALIZE(s, has_city_level, city_level);
            CONDITIONAL_SERIALIZE(s, has_truce, truce);
            CONDITIONAL_SERIALIZE(s, has_last_state, last_state);
            CONDITIONAL_SERIALIZE(s, has_state_timestamp, state_timestamp);

            CONDITIONAL_SERIALIZE(s, has_bounties, bounties);

            return true;
        }
    };

    struct Tile
    {
        U32 id;

        bool has_overlay;
        U32 overlay;

        bool has_city;
        City city;

        bool has_army;
        Army army;

        bool has_wonder;
        Wonder wonder;

        // resource tile information
        bool has_r_level;
        U32 r_level;

        bool has_r_amount;
        U32 r_amount;

        bool has_r_gather_start_time;
        U32 r_gather_start_time;

        bool has_add_drain_rate;
        U32 add_drain_rate;

        Tile()
        : id(0)
        , has_overlay(false), overlay(0)
        , has_city(false)
        , has_army(false)
        , has_wonder(false)
        , has_r_level(false), r_level(0)
        , has_r_amount(false), r_amount(0)
        , has_r_gather_start_time(false), r_gather_start_time(0)
        , has_add_drain_rate(false), add_drain_rate(0)
        {}

        void Setup()
        {
            id = 333;

            has_overlay = true;
            overlay = 1;

            has_city = true;
            city.Setup();

            has_army = true;
            army.Setup();

            has_wonder = true;
            wonder.Setup();

            has_r_level = true;
            r_level = 50;

            has_r_amount = true;
            r_amount = 100;

            has_r_gather_start_time = true;
            r_gather_start_time = 1000;

            has_add_drain_rate = true;
            add_drain_rate = 100;
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, id);

            CONDITIONAL_SERIALIZE(s, has_overlay, overlay);
            CONDITIONAL_SERIALIZE(s, has_city, city);
            CONDITIONAL_SERIALIZE(s, has_army, army);
            CONDITIONAL_SERIALIZE(s, has_wonder, wonder);
            CONDITIONAL_SERIALIZE(s, has_r_level, r_level);
            CONDITIONAL_SERIALIZE(s, has_r_amount, r_amount);
            CONDITIONAL_SERIALIZE(s, has_r_gather_start_time, r_gather_start_time);
            CONDITIONAL_SERIALIZE(s, has_add_drain_rate, add_drain_rate);

            return true;
        }
    };

    struct Chunk
    {
        U32 p_id;
        U32 c_id;

        bool has_tiles;
        std::deque<Tile> tiles;

        Chunk()
        : p_id(0)
        , c_id(0)
        , has_tiles(false)
        {}

        void Setup()
        {
            p_id = 111;
            c_id = 222;

            has_tiles = true;
            tiles.resize(10);
            for (auto& tile : tiles)
            {
                tile.Setup();
            }
        }

        bool Serialize(ISerializationType& s)
        {
            SERIALIZE(s, p_id);
            SERIALIZE(s, c_id);

            CONDITIONAL_SERIALIZE(s, has_tiles, tiles);

            return true;
        }
    };

    bool has_chunks;
    std::deque<Chunk> chunks;

    bool has_marches;
    std::deque<March> marches;

    bool has_empires;
    std::deque<Empire> empires;

    bool has_alliances;
    std::deque<Alliance> alliances;

    MapData()
    : has_chunks(false)
    , has_marches(false)
    , has_empires(false)
    , has_alliances(false)
    {}

    void Setup()
    {
        has_chunks = true;
        chunks.resize(10);
        for (auto& chunk : chunks)
        {
            chunk.Setup();
        }

        has_marches = true;
        marches.resize(10);
        for (auto& march : marches)
        {
            march.Setup();
        }

        has_empires = true;
        empires.resize(10);
        for (auto& empire : empires)
        {
            empire.Setup();
        }

        has_alliances = true;
        alliances.resize(10);
        for (auto& alliance : alliances)
        {
            alliance.Setup();
        }
    }

    bool Serialize(ISerializationType& s)
    {
        CONDITIONAL_SERIALIZE(s, has_chunks, chunks);
        CONDITIONAL_SERIALIZE(s, has_marches, marches);
        CONDITIONAL_SERIALIZE(s, has_empires, empires);
        CONDITIONAL_SERIALIZE(s, has_alliances, alliances);

        return true;
    }
};

static void TestMapDataSerialization()
{
    Buffer buffer;

    // Sender / Encoder side ... 
    DataPolicyContainerType container;
    SerializationOutputWrapperType output(container, buffer);

    MapData md_out;
    md_out.Setup();

    {
        auto start = std::chrono::high_resolution_clock::now();
        md_out.Serialize(output);
        auto end = std::chrono::high_resolution_clock::now();

        std::cout << "Encoded MapData into " << buffer.size() << " bytes, took: " << std::chrono::duration_cast< std::chrono::duration< float, std::ratio<1, 1000> > >(end - start).count() << " ms" << std::endl;
    }

    // Receiver / Decoder side ...
    SerializationInputWrapperType input(container, buffer);

    MapData md_in;

    {
        auto start = std::chrono::high_resolution_clock::now();
        md_in.Serialize(input);
        auto end = std::chrono::high_resolution_clock::now();

        std::cout << "Decoded MapData from " << buffer.size() << " bytes, took: " << std::chrono::duration_cast< std::chrono::duration< float, std::ratio<1, 1000> > >(end - start).count() << " ms" << std::endl;
    }
}

struct Visitor
{
    template <typename T>
    void operator()(T& t)
    {
        std::cout << "type: " << typeid(T).name() << ", value: " << t << std::endl; 
    }

    void operator()(String& s)
    {
        std::cout << "type: " << typeid(String).name() << ", value: " << s.c_str() << std::endl; 
    }

    void operator()(Buffer& b)
    {
        std::cout << "type: " << "Buffer" << ", size: " << b.size() << std::endl;
    }
};

typedef TypeList<String, I64, F64, bool> VariantTypes;
typedef Struct<VariantTypes> S;

typedef Variant<VariantTypes> V;

struct Serializable
{
    V v;

    bool Serialize(ISerializationType& s)
    {
        SERIALIZE(s, v);
        return true;
    }
};

static void TestVariant()
{
    V v0 = "hello"; // NB: const char[] is implicitly convertible to bool, so we have to make "String" before "bool", so "String" is bound first!
    const auto& s0 = v0.Get<String>();
    V v = std::move(v0);
    const auto& s = v.Get<String>();

    std::cout << "s0: " << s0.c_str() << ", s: " << s.c_str() << std::endl;

    Serializable ser;
    ser.v = "cool stuff";

    Buffer buffer;

    // Sender / Encoder side ... 
    DataPolicyContainerType container;
    SerializationOutputWrapperType output(container, buffer);
    v.Serialize(output);
    ser.Serialize(output);

    // Receiver / Decoder side ...
    SerializationInputWrapperType input(container, buffer);
    V v_in;
    v_in.Serialize(input);
    Serializable ser_in;
    ser_in.Serialize(input);

    const auto& s_in = v_in.Get<String>();
    const auto& ss_in = ser_in.v.Get<String>();

    std::cout << "s_in: " << s_in.c_str() << ", ss_in: " << ss_in.c_str() << std::endl;

    Visitor visitor;
    v_in.Apply(visitor);

    std::vector<V> vv;
    vv.push_back(123);
    vv.push_back("world");
}

class ValuePrinter
{
public:
    ValuePrinter(const std::string& indent = "")
    : m_indent(indent)
    {}

    template <typename T>
    void operator()(const T& t) const
    {
        std::cout << m_indent << t << std::endl;
    }

    void operator()(const String& s) const
    {
        std::cout << m_indent << s.c_str() << std::endl;
    }

    void operator()(const S& s) const
    {
        const auto& fields = s.GetFields();
        for (const auto& field : fields)
        {
            std::cout << m_indent << field.GetName().c_str() << ":" << std::endl;
            if ( field.HasValue() )
            {
                ValuePrinter printer(m_indent + "  ");
                field.GetValue().ConstApply(printer);
            }
        }
    }

    void operator()(const S::FieldType::ValueType::ArrayType& a) const
    {
        for (size_t i = 0; i < a.size(); ++i)
        {
            std::cout << m_indent << i << ":" << std::endl;

            ValuePrinter printer(m_indent + "  ");
            static_cast<const S::FieldType::ValueType&>(a[i]).ConstApply(printer);
        }
    }
    
private:
    const std::string m_indent;
};

static void PrintStruct(const S& s)
{
    ValuePrinter printer;
    printer(s);
}

// NB: the array is heterogeneous, rather than homogeneous!
static void TestMetaStruct()
{
    S mapdata("MapData");

    auto& chunks = mapdata.AddField("chunks").SetValue<S::FieldType::ValueType::ArrayType>();
    for (size_t i = 0; i < 4; ++i)
    {
        S chunk("Chunk");
        chunk.AddField("p_id").SetValue((U32)1000 + (U32)i);
        chunk.AddField("c_id").SetValue((U32)2000 + (U32)i);

        auto& tiles = chunk.AddField("tiles").SetValue<S::FieldType::ValueType::ArrayType>();
        for (size_t j = 0; j < 4; ++j)
        {
            S tile("Tile");
            tile.AddField("id").SetValue((U32)1000 + (U32)i*10 + (U32)j);
            tile.AddField("nm").SetValue("abc");

            tiles.push_back( S::FieldType::ValueType( std::move(tile) ) );
        }
        chunks.push_back( S::FieldType::ValueType( std::move(chunk) ) );
    }

    auto& cells = mapdata.AddField("cells").SetValue<S::FieldType::ValueType::ArrayType>();
    for (size_t i = 0; i < 4; ++i)
    {
        S::FieldType::ValueType::ArrayType row;
        for (size_t j = 0; j < 4; ++j)
        {
            S cell("Cell");
            cell.AddField("a").SetValue((U32)1000 + (U32)i*10 + (U32)j);
            row.push_back( S::FieldType::ValueType( std::move(cell) ) );
        }
        cells.push_back( S::FieldType::ValueType( std::move(row) ) );
    }

    Buffer buffer;

    // Sender / Encoder side ... 
    DataPolicyContainerType container;
    container.Setup( DataPolicyContainerPreloadType::Singleton().Retrieve() );
    SerializationOutputWrapperType output(container, buffer);
    mapdata.Serialize(output);

    // Receiver / Decoder side ...
    SerializationInputWrapperType input(container, buffer);
    S mapdata_in;
    mapdata_in.Serialize(input);

    // Print the data
    PrintStruct(mapdata_in);
}

/////////////////////////////////////////////////////////////////////

static S l_encode_table(lua_State* L);
static S::FieldType::ValueType::ArrayType l_encode_array(lua_State* L);

static bool l_is_array(lua_State* L)
{
    lua_rawgeti(L, -1, 1);
    bool isarray = !lua_isnil(L, -1);
    lua_pop(L, 1);
    return isarray;
}

static S::FieldType::ValueType l_encode_value(lua_State* L)
{
    switch ( lua_type(L, -1) )
    {
        case LUA_TBOOLEAN:
        {
            return (bool)lua_toboolean(L, -1) == 1;
        }
        case LUA_TNUMBER:
        {
            lua_Number x = lua_tonumber(L, -1);
            if ( std::floor(x) == x )
            {
                return (I64)x;
            }
            else
            {
                return (F64)x;
            }
        }
        case LUA_TSTRING:
        {
            return (String)lua_tostring(L, -1);
        }
        case LUA_TTABLE:
        {
            if ( l_is_array(L) )
            {
                return l_encode_array(L);
            }
            else
            {
                return l_encode_table(L);
            }
        }
    }

    throw;
}

S::FieldType::ValueType::ArrayType l_encode_array(lua_State* L)
{
    S::FieldType::ValueType::ArrayType array;

    size_t n = lua_rawlen(L, -1);
    for (size_t i = 1; i <= n; ++i) // NB: Lua array index starts at 1
    {
        lua_rawgeti(L, -1, (int)i);

        array.push_back( l_encode_value(L) );

        lua_pop(L, 1);
    }

    return std::move(array);
}

S l_encode_table(lua_State* L)
{
    S s;

    lua_pushnil(L); // -1 => nil, -2 => table
    while ( lua_next(L, -2) )
    {
        // ***: -1 => value, -2 => key, -3 => table
        // copy the key so that lua_tostring does not modify the original
        lua_pushvalue(L, -2); // -1 => key, -2 => value, -3 => key, -4 => table;
        String key = lua_tostring(L, -1); // NB: our dynamic meta struct doesn't care the syntax of the property name, as long as it's a string that can be retreived and restored the same
        lua_pop(L, 1); // pops the copy of the key, the stack looks like *** again

        // Now we need to deal with "value" at index -1 (top of the stack)
        s.AddField(key).SetValue( l_encode_value(L) );

        lua_pop(L, 1); // pops the value, now -1 => key, -2 => table (original); ready for next iteration!
    }

    return std::move(s);
}

static int l_mzmmp_encode(lua_State* L)
{
    if ( lua_istable(L, -1) )
    {
        S s = l_encode_table(L);

        PrintStruct(s);

        Buffer buffer;
        DataPolicyContainerType container;
        container.Setup( DataPolicyContainerPreloadType::Singleton().Retrieve() );
        SerializationOutputWrapperType output(container, buffer);
        s.Serialize(output);

        lua_pushlstring( L, (char*)buffer.data(), buffer.size() );

        return 1;
    }

    return 0;
}

static void l_decode_table(lua_State* L, const S& s);
static void l_decode_array(lua_State* L, const S::FieldType::ValueType::ArrayType& array);

class LuaValue
{
public:
    LuaValue(lua_State* L) : m_L(L) {}

    void operator()(bool value) const
    {
        lua_pushboolean(m_L, value ? 1 : 0);
    }

    void operator()(I64 value) const
    {
        lua_pushnumber(m_L, value);
    }

    void operator()(F64 value) const
    {
        lua_pushnumber(m_L, value);
    }

    void operator()(const String& value) const
    {
        lua_pushstring(m_L, value.c_str());
    }

    void operator()(const S& s) const
    {
        l_decode_table(m_L, s);
    }

    void operator()(const S::FieldType::ValueType::ArrayType& array)
    {
        l_decode_array(m_L, array);
    }

private:
    lua_State* m_L;
};

void l_decode_array(lua_State* L, const S::FieldType::ValueType::ArrayType& array)
{
    lua_newtable(L); // the array at the top (-1)
    // fill in the array elements
    for ( size_t i = 0; i < array.size(); ++i )
    {
        LuaValue visitor(L);
        static_cast<const S::FieldType::ValueType&>(array[i]).ConstApply(visitor); // pushes the value to the top of the stack, the array is now at index -2
        lua_rawseti(L, -2, (int)i+1);
    }
}

void l_decode_table(lua_State* L, const S& s)
{
    lua_newtable(L); // the table at index (-1)

    const auto& fields = s.GetFields();
    for (const auto& field : fields)
    {
        lua_pushstring( L, field.GetName().c_str() ); // key, now at the top (-1), but after the next line, at (-2)

        if ( field.HasValue() )
        {
            LuaValue visitor(L);
            field.GetValue().ConstApply(visitor); // the value is at -1, the key is at -2, the table is at -3
        }
        else
        {
            lua_pushnil(L);
        }
        lua_rawset(L, -3); // now the table is at the top (-1), the previous pushed key/array are popped!
    }
}

static int l_mzmmp_decode(lua_State* L)
{
    if ( lua_isstring(L, -1) )
    {
        size_t len = 0;
        const char* str = lua_tolstring(L, -1, &len);
        Buffer buffer(len);
        memcpy( buffer.data(), str, len );

        DataPolicyContainerType container;
        container.Setup( DataPolicyContainerPreloadType::Singleton().Retrieve() );
        SerializationInputWrapperType input(container, buffer);

        S s;
        s.Serialize(input);

        PrintStruct(s);

        l_decode_table(L, s);

        return 1;
    }

    return 0;
}

static int luaopen_mzmmp(lua_State* L)
{
    static const luaL_Reg mzmmp[] = {
        {"encode", l_mzmmp_encode},
        {"decode", l_mzmmp_decode},
        {NULL, NULL},
    };

    luaL_newlib(L, mzmmp);
    return 1;
}

static void TestLua()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_requiref(L, "mzmmp", luaopen_mzmmp, 1);

    luaL_dofile(L, "test.lua");

    lua_close(L);
}

///////////////////////////////////////////////////////////////////////////////

static S py_encode_object(PyObject* o);
static S::FieldType::ValueType::ArrayType py_encode_array(PyObject* a);

static S::FieldType::ValueType py_encode_value(PyObject* val)
{
    if ( PyBool_Check(val) )
    {
        return (bool)(val == Py_True);
    }
    else if ( PyLong_Check(val) )
    {
        return (I64)PyLong_AsLongLong(val);
    }
    else if ( PyFloat_Check(val) )
    {
        return (F64)PyFloat_AsDouble(val);
    }
    else if ( PyUnicode_Check(val) )
    {
        return (String)PyUnicode_AsUTF8(val);
    }
    else if ( PyList_Check(val) )
    {
        return py_encode_array(val);
    }
    else
    {
        return py_encode_object(val);
    }
}

S::FieldType::ValueType::ArrayType py_encode_array(PyObject* a)
{
    S::FieldType::ValueType::ArrayType array;

    Py_ssize_t sz = PyList_Size(a);
    for (Py_ssize_t i = 0; i < sz; ++i)
    {
        PyObject* item = PyList_GET_ITEM(a, i);
        array.push_back( py_encode_value(item) );
    }

    return std::move(array);
}

S py_encode_object(PyObject* o)
{
    S s;

    PyObject* attributes = PyObject_GetAttrString(o, "__dict__");
    if ( attributes && PyDict_Check(attributes) )
    {
        PyObject* key = NULL;
        PyObject* val = NULL;
        Py_ssize_t pos = 0;
        while ( PyDict_Next(attributes, &pos, &key, &val) )
        {
            if ( PyUnicode_Check(key) )
            {
                String k = PyUnicode_AsUTF8(key);
                s.AddField(k).SetValue( py_encode_value(val) );
            }
        }
    }

    return std::move(s);
}

static PyObject* Py_mzmmp_encode(PyObject* self, PyObject* args)
{
    PyObject* o = NULL;
    if ( !PyArg_ParseTuple(args, "O", &o) )
    {
        return NULL;
    }

    S s = py_encode_object(o);

    PrintStruct(s);

    Buffer buffer;
    DataPolicyContainerType container;
    container.Setup( DataPolicyContainerPreloadType::Singleton().Retrieve() );
    SerializationOutputWrapperType output(container, buffer);
    s.Serialize(output);

    return PyBytes_FromStringAndSize( (char*)buffer.data(), buffer.size() );
}

static PyObject* py_decode_object(const S& s);
static PyObject* py_decode_array(const S::FieldType::ValueType::ArrayType& array);

class PyValue
{
public:
    PyValue()
        : m_value(NULL)
    {}

    void operator()(bool value)
    {
        m_value = value ? Py_True : Py_False;
        Py_INCREF(m_value);
    }

    void operator()(I64 value)
    {
        m_value = PyLong_FromLongLong(value);
    }

    void operator()(F64 value)
    {
        m_value = PyFloat_FromDouble(value);
    }

    void operator()(const String& value)
    {
        m_value = PyUnicode_FromString( value.c_str() );
    }

    void operator()(const S& s)
    {
        m_value = py_decode_object(s);
    }

    void operator()(const S::FieldType::ValueType::ArrayType& array)
    {
        m_value = py_decode_array(array);
    }

    PyObject* Get() const
    {
        return m_value;
    }

private:
    PyObject* m_value;
};

PyObject* py_decode_array(const S::FieldType::ValueType::ArrayType& array)
{
    PyObject* pyList = PyList_New( array.size() );
    for ( size_t i = 0; i < array.size(); ++i )
    {
        PyValue visitor;
        static_cast<const S::FieldType::ValueType&>(array[i]).ConstApply(visitor);
        PyList_SET_ITEM( pyList, i, visitor.Get() ); // NB: this function "steals" the reference, so we don't have to explicitly decrement the reference on the result!
    }
    return pyList;
}

PyObject* py_decode_object(const S& s)
{
    PyObject* __main__ = PyImport_ImportModule("__main__");
    PyObject* C = PyObject_GetAttrString(__main__, "C");
    PyObject* o = PyObject_CallObject(C, NULL);
    Py_XDECREF(C);

    const auto& fields = s.GetFields();
    for (const auto& field : fields)
    {
        PyObject* pyValue = NULL;

        if ( field.HasValue() )
        {
            PyValue visitor;
            field.GetValue().ConstApply(visitor);
            pyValue = visitor.Get();
        }

        PyObject_SetAttrString( o, field.GetName().c_str(), pyValue ); // NB: PyObject_SetAttrString "borrows" reference, so we have to balance the ref count explicitly!
        Py_XDECREF(pyValue);
    }

    return o;
}

static PyObject* Py_mzmmp_decode(PyObject* self, PyObject* args)
{
    Py_ssize_t sz = -1;
    const char* bytes = NULL;
    if ( !PyArg_ParseTuple(args, "y#", &bytes, &sz) )
    {
        return NULL;
    }

    Buffer buffer(sz);
    memcpy( buffer.data(), bytes, sz );
    DataPolicyContainerType container;
    container.Setup( DataPolicyContainerPreloadType::Singleton().Retrieve() );
    SerializationInputWrapperType input(container, buffer);

    S s;
    s.Serialize(input);

    PrintStruct(s);

    return py_decode_object(s);
}

static PyMethodDef s_methods[] = {
    {"encode", Py_mzmmp_encode, METH_VARARGS, "encode"},
    {"decode", Py_mzmmp_decode, METH_VARARGS, "decode"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef s_module = {
    PyModuleDef_HEAD_INIT,
    "mzmmp",
    NULL,
    -1,
    s_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

static PyObject* PyInit_mzmmp()
{
    return PyModule_Create(&s_module);
}

static void TestPython()
{
    PyImport_AppendInittab("mzmmp", &PyInit_mzmmp);
    Py_Initialize();

    PyRun_SimpleString("class C: pass\n"); // NB: this adds class definition for "C" to the "__main__" module

    PyObject* path = Py_BuildValue("s", "test.py");
    FILE *f = _Py_fopen(path, "r");
    PyRun_SimpleFile(f, "test.py");

    Py_Finalize();
}

int main(int argc, const char * argv[])
{
    TestBitStream();

    TestSerialization();

    TestMapDataSerialization();

    TestVariant();

    TestMetaStruct();

    TestLua();

    TestPython();

    // insert code here...
    std::cout << "Hello, World!\n";
    return 0;
}

