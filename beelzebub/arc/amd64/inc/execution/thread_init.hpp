#pragma once

#include <execution/thread.hpp>
#include <execution/thread_state.hpp>
#include <handles.h>
#include <metaprogramming.h>

using namespace Beelzebub::Memory;

namespace Beelzebub { namespace Execution
{
    __bland void InitializeThreadState(Thread * const thread);

    __cold __bland __noinline Handle InitializeBootstrapThread(Thread * const bst, Process * const bsp, MemoryManager * const bsmm);
}}
