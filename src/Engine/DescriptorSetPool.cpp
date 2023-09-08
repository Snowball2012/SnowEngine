#include "StdAfx.h"

#include "DescriptorSetPool.h"

RHIDescriptorSet* DescriptorSetPool::Allocate( RHIDescriptorSetLayout& layout )
{
    DescriptorPoolEntry& layout_sets = m_sets[&layout];

    if ( layout_sets.version != m_cur_version )
    {
        layout_sets.version = m_cur_version;
        layout_sets.used = 0;
    }

    if ( layout_sets.used == layout_sets.sets.size() )
    {
        layout_sets.sets.emplace_back( GetRHI().CreateDescriptorSet( layout ) );
    }

    if ( layout_sets.used < layout_sets.sets.size() )
    {
        RHIDescriptorSet* available_set = layout_sets.sets[layout_sets.used].get();
        layout_sets.used++;
        return available_set;
    }

    SE_ENSURE( false );
    return nullptr;
}