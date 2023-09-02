#pragma once

#include "StdAfx.h"

struct DescriptorPoolEntry
{
    std::vector<RHIDescriptorSetPtr> sets;
    uint64_t version = 0;
    uint64_t used = 0;
};

// This class holds reusable descriptor sets. Not thread-safe. You must call Reset to be able to reuse descriptors.
// Descriptor sets allocated from the pool "expire" when you call Reset and must not be used after that. Sadly we don't have any checks that explicitly prevent that.
class DescriptorSetPool
{
private:

    uint64_t m_cur_version = 0;
    std::unordered_map<const RHIDescriptorSetLayout*, DescriptorPoolEntry> m_sets;

public:

    DescriptorSetPool() = default;

    void Reset() { m_cur_version++; }

    RHIDescriptorSet* Allocate( RHIDescriptorSetLayout& layout );
};