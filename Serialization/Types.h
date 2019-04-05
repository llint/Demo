//
//  Types.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_Types_h
#define SerializationFramework_Types_h

#include <cmath>
#include <cstring>

#include <map>
#include <set>
#include <list>
#include <tuple>
#include <deque>
#include <vector>
#include <string>
#include <memory>
#include <limits>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include <inttypes.h>

typedef int8_t  I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef float  F32;
typedef double F64;

typedef int          __I;
typedef unsigned int __U;

typedef unsigned char Byte;
typedef std::vector<Byte> Buffer;

//typedef std::string String;

#endif
