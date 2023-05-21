#include "StdAfx.h"

#include "Rendergraph.h"

void RendergraphExample()
{
    Rendergraph rendergraph;
    // 1. Setup stage. Setup your resource handles and passes

    RendergraphPass* rt_pass = rendergraph.AddPass();

    RendergraphPass* copy_to_swapchain = rendergraph.AddPass();

    // 2. Compile stage. That will create a timeline for each resource, allowing us to fetch real RHI handles for them inside passes
    rendergraph.Compile();

    // 3. Fill passes with command lists

    RHICommandList* cmd_list = GetRHI().GetCommandList( RHI::QueueType::Graphics );

    rt_pass->AddCommandList( *cmd_list );

    copy_to_swapchain->AddCommandList( *cmd_list );

    // 4. Submit.
    rendergraph.Submit();
}

RendergraphPass* Rendergraph::AddPass()
{
    NOTIMPL;
    return nullptr;
}

bool Rendergraph::Compile()
{
    NOTIMPL;
    return false;
}

bool Rendergraph::Submit()
{
    for ( const auto& submission : m_submissions )
    {
        RHI::SubmitInfo rhi_submission = {};
        size_t cmd_lists_num = 0;
        for ( const auto* pass : submission.passes )
        {
            cmd_lists_num += pass->m_cmd_lists.size();
        }

        std::vector<RHICommandList*> cmd_lists;
        cmd_lists.reserve( cmd_lists_num );
        for ( const auto* pass : submission.passes )
        {
            cmd_lists.insert( cmd_lists.end(), pass->m_cmd_lists.begin(), pass->m_cmd_lists.end() );
        }

        rhi_submission.cmd_lists = cmd_lists.data();
        rhi_submission.cmd_list_count = cmd_lists.size();


        RHIFence fence = GetRHI().SubmitCommandLists( rhi_submission );
    }
    return true;
}

void RendergraphPass::AddResource( RendergraphResource& resource )
{
    NOTIMPL;
}

void RendergraphPass::AddCommandList( RHICommandList& cmd_list )
{
    m_cmd_lists.emplace_back( &cmd_list );
}
