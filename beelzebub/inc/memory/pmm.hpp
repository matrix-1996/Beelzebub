/*
    Copyright (c) 2016 Alexandru-Mihai Maftei. All rights reserved.


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

/**
 *  The implementation of the physical memory manager is largely architecture-
 *  specific.
 */

#pragma once

#include <beel/handles.h>
#include <beel/enums.kernel.hpp>
#include <beel/structs.kernel.hpp>

namespace Beelzebub { namespace Memory
{
    /**
     *  The physical memory manager.
     */
    class Pmm
    {
    public:
        /*  Frame operations  */

        static __hot __solid paddr_t AllocateFrame(FrameSize size = FrameSize::_4KiB, AddressMagnitude magn = AddressMagnitude::Any, uint32_t refCnt = 0);

        static __hot __forceinline paddr_t AllocateFrame(AddressMagnitude magn, FrameSize size = FrameSize::_4KiB, uint32_t refCnt = 0)
        { return AllocateFrame(size, magn, refCnt); }

        static __hot __forceinline paddr_t AllocateFrame(uint32_t refCnt, FrameSize size = FrameSize::_4KiB, AddressMagnitude magn = AddressMagnitude::Any)
        { return AllocateFrame(size, magn, refCnt); }

        static __hot __forceinline paddr_t AllocateFrame(uint32_t refCnt, AddressMagnitude magn, FrameSize size = FrameSize::_4KiB)
        { return AllocateFrame(size, magn, refCnt); }

        static __hot __solid Handle FreeFrame(paddr_t addr, bool ignoreRefCnt = true);
        static __cold __solid Handle ReserveRange(paddr_t start, size_t size, bool includeBusy = false);

        static __hot __solid Handle AdjustReferenceCount(paddr_t addr, uint32_t & newCnt, int32_t diff);

        static __hot __forceinline Handle AdjustReferenceCount(paddr_t addr, int32_t diff)
        {
            uint32_t dummy;

            return AdjustReferenceCount(addr, dummy, diff);
        }

        static __solid Handle GetFrameInfo(paddr_t addr, FrameSize & size, uint32_t & refCnt);
    };
}}
