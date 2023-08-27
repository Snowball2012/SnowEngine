#pragma once

#include "StdAfx.h"

using Json = rapidjson::Document;
using JsonValue = rapidjson::Value;
using JsonAllocator = decltype( ( ( Json* )( 0 ) )->GetAllocator() ); 

// Each serializable class can make its own implemention of Serialize/Deserialize functions and put them in Serialization namespace
namespace Serialization
{
    bool Deserialize( const JsonValue& serialized, float& value );

    void Serialize( const glm::vec2& value, JsonValue& out, JsonAllocator& allocator );
    bool Deserialize( const JsonValue& serialized, glm::vec2& value );

    void Serialize( const glm::vec3& value, JsonValue& out, JsonAllocator& allocator );
    bool Deserialize( const JsonValue& serialized, glm::vec3& value );

    void Serialize( const glm::vec4& value, JsonValue& out, JsonAllocator& allocator );
    bool Deserialize( const JsonValue& serialized, glm::vec4& value );

    void Serialize( const glm::quat& value, JsonValue& out, JsonAllocator& allocator );
    bool Deserialize( const JsonValue& serialized, glm::quat& value );

    void Serialize( const Transform& value, JsonValue& out, JsonAllocator& allocator );
    bool Deserialize( const JsonValue& serialized, Transform& value );
}

SE_LOG_CATEGORY( Serialization );