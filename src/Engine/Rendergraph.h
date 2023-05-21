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

    // Has to be called, when all command lists are written for that pass
    void EndPass();

    // If true, you can reuse command list from previous pass. You can only borrow one last command list, and you still have to call EndPass at the end of the pass
    // This allows to avoid having a separate command list for short consequent passes.
    // It can be used like this:
    // prev_pass->AddCommandList( list );
    // prev_pass->EndPass();
    // curr_pass->BorrowCommmandList( list );
    // curr_pass->AddCommandList( some_other_list ); // optional
    // curr_pass->EndPass();
    bool CanBorrowCommandListFromPreviousPass() const { NOTIMPL; return false; }
    void BorrowCommandList( RHICommandList& cmd_list ) { NOTIMPL; }
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