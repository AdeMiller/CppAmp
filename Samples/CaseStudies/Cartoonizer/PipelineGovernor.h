//===============================================================================
//
// Microsoft Press
// C++ AMP: Accelerated Massive Parallelism with Microsoft Visual C++
//
//===============================================================================
// Copyright (c) 2012-2013 Ade Miller & Kate Gregory.  All rights reserved.
// This code released under the terms of the 
// Microsoft Public License (Ms-PL), http://ampbook.codeplex.com/license.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//===============================================================================

#pragma once

#include <agents.h>

using namespace ::concurrency;

// A pipeline has a fixed capacity of in-flight elements. The PipelineGovernor class
// implements a messaged-based throttling mechanism that is similar in functionality
// to a semaphore but uses messages instead of shared memory.
//
// The last stage of the pipeline should call FreePipelineSlot() each time it finishes processing 
// an element.
//
// The first stage of the pipeline should call WaitForAvailablePipelineSlot() before forwarding
// each new element to the next stage of the pipeline.
//
// The first stage of the pipeline should call WaitForEmptyPipeline() on shutdown before it
// reclaims memory for pipeline stages.
//
// Note that the governor does not use interlocked operations when updating or reading m_count. 
// this may result in race conditions leading to off by one errors (but no torn reads or writes as
// short is smaller than 32 bits). This isn;t an issue for the pipeline small rounding errors in the 
// amount of work in progress whill not  alter its performance.

class PipelineGovernor
{
private:
    struct signal {};

    const short m_capacity;
    short m_count;
    unbounded_buffer<signal> m_completedItem;

public:
    PipelineGovernor(int capacity) :
        m_count(0), m_capacity(capacity) 
    {}

    // Only called by last stage of pipeline. As items complete it signals the event.

    void FreePipelineSlot() 
    {
        send(m_completedItem, signal());
    }

    // Only called by first pipeline stage
    
    void WaitForAvailablePipelineSlot() 
    {
        if (m_count < m_capacity)
            ++m_count;
        else
            receive(m_completedItem);
    }

    // Only called by first pipeline stage. If there are items in the pipeline it
    // waits for a completed item message before checking the pipeline again.
    
    void WaitForEmptyPipeline() 
    {
        while(m_count > 0)
        {
            --m_count;
            receive(m_completedItem);
        }
    }

private:
    // Disable copy constructor and assignment.
    PipelineGovernor(const PipelineGovernor&);
    PipelineGovernor const & operator=(PipelineGovernor const&);
};
