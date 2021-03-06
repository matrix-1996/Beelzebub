/*
    Copyright (c) 2015 Alexandru-Mihai Maftei. All rights reserved.


    Developed by: Alexandru-Mihai Maftei
    aka Vercas
    http://vercas.com | https://github.com/vercas/Beelzebub

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal with the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

      * Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimers.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimers in the
        documentation and/or other materials provided with the distribution.
      * Neither the names of Alexandru-Mihai Maftei, Vercas, nor the names of
        its contributors may be used to endorse or promote products derived from
        this Software without specific prior written permission.


    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    WITH THE SOFTWARE.

    ---

    You may also find the text of this license in "LICENSE.md", along with a more
    thorough explanation regarding other files.
*/

#include "system/syscalls.hpp"
#include "syscalls.kernel.hpp"
#include "system/msrs.hpp"
#include "entry.h"

#include <beel/sync/smp.lock.hpp>
#include <beel/syscalls/memory.h>

using namespace Beelzebub;
using namespace Beelzebub::Syscalls;
using namespace Beelzebub::System;

static Synchronization::SmpLock InitLock {};
static bool Initialized = false;

__thread uintptr_t System::SyscallStack;
__thread uintptr_t System::SyscallUserlandStack;
__thread SyscallRegisters64 System::SyscallRegisters;

/********************
    Syscall class
********************/

void Syscall::Initialize()
{
    Msrs::SetFmask(FlagsRegisterFlags::Reserved1);
    Msrs::SetStar(Ia32Star().SetSyscallCsSs(0x8).SetSysretCsSs(0x18));

    Msrs::SetLstar(&SyscallEntry_64);

    if (BootstrapCpuid.Vendor == CpuVendor::Intel)
        Msrs::SetEfer(Msrs::GetEfer().SetSyscallEnable(true));

    withLock (InitLock)
    {
        if likely(!Initialized)
        {
#define SET_SYSCALL(enum, func) \
            DefaultSystemCalls[(size_t)SyscallSelection::enum] = &func

            SET_SYSCALL(MemoryRequest, MemoryRequest);
            SET_SYSCALL(MemoryRelease, MemoryRelease);
            SET_SYSCALL(MemoryCopy   , MemoryCopy);
            SET_SYSCALL(MemoryFill   , MemoryFill);

            Initialized = true;
        }
    }
}
