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

/**
 *  Bit 11 of all entries is used as a spinlock!
 */

#pragma once

#include <utils/bitfields.hpp>

//  Creates the functions necessary for operating a spinlock over an entry.
#define SPINLOCK(name, bit)                                  \
__forceinline bool MCATS2(TryAcquire, name)()                \
{                                                            \
    bool res = 0;                                            \
                                                             \
    asm volatile ("lock btsq $" #bit ", %[val] \n\t"         \
                  "setnc %[res] \n\t"                        \
                 : [res] "=r" (res)                          \
                 : [val] "m" (this->Value)                   \
                 : "cc");                                    \
                                                             \
    return res;                                              \
}                                                            \
__forceinline void MCATS2(Spin, name)()                      \
{                                                            \
    do                                                       \
    {                                                        \
        asm volatile ("pause");                              \
    } while (!this->MCATS2(Check, name)());                  \
}                                                            \
__forceinline void MCATS2(Await, name)()                     \
{                                                            \
    while (!this->MCATS2(Check, name)())                     \
    {                                                        \
        asm volatile ("pause");                              \
    }                                                        \
}                                                            \
__forceinline void MCATS2(Acquire, name)()                   \
{                                                            \
    while (!this->MCATS2(TryAcquire, name)())                \
        this->MCATS2(Spin, name)();                          \
}                                                            \
__forceinline void MCATS2(Release, name)()                   \
{                                                            \
    asm volatile ("lock btrq $" #bit ", %[val] \n\t"         \
                 :                                           \
                 : [val] "m" (this->Value)                   \
                 : "cc");                                    \
}                                                            \
__forceinline bool MCATS2(Check, name)()                     \
{                                                            \
    return 0 == (this->Value & (1 << bit));                  \
    /*  A simple AND operation works very well here.  */     \
}

namespace Beelzebub { namespace Memory
{
    /**
     *  Page Map Level Entries' common aspects.
     */
    struct PmlCommonEntry
    {
        /*  Bit structure for paging table entries:
         *       0       : Present (if 1)
         *       1       : R/W (1 means writes allowed)
         *       2       : U/S (1 if usermode access allowed)
         *       3       : PWT (page-level write-through)
         *       4       : PCD (page-level cache disable)
         *       5       : Accessed
         *       6       : Dirty
         *       7       : PAT
         *       8       : Global
         *       9 -  11 : Ignored
         *      12 - M-1 : Physical address of aligned 4-KiB page;
         *       M -  51 : Reserved (must be 0)
         *      52 -  62 : Ignored
         *      63       : XD (eXecute Disable, if 1)
         */

        /*  Properties  */

        BITFIELD_DEFAULT_1W( 0, Present )
        BITFIELD_DEFAULT_1W( 1, Writable)
        BITFIELD_DEFAULT_1W( 2, Userland)
        BITFIELD_DEFAULT_1W( 3, Pwt     )
        BITFIELD_DEFAULT_1W( 4, Pcd     )
        BITFIELD_DEFAULT_1W( 5, Accessed)
        BITFIELD_DEFAULT_1W( 6, Dirty   )
        BITFIELD_DEFAULT_1W( 7, Pat1    )
        BITFIELD_DEFAULT_1W( 7, PageSize)
        BITFIELD_DEFAULT_1W( 8, Global  )
        BITFIELD_DEFAULT_1W(12, Pat2    )
        BITFIELD_DEFAULT_1W(63, Xd      )
        BITFIELD_DEFAULT_2W(12, 40, paddr_t, Address)
        
        /*  Constructors  */

        /**
         *  Creates an empty PML entry structure.
         */
        inline PmlCommonEntry() : Value( 0ULL ) { }

        /**
         *  Creates a new PML entry structure that references a 4-KiB page.
         */
        inline PmlCommonEntry(paddr_t const paddr
                            , bool    const present
                            , bool    const writable
                            , bool    const userAccessible
                            , bool    const global
                            , bool    const xd)
            : Value((paddr & AddressBits)
                  | (present        ? PresentBit  : 0ULL)
                  | (writable       ? WritableBit : 0ULL)
                  | (userAccessible ? UserlandBit : 0ULL)
                  | (global         ? GlobalBit   : 0ULL)
                  | (xd             ? XdBit       : 0ULL))
        {

        }

        /**
         *  Creates a new PML entry structure that references a 4-KiB page.
         */
        inline PmlCommonEntry(paddr_t const paddr
                            , bool    const present
                            , bool    const writable
                            , bool    const userAccessible
                            , bool    const pwt
                            , bool    const pcd
                            , bool    const accessed
                            , bool    const dirty
                            , bool    const global
                            , bool    const pat1
                            , bool    const pat2
                            , bool    const xd)
            : Value((paddr & AddressBits)
                  | (present        ? PresentBit  : 0ULL)
                  | (writable       ? WritableBit : 0ULL)
                  | (userAccessible ? UserlandBit : 0ULL)
                  | (pwt            ? PwtBit      : 0ULL)
                  | (pcd            ? PcdBit      : 0ULL)
                  | (accessed       ? AccessedBit : 0ULL)
                  | (dirty          ? DirtyBit    : 0ULL)
                  | (pat1           ? Pat1Bit     : 0ULL)
                  | (global         ? GlobalBit   : 0ULL)
                  | (pat2           ? Pat2Bit     : 0ULL)
                  | (xd             ? XdBit       : 0ULL))
        {

        }

        /*  Properties  */

        inline bool IsNull()
        {
            return (this->GetAddress() == nullpaddr) && !this->GetPresent();
        }

        /*  Synchronization  */

        SPINLOCK(ContentLock, 10);
        SPINLOCK(PropertiesLock, 11);

    //private:

        uint64_t Value;
    };

    /**
     *  Page Map Level 1 Entry
     */
    struct Pml1Entry : PmlCommonEntry
    {
        /*  Bit structure for 4-KiB page:
         *       0       : Present (if 1)
         *       1       : R/W (1 means writes allowed)
         *       2       : U/S (1 if usermode access allowed)
         *       3       : PWT (page-level write-through)
         *       4       : PCD (page-level cache disable)
         *       5       : Accessed
         *       6       : Dirty
         *       7       : PAT
         *       8       : Global
         *       9 -  11 : Ignored
         *      12 - M-1 : Physical address of aligned 4-KiB page;
         *       M -  51 : Reserved (must be 0)
         *      52 -  62 : Ignored
         *      63       : XD (eXecute Disable, if 1)
         */

        /*  Constructors  */

        /**
         *  Creates an empty PML1 (PT) entry structure.
         */
        inline Pml1Entry() : PmlCommonEntry() { }

        /**
         *  Creates a new PML1 (PT) entry structure that maps a 4-KiB page.
         */
        inline Pml1Entry(paddr_t const paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const global
                       , bool    const XD)
            : PmlCommonEntry(paddr, present, writable, userAccessible, global
                           , XD)
        {

        }

        /**
         *  Creates a new PML1 (PT) entry structure that maps a 4-KiB page.
         */
        inline Pml1Entry(paddr_t const paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const PWT
                       , bool    const PCD
                       , bool    const accessed
                       , bool    const dirty
                       , bool    const global
                       , bool    const PAT
                       , bool    const XD)
            : PmlCommonEntry(paddr, present, writable, userAccessible, PWT, PCD
                           , accessed, dirty, global, PAT, false, XD)
        {

        }
    };
    typedef Pml1Entry PtEntry;

    /**
     *  Page Map Level 1: Page Table.
     */
    struct Pml1
    {
        /*
         *  This table is indexed by bits 12-20 from the linear address.
         *  Naturally, bits 0-2 of the index are 0. (8-byte entries)
         */

        static const uint64_t AddressBits   = 0x00000000001FF000ULL;
        static const uint64_t IndexOffset   = 12;

        /*  Constructors  */

        Pml1() = default;
        Pml1(Pml1 const &) = default;

        /*  Operators  */

        /**
         *  Gets the entry at a given index.
         */
        inline Pml1Entry & operator [](uint16_t const ind)
        {
            return this->Entries[ind];
        }

        /**
         *  Gets the entry corresponding to the given linear address.
         */
        inline Pml1Entry & operator [](void const * const vaddr)
        {
            //  Take the interesting bits from the linear address...
            //  Shift it by the required amount of bits...
            //  And use that as an index! :D
            return this->Entries[(reinterpret_cast<uintptr_t>(vaddr) & AddressBits) >> IndexOffset];
        }

        /*  Field(s)  */

    //private:

        Pml1Entry Entries[512]; //  Yeah...
    };
    typedef Pml1 Pt;


    /**
     *  Page Map Level 2 Entry
     */
    struct Pml2Entry : PmlCommonEntry
    {
        /*  Bit structure for 2-MiB page:
         *       0       : Present (if 1)
         *       1       : R/W (1 means writes allowed)
         *       2       : U/S (1 if usermode access allowed)
         *       3       : PWT (page-level write-through)
         *       4       : PCD (page-level cache disable)
         *       5       : Accessed
         *       6       : Dirty
         *       7       : PageSize (must be 1)
         *       8       : Global
         *       9 -  11 : Ignored
         *      12       : PAT
         *      13 -  20 : Reserved (must be 0)
         *      21 - M-1 : Physical address of aligned 2-MiB page;
         *       M -  51 : Reserved (must be 0)
         *      52 -  62 : Ignored
         *      63       : XD (eXecute Disable, if 1)
         *
         *  Bit structure for PML1 (PT) reference:
         *       0       : Present (if 1)
         *       1       : R/W (1 means writes allowed)
         *       2       : U/S (1 if usermode access allowed)
         *       3       : PWT (page-level write-through)
         *       4       : PCD (page-level cache disable)
         *       5       : Accessed
         *       6       : Ignored
         *       7       : PageSize (must be 0)
         *       8 -  11 : Ignored
         *      12 - M-1 : Physical address of PML1 (PT) table; 4-KiB aligned.
         *       M -  51 : Reserved (must be 0)
         *      52 -  62 : Ignored
         *      63       : XD (eXecute Disable, if 1)
         */

        /*  Properties  */

        BITFIELD_DEFAULT_2W(12, 40, Pml1 * , Pml1Ptr)
        BITFIELD_DEFAULT_2W(21, 31, paddr_t, PageAddress)

        /*  Constructors  */

        /**
         *  Creates an empty PML2 (PD) entry structure.
         */
        inline Pml2Entry() : PmlCommonEntry() { }

        /**
         *  Creates a new PML2 (PD) entry structure that points to a PML1 (PT) table.
         */
        inline Pml2Entry(paddr_t const pml1_paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const XD)
            : PmlCommonEntry(pml1_paddr, present, writable, userAccessible
                           , false, XD)
        {

        }

        /**
         *  Creates a new PML2 (PD) entry structure that points to a PML1 (PT) table.
         */
        inline Pml2Entry(paddr_t const pml1_paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const PWT
                       , bool    const PCD
                       , bool    const accessed
                       , bool    const XD)
            : PmlCommonEntry(pml1_paddr, present, writable, userAccessible, PWT, PCD
                           , accessed, false, false, false, false, XD)
        {

        }

        /**
         *  Creates a new PML2 (PD) entry structure that maps a 2-MiB page.
         */
        inline Pml2Entry(paddr_t const paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const global
                       , bool    const XD)
            : PmlCommonEntry(paddr, present, writable, userAccessible, false, false
                           , false, false, global, true, false, XD)
        {

        }

        /**
         *  Creates a new PML2 (PD) entry structure that maps a 2-MiB page.
         */
        inline Pml2Entry(paddr_t const paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const PWT
                       , bool    const PCD
                       , bool    const accessed
                       , bool    const dirty
                       , bool    const global
                       , bool    const pat
                       , bool    const XD)
            : PmlCommonEntry(paddr, present, writable, userAccessible, PWT, PCD
                           , accessed, dirty, global, true, pat, XD)
        {

        }
    };
    typedef Pml2Entry PdEntry;

    /**
     *  Page Map Level 2: Page Directory.
     */
    struct Pml2
    {
        /*
         *  This table is indexed by bits 21-29 from the linear address.
         *  Naturally, bits 0-2 of the index are 0. (8-byte entries)
         */

        static const uint64_t AddressBits   = 0x000000003FE00000ULL;
        static const uint64_t IndexOffset   = 21;

        /*  Constructors  */

        Pml2() = default;
        Pml2(Pml2 const &) = default;

        /*  Operators  */

        /**
         *  Gets the entry at a given index.
         */
        inline Pml2Entry & operator [](uint16_t const ind)
        {
            return this->Entries[ind];
        }

        /**
         *  Gets the entry corresponding to the given linear address.
         */
        inline Pml2Entry & operator [](void const * const vaddr)
        {
            //  Take the interesting bits from the linear address...
            //  Shift it by the required amount of bits...
            //  And use that as an index! :D
            return this->Entries[(reinterpret_cast<uintptr_t>(vaddr) & AddressBits) >> IndexOffset];
        }

        /*  Field(s)  */

    //private:

        Pml2Entry Entries[512]; //  Yeah...
    };
    typedef Pml2 Pd;


    /**
     *  Page Map Level 3 Entry
     */
    struct Pml3Entry : PmlCommonEntry
    {
        /*  Bit structure for 1-GiB page:
         *       0       : Present (if 1)
         *       1       : R/W (1 means writes allowed)
         *       2       : U/S (1 if usermode access allowed)
         *       3       : PWT (page-level write-through)
         *       4       : PCD (page-level cache disable)
         *       5       : Accessed
         *       6       : Dirty
         *       7       : PageSize (must be 1)
         *       8       : Global
         *       9 -  11 : Ignored
         *      12       : PAT
         *      13 -  29 : Reserved (must be 0)
         *      30 - M-1 : Physical address of aligned 1-GiB page.
         *       M -  51 : Reserved (must be 0)
         *      52 -  62 : Ignored
         *      63       : XD (eXecute Disable, if 1)
         *
         *  Bit structure for PML2 reference:
         *       0       : Present (if 1)
         *       1       : R/W (1 means writes allowed)
         *       2       : U/S (1 if usermode access allowed)
         *       3       : PWT (page-level write-through)
         *       4       : PCD (page-level cache disable)
         *       5       : Accessed
         *       6       : Ignored
         *       7       : PageSize (must be 0)
         *       8 -  11 : Ignored
         *      12 - M-1 : Physical address of PML2 (PD) table; 4-KiB aligned.
         *       M -  51 : Reserved (must be 0)
         *      52 -  62 : Ignored
         *      63       : XD (eXecute Disable, if 1)
         */

        /*  Properties  */

        BITFIELD_DEFAULT_2W(12, 40, Pml2 * , Pml2Ptr)
        BITFIELD_DEFAULT_2W(30, 22, paddr_t, PageAddress)

        /*  Constructors  */

        /**
         *  Creates an empty PML3 (PDPT) entry structure.
         */
        inline Pml3Entry() : PmlCommonEntry() { }

        /**
         *  Creates a new PML3 (PDPT) entry structure that points to a PML2 (PD) table.
         */
        inline Pml3Entry(paddr_t const pml2_paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const XD)
            : PmlCommonEntry(pml2_paddr, present, writable, userAccessible
                           , false, XD)
        {

        }

        /**
         *  Creates a new PML3 (PDPT) entry structure that points to a PML2 (PD) table.
         */
        inline Pml3Entry(paddr_t const pml2_paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const PWT
                       , bool    const PCD
                       , bool    const accessed
                       , bool    const XD)
            : PmlCommonEntry(pml2_paddr, present, writable, userAccessible, PWT, PCD
                           , accessed, false, false, false, false, XD)
        {

        }

        /**
         *  Creates a new PML3 (PDPT) entry structure that maps a 1-GiB page.
         */
        inline Pml3Entry(paddr_t const paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const global
                       , bool    const XD)
            : PmlCommonEntry(paddr, present, writable, userAccessible, false, false
                           , false, false, global, true, false, XD)
        {

        }

        /**
         *  Creates a new PML3 (PDPT) entry structure that maps a 1-GiB page.
         */
        inline Pml3Entry(paddr_t const paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const PWT
                       , bool    const PCD
                       , bool    const accessed
                       , bool    const dirty
                       , bool    const global
                       , bool    const pat
                       , bool    const XD)
            : PmlCommonEntry(paddr, present, writable, userAccessible, PWT, PCD
                           , accessed, dirty, global, true, pat, XD)
        {

        }
    };
    typedef Pml3Entry PdptEntry;

    /**
     *  Page Map Level 3: Page Directory Pointer Table.
     */
    struct Pml3
    {
        /*
         *  This table is indexed by bits 30-38 from the linear address.
         *  Naturally, bits 0-2 of the index are 0. (8-byte entries)
         */

        static const uint64_t AddressBits   = 0x0000007FC0000000ULL;
        static const uint64_t IndexOffset   = 30;

        /*  Constructors  */

        Pml3() = default;
        Pml3(Pml3 const &) = default;

        /*  Operators  */

        /**
         *  Gets the entry at a given index.
         */
        inline Pml3Entry & operator [](uint16_t const ind)
        {
            return this->Entries[ind];
        }

        /**
         *  Gets the entry corresponding to the given linear address.
         */
        inline Pml3Entry & operator [](void const * const vaddr)
        {
            //  Take the interesting bits from the linear address...
            //  Shift it by the required amount of bits...
            //  And use that as an index! :D
            return this->Entries[(reinterpret_cast<uintptr_t>(vaddr) & AddressBits) >> IndexOffset];
        }

        /*  Field(s)  */

    //private:

        Pml3Entry Entries[512]; //  Yeah...
    };
    typedef Pml3 Pdpt;


    /**
     *  Page Map Level 4 Entry
     */
    struct Pml4Entry : PmlCommonEntry
    {
        /*  Bit structure:
         *       0       : Present (if 1)
         *       1       : R/W (1 means writes allowed)
         *       2       : U/S (1 if usermode access allowed)
         *       3       : PWT (page-level write-through)
         *       4       : PCD (page-level cache disable)
         *       5       : Accessed
         *       6       : Ignored
         *       7       : Reserved (must be 0)
         *       8 -  11 : Ignored
         *      12 - M-1 : Physical address of PML3(PDPT) table; 4-KiB aligned.
         *       M -  51 : Reserved (must be 0)
         *      52 -  62 : Ignored
         *      63       : XD (eXecute Disable, if 1)
         */

        /*  Properties  */

        BITFIELD_DEFAULT_2W(12, 40, Pml3 * , Pml3Ptr)

        /*  Constructors  */

        /**
         *  Creates an empty PML4 (PT) entry structure.
         */
        inline Pml4Entry() : PmlCommonEntry() { }

        /**
         *  Creates a new PML4 entry structure that points to a PML3 (PDPT) table.
         */
        inline Pml4Entry(paddr_t const pml3_paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const XD)
            : PmlCommonEntry(pml3_paddr, present, writable, userAccessible
                           , false, XD)
        {

        }

        /**
         *  Creates a new PML4 entry structure that points to a PML3 (PDPT) table.
         */
        inline Pml4Entry(paddr_t const pml3_paddr
                       , bool    const present
                       , bool    const writable
                       , bool    const userAccessible
                       , bool    const PWT
                       , bool    const PCD
                       , bool    const accessed
                       , bool    const XD)
            : PmlCommonEntry(pml3_paddr, present, writable, userAccessible, PWT, PCD
                           , accessed, false, false, false, false, XD)
        {

        }
    };

    /**
     *  Page Map Level 4
     */
    struct Pml4
    {
        /*
         *  This table is indexed by bits 39-47 from the linear address.
         *  Naturally, bits 0-2 of the index are 0. (8-byte entries)
         */

        static const uint64_t AddressBits   = 0x0000FF8000000000ULL;
        static const uint64_t IndexOffset   = 39;

        /*  Constructors  */

        Pml4() = default;
        Pml4(Pml4 const &) = default;

        /*  Operators  */

        /**
         *  Gets the entry at a given index.
         */
        inline Pml4Entry & operator [](uint16_t const ind)
        {
            return this->Entries[ind];
        }

        /**
         *  Gets the entry corresponding to the given linear address.
         */
        inline Pml4Entry & operator [](void const * const vaddr)
        {
            //  Take the interesting bits from the linear address...
            //  Shift it by the required amount of bits...
            //  And use that as an index! :D
            return this->Entries[(reinterpret_cast<uintptr_t>(vaddr) & AddressBits) >> IndexOffset];
        }

        /*  Field(s)  */

    //private:

        Pml4Entry Entries[512]; //  Yeah...
    };
}}
