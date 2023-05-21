#pragma once

void RendergraphExample();

class RHICommandList;

class RendergraphResource
{
};

struct RendergraphResourceUsageEntry
{
    RendergraphResource* resource = nullptr;
    // some usage info
};

class RendergraphPass
{
    friend class Rendergraph;

    std::vector<RendergraphResource*> m_used_resources;
    std::vector<RHICommandList*> m_cmd_lists;

public:
    void AddResource( RendergraphResource& resource );

    void AddCommandList( RHICommandList& cmd_list );
};


struct RendergraphSubmission
{
    std::vector<RendergraphPass*> passes;
    RHI::QueueType type;
};

class Rendergraph
{
    std::vector<RendergraphSubmission> m_submissions;

public:
    RendergraphPass* AddPass();

    bool Compile();

    bool Submit();
};