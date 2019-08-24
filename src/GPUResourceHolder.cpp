// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "stdafx.h"

#include "GPUResourceHolder.h"


GPUResourceHolder::~GPUResourceHolder()
{
    Clear();
}


void GPUResourceHolder::Merge( GPUResourceHolder&& other )
{
    m_resources.reserve( m_resources.size() + other.m_resources.size() );
    m_allocators.reserve( m_allocators.size() + other.m_allocators.size() );

    m_resources.insert( m_resources.end(),
                        std::make_move_iterator( other.m_resources.begin() ),
                        std::make_move_iterator( other.m_resources.end() ) );
    m_allocators.insert( m_allocators.end(),
                         std::make_move_iterator( other.m_allocators.begin() ),
                         std::make_move_iterator( other.m_allocators.end() ) );
}


void GPUResourceHolder::AddAllocators( const span<GPULinearAllocator>& allocators )
{
    m_allocators.reserve( m_allocators.size() + allocators.size() );
    for ( auto& allocator : allocators )
        m_allocators.emplace_back( std::move( allocator ) );
}


void GPUResourceHolder::AddResources( const span<ComPtr<ID3D12Resource>>& resources )
{
    m_resources.reserve( m_resources.size() + resources.size() );
    for ( auto& resource : resources )
        m_resources.emplace_back( std::move( resource ) );
}

void GPUResourceHolder::Clear() noexcept
{
    m_resources.clear();
    m_allocators.clear();
}
