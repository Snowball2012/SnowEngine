#include "StdAfx.h"

#include "Serialization.h"

namespace Serialization
{
    bool Deserialize( const JsonValue& serialized, float& value )
    {
        if ( !serialized.IsFloat() )
        {
            SE_LOG_ERROR( Serialization, "value is not a float" );
            return false;
        }

        value = serialized.GetFloat();

        return true;
    }

    template<typename T>
    void SerializeVec( const T& value, JsonValue& out, JsonAllocator& allocator )
    {
        out.SetArray();
        for ( int i = 0; i < T::length(); ++i )
        {
            out.PushBack( value[i], allocator );
        }
    }

    template<typename T>
    bool DeserializeVec( const JsonValue& serialized, T& value )
    {
        static constexpr int dimensions = T::length();
        if ( !serialized.IsArray() )
        {
            SE_LOG_ERROR( Serialization, "vec%d value is not a %d element array", dimensions, dimensions );
            return false;
        }

        auto ser_array = serialized.GetArray();
        if ( ser_array.Size() != dimensions )
        {
            SE_LOG_ERROR( Serialization, "vec%d value is not a %d element array", dimensions, dimensions );
            return false;
        }

        for ( int i = 0; i < dimensions; ++i )
        {
            if ( !Deserialize( ser_array[i], value[i] ) )
            {
                SE_LOG_ERROR( Serialization, "vec%d %d coordinate is invalid", dimensions, i );
                return false;
            }
        }

        return true;
    }

    void Serialize( const glm::vec2& value, JsonValue& out, JsonAllocator& allocator )
    {
        SerializeVec( value, out, allocator );
    }

    bool Deserialize( const JsonValue& serialized, glm::vec2& value )
    {
        return DeserializeVec( serialized, value );
    }

    void Serialize( const glm::vec3& value, JsonValue& out, JsonAllocator& allocator )
    {
        SerializeVec( value, out, allocator );
    }

    bool Deserialize( const JsonValue& serialized, glm::vec3& value )
    {
        return DeserializeVec( serialized, value );
    }

    void Serialize( const glm::vec4& value, JsonValue& out, JsonAllocator& allocator )
    {
        SerializeVec( value, out, allocator );
    }

    bool Deserialize( const JsonValue& serialized, glm::vec4& value )
    {
        return DeserializeVec( serialized, value );
    }

    void Serialize( const glm::quat& value, JsonValue& out, JsonAllocator& allocator )
    {
        SerializeVec( value, out, allocator );
    }

    bool Deserialize( const JsonValue& serialized, glm::quat& value )
    {
        return DeserializeVec( serialized, value );
    }

    void Serialize( const Transform& value, JsonValue& out, JsonAllocator& allocator )
    {
        out.SetObject();

        // @todo - add helper serializers for vectors
        JsonValue translation;
        Serialization::Serialize( value.translation, translation, allocator );

        JsonValue orientation;
        Serialization::Serialize( value.orientation, orientation, allocator );

        JsonValue scale;
        Serialization::Serialize( value.scale, scale, allocator );

        out.AddMember( "t", translation, allocator );
        out.AddMember( "o", orientation, allocator );
        out.AddMember( "s", scale, allocator );
    }

    bool Deserialize( const JsonValue& serialized, Transform& value )
    {
        if ( !serialized.IsObject() )
        {
            SE_LOG_ERROR( Serialization, "transform value is not a json object" );
            return false;
        }

        JsonValue::ConstMemberIterator translation = serialized.FindMember( "t" );
        if ( translation == serialized.MemberEnd()
            || ( Deserialize( translation->value, value.translation ) == false ) )
        {
            SE_LOG_ERROR( Serialization, "translation invalid" );
            return false;
        }

        JsonValue::ConstMemberIterator orientation = serialized.FindMember( "o" );
        if ( orientation == serialized.MemberEnd()
            || ( Deserialize( orientation->value, value.orientation ) == false ) )
        {
            SE_LOG_ERROR( Serialization, "orientation invalid" );
            return false;
        }

        JsonValue::ConstMemberIterator scale = serialized.FindMember( "s" );
        if ( scale == serialized.MemberEnd()
            || ( Deserialize( scale->value, value.scale ) == false ) )
        {
            SE_LOG_ERROR( Serialization, "scale invalid" );
            return false;
        }

        return true;
    }
}