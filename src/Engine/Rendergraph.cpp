#include "StdAfx.h"

#include "Rendergraph.h"

void RendergraphExample()
{
    Rendergraph rendergraph;
    // 1. Setup stage. Setup your resource handles and passes

    RendergraphResource* swapchain = rendergraph.RegisterExternalTexture();
    RendergraphResource* final_frame = rendergraph.CreateTransientTexture();

    RendergraphResource* tlas = rendergraph.RegisterExternalAS();
    RendergraphResource* scratch = rendergraph.CreateTransientBuffer();

    RendergraphPass* update_as_pass = rendergraph.AddPass();
    update_as_pass->AddResource( *tlas );
    update_as_pass->AddResource( *scratch );

    RendergraphPass* rt_pass = rendergraph.AddPass();
    rt_pass->AddResource( *tlas );
    rt_pass->AddResource( *final_frame );

    RendergraphPass* copy_to_swapchain = rendergraph.AddPass();
    copy_to_swapchain->AddResource( *swapchain );
    copy_to_swapchain->AddResource( *final_frame );

    // 2. Compile stage. That will create a timeline for each resource, allowing us to fetch real RHI handles for them inside passes
    rendergraph.Compile();

    // 3. Record phase. Fill passes with command lists

    RHICommandList* cmd_list_rt = GetRHI().GetCommandList( RHI::QueueType::Graphics );

    rt_pass->AddCommandList( *cmd_list_rt );
    rt_pass->EndPass();

    copy_to_swapchain->BorrowCommandList( *cmd_list_rt );
    copy_to_swapchain->EndPass();

    // Passes can be filled out of order
    RHICommandList* cmd_list_update_as = GetRHI().GetCommandList( RHI::QueueType::Graphics );
    update_as_pass->AddCommandList( *cmd_list_update_as );
    update_as_pass->EndPass();

    // 4. Submit stage.
    rendergraph.Submit();
}

RendergraphPass* Rendergraph::AddPass()
{
    NOTIMPL;
    return nullptr;
}

RendergraphResource* Rendergraph::RegisterExternalTexture()
{
    NOTIMPL;
    return nullptr;
}

RendergraphResource* Rendergraph::CreateTransientTexture()
{
    NOTIMPL;
    return nullptr;
}

RendergraphResource* Rendergraph::RegisterExternalAS()
{
    NOTIMPL;
    return nullptr;
}

RendergraphResource* Rendergraph::CreateTransientBuffer()
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

void RendergraphPass::EndPass()
{
    NOTIMPL;
}
