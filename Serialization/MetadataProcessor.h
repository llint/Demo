//
//  MetadataProcessor.h
//  SerializationFramework
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef SerializationFramework_MetadataProcessor_h
#define SerializationFramework_MetadataProcessor_h

#include "Types.h"

////////////////////////////////////////////////////////////////////////////////
// IMetadataProcessor abstracts away the actual format for the data policies definition
// Possible formats include XML, LUA, or even directly in the C++ code!

template <typename T>
struct NoDelete
{
    void operator()(T*) const {}
};

struct IMetadataProcessor
{
    typedef std::unique_ptr< IMetadataProcessor, NoDelete<IMetadataProcessor> > ptr;

    struct Element
    {
        String name;

        typedef std::unordered_map<String, String> Attributes;
        Attributes attributes;

        typedef std::list<Element> Elements;
        Elements children;
    };

    typedef std::list<Element> Elements;

    virtual const Elements& Retrieve() const = 0;

    virtual ~IMetadataProcessor() {}
};

#endif
