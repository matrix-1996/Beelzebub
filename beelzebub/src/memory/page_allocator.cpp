#include <memory/page_allocator.hpp>
#include <debug.hpp>
#include <math.h>

using namespace Beelzebub;
using namespace Beelzebub::Terminals;
using namespace Beelzebub::Debug;
using namespace Beelzebub::Memory;

/*********************************
    PageAllocationSpace struct
*********************************/

/*  Constructors    */

PageAllocationSpace::PageAllocationSpace()
    //  Just the basics.
    : MemoryStart( 0 )
    , MemoryEnd(0)
    , AllocationEnd(0)
    , PageSize(0)
    , PageCount(0)
    , Size(0)

    //  Number of pages that are allocable.
    , AllocablePageCount(0)

    //  Miscellaneous.
    , ReservedPageCount(0)
    , Map(nullptr)
    , Locker()
{

}

PageAllocationSpace::PageAllocationSpace(const paddr_t phys_start, const paddr_t phys_end
                                       , const psize_t page_size)
    //  Just the basics.
    : MemoryStart( phys_start )
    , MemoryEnd(phys_end)
    , AllocationEnd(phys_end)
    , PageSize(page_size)
    , PageCount((phys_end - phys_start) / page_size)
    , Size(phys_end - phys_start)

    //  Number of pages that are allocable.
    , AllocablePageCount((phys_end - phys_start) / (page_size + sizeof(PageDescriptor) + sizeof(pgind_t)))

    //  Miscellaneous.
    , ReservedPageCount(0)
    , Map((PageDescriptor *)phys_start)
    , Locker()
{
    this->FreePageCount = this->AllocablePageCount;
    this->ControlPageCount = this->PageCount - this->FreePageCount;
    this->FreeSize = this->AllocableSize = this->AllocablePageCount * page_size;
    this->ReservedSize = this->ReservedPageCount * page_size;

    this->Stack = (psize_t *)(this->Map + this->AllocablePageCount);

    for (size_t i = 0; i < this->AllocablePageCount; ++i)
        this->Map[i] = PageDescriptor(this->Stack[i] = i, PageDescriptorStatus::Free);

    this->StackFreeTop = this->StackCacheTop = this->AllocablePageCount - 1;
    this->AllocationStart = phys_start + (this->ControlPageCount * page_size);
}

/*  Page manipulation  */

Handle PageAllocationSpace::ReservePageRange(const pgind_t start, const psize_t count, const PageReservationOptions options)
{
    if unlikely(start >= this->AllocablePageCount || start + count > this->AllocablePageCount)
        return Handle(HandleResult::PagesOutOfAllocatorRange);

    const bool inclCaching  = (0 != (options & PageReservationOptions::IncludeCaching));
    const bool inclInUse    = (0 != (options & PageReservationOptions::IncludeInUse  ));
    const bool ignrReserved = (0 != (options & PageReservationOptions::IgnoreReserved));

    PageDescriptor * const map = this->Map + start;

    for (pgind_t i = 0; i < count; ++i)
    {
        PageDescriptor * const page = map + i;
        const PageDescriptorStatus status = page->Status;

        /*msg("Reserving page #%us (%Xp @%us: %s).%n", start + i
            , this->MemoryStart + (start + i) * this->PageSize
            , i, page->GetStatusString());//*/

        if likely(status == PageDescriptorStatus::Free)
        {
            /*msg("  SI: %us; SFT: %us; SCT: %us.%n"
                , page->StackIndex
                , this->StackFreeTop
                , this->StackCacheTop);//*/

            this->Lock();

            this->PopPage(start + i);
        }
        else if (status == PageDescriptorStatus::Caching)
        {
            if (inclCaching)
            {
                /*msg("  SI: %us; SCT: %us.%n"
                    , page->StackIndex
                    , this->StackCacheTop);//*/

                this->Lock();

                //  TODO: Perhaps notify of this event?

                this->PopPage(start + i);
            }
            else
                return Handle(HandleResult::PageCaching);
        }
        else if (status == PageDescriptorStatus::InUse)
        {
            if (inclInUse)
                this->Lock();
            else
                return Handle(HandleResult::PageInUse);

            //  Yes, locking is enough. It will reserve and unlock later.
        }
        else if (ignrReserved)
            continue;
        else
            return Handle(HandleResult::PageReserved);

        page->Reserve();

        this->Unlock();
    }

    this->FreePageCount -= count;
    this->ReservedPageCount += count;

    this->FreeSize = this->FreePageCount * this->PageSize;
    this->ReservedSize = this->ReservedPageCount * this->PageSize;

    return Handle(HandleResult::Okay);
}

Handle PageAllocationSpace::FreePageRange(const pgind_t start, const psize_t count)
{
    if unlikely(start >= this->AllocablePageCount || start + count > this->AllocablePageCount)
        return Handle(HandleResult::PagesOutOfAllocatorRange);

    PageDescriptor * const map = this->Map + start;

    for (pgind_t i = 0; i < count; ++i)
    {
        PageDescriptor * const page = map + i;
        const PageDescriptorStatus status = page->Status;

        if likely(status == PageDescriptorStatus::InUse)
        {
            this->Lock();

            if (this->StackCacheTop != this->StackFreeTop)
                this->Stack[++this->StackCacheTop] = this->Stack[this->StackFreeTop];

            this->Stack[page->StackIndex = ++this->StackFreeTop] = start + i;
        }
        else if (status == PageDescriptorStatus::Caching)
        {
            this->Lock();

            //  TODO: Perhaps notify of this event?

            this->Stack[page->StackIndex] = this->Stack[++this->StackFreeTop];
            //  Move the bottom free page to the position of the current page.

            this->Stack[page->StackIndex = this->StackFreeTop] = start + i;
        }
        else
        {
            //  Reserved pages won't be freed by this function.
            //  Free pages are already free.

            if (status == PageDescriptorStatus::Reserved)
                return Handle(HandleResult::PageReserved);
            else
                return Handle(HandleResult::PageFree);

            //  The proper error must be returned!
        }

        page->Free();

        this->Unlock();
    }

    return Handle(HandleResult::Okay);
}

paddr_t PageAllocationSpace::AllocatePage(PageDescriptor * & desc)
{
    if likely(this->FreePageCount != 0)
    {
        this->Lock();

        pgind_t i = this->Stack[this->StackFreeTop--];

        if (this->StackCacheTop != this->StackFreeTop + 1)
            this->Stack[this->StackFreeTop + 1] = this->Stack[this->StackCacheTop--];
        else
            --this->StackCacheTop;

        (desc = this->Map + i)->Use();
        //  Mark the page as used.
        //  And, of course, return the descriptor.

        --this->FreePageCount;
        this->FreeSize -= this->PageSize;
        //  Change the info accordingly.

        this->Unlock();

        return this->AllocationStart + i * this->PageSize;
    }

    desc = nullptr;

    return nullptr;
}

paddr_t PageAllocationSpace::AllocatePages(const psize_t count)
{
    if (count == 1)
    {
        PageDescriptor * desc;

        return this->AllocatePage(desc);
    }
    else if (count == 0)
        return nullptr;

    //  TODO: Implement this in the most noobish way imaginable.

    return nullptr;
}

/*  Utilitary Methods  */

Handle PageAllocationSpace::PopPage(const pgind_t ind)
{
    if unlikely(ind >= this->AllocablePageCount)
        return Handle(HandleResult::PagesOutOfAllocatorRange);

    PageDescriptor * const page = this->Map + ind;

    if (page->Status == PageDescriptorStatus::Free)
    {
        PageDescriptor * freeTop = this->Map + this->Stack[this->StackFreeTop];

        //  Popping off el stacko.
        this->Stack[page->StackIndex] = this->Stack[this->StackFreeTop--];
        freeTop->StackIndex = page->StackIndex;

        if (this->StackCacheTop != this->StackFreeTop + 1)
        {
            PageDescriptor * cacheTop = this->Map + this->Stack[this->StackCacheTop];

            this->Stack[this->StackFreeTop + 1] = this->Stack[this->StackCacheTop--];
            cacheTop->StackIndex = this->StackFreeTop + 1;
        }
        else
            --this->StackCacheTop;

        --this->FreePageCount;
        this->FreeSize -= this->PageSize;
        //  Change the info accordingly.

        return Handle(HandleResult::Okay);
    }
    else if (page->Status == PageDescriptorStatus::Caching)
    {
        PageDescriptor * cacheTop = this->Map + this->Stack[this->StackCacheTop];

        //  Popping off el stacko.
        this->Stack[page->StackIndex] = this->Stack[this->StackCacheTop--];
        cacheTop->StackIndex = page->StackIndex;

        return Handle(HandleResult::Okay);
    }

    return Handle(HandleResult::PageNotStacked);
}

/*  Debug  */

#ifdef __BEELZEBUB__DEBUG
TerminalWriteResult PageAllocationSpace::PrintStackToTerminal(TerminalBase * const term, const bool details)
{
    TerminalWriteResult tret;
    uint32_t cnt;

    if (details)
    {
        TERMTRY0(term->WriteFormat("--|T|M|  Stack  Index  |   Page Index   | ... PR: %Xp-%Xp; STK: %Xp--%n", this->AllocationStart, this->AllocationEnd, this->Stack), tret);

        for (size_t i = 0; i <= this->StackFreeTop; ++i)
        {
            TERMTRY1(term->Write("  |F|"), tret, cnt);
            TERMTRY1(term->Write((i == this->Stack[i]) ? ' ' : 'X'), tret, cnt);
            TERMTRY1(term->Write('|'), tret, cnt);
            TERMTRY1(term->WriteHex64((uint64_t)i), tret, cnt);
            TERMTRY1(term->Write('|'), tret, cnt);
            TERMTRY1(term->WriteHex64((uint64_t)(this->Stack[i])), tret, cnt);
            TERMTRY1(term->WriteLine(""), tret, cnt);
        }

        if (this->StackCacheTop != this->StackFreeTop)
        {
            TERMTRY1(term->WriteLine("  NEXT ARE CACHING PAGES"), tret, cnt);

            for (size_t i = this->StackFreeTop + 1; i <= this->StackCacheTop; ++i)
            {
                TERMTRY1(term->Write("  |C|"), tret, cnt);
                TERMTRY1(term->Write((i == this->Stack[i]) ? ' ' : 'X'), tret, cnt);
                TERMTRY1(term->Write('|'), tret, cnt);
                TERMTRY1(term->WriteHex64((uint64_t)i), tret, cnt);
                TERMTRY1(term->Write('|'), tret, cnt);
                TERMTRY1(term->WriteHex64((uint64_t)(this->Stack[i])), tret, cnt);
                TERMTRY1(term->WriteLine(""), tret, cnt);
            }
        }
    }
    else
    {
        TERMTRY0(term->WriteFormat("--|T|M|  Stack  Index  |   Page Index   | ... PR: %Xp-%Xp; STK: %Xp--%n", this->MemoryStart, this->MemoryEnd, this->Stack), tret);

        for (size_t i = 0; i <= this->StackFreeTop; ++i)
        {
            TERMTRY1(term->Write("  |F|"), tret, cnt);
            TERMTRY1(term->Write((i == this->Stack[i]) ? ' ' : 'X'), tret, cnt);
            TERMTRY1(term->Write('|'), tret, cnt);
            TERMTRY1(term->WriteHex64((uint64_t)i), tret, cnt);
            TERMTRY1(term->Write('|'), tret, cnt);
            TERMTRY1(term->WriteHex64((uint64_t)(this->Stack[i])), tret, cnt);
            TERMTRY1(term->WriteLine(""), tret, cnt);
        }

        if (this->StackCacheTop != this->StackFreeTop)
        {
            TERMTRY1(term->WriteLine("  NEXT ARE CACHING PAGES"), tret, cnt);

            for (size_t i = this->StackFreeTop + 1; i <= this->StackCacheTop; ++i)
            {
                TERMTRY1(term->Write("  |C|"), tret, cnt);
                TERMTRY1(term->Write((i == this->Stack[i]) ? ' ' : 'X'), tret, cnt);
                TERMTRY1(term->Write('|'), tret, cnt);
                TERMTRY1(term->WriteHex64((uint64_t)i), tret, cnt);
                TERMTRY1(term->Write('|'), tret, cnt);
                TERMTRY1(term->WriteHex64((uint64_t)(this->Stack[i])), tret, cnt);
                TERMTRY1(term->WriteLine(""), tret, cnt);
            }
        }
    }

    return tret;
}
#endif

/***************************
    PageAllocator struct
***************************/

/*  Constructors  */

PageAllocator::PageAllocator()
    : ChainLock()
    , FirstSpace(nullptr)
    , LastSpace(nullptr)
{
    
}

PageAllocator::PageAllocator(PageAllocationSpace * const first)
    : ChainLock()
    , FirstSpace(first)
    , LastSpace(first)
{
    assert(first != nullptr
        , "Attempted to construct a page allocator with a null first allocation space.");
    assert(first->Previous == nullptr
        , "Attempted to construct a page allocator with a first allocation space that seems to be linked to a previous one.");

    while (this->LastSpace->Next != nullptr)
    {
        //  While the last allocation space links to a 'next one'...
        
        assert(this->LastSpace == this->LastSpace->Next->Previous
            , "Linking error in allocation spaces!");
        //  Assert proper linkage between the allocation spaces.

        this->LastSpace = this->LastSpace->Next;
        //  The next will become the last.
    }
}

/*  Page Manipulation  */

Handle PageAllocator::ReserveByteRange(const paddr_t phys_start, const psize_t length, const PageReservationOptions options)
{
    Handle res;
    PageAllocationSpace * space = this->FirstSpace;

    while (space != nullptr)
    {
        res = space->ReserveByteRange(phys_start, length, options);

        if (!res.IsResult(HandleResult::PagesOutOfAllocatorRange))
            return res;

        space = space->Next;
    }

    return Handle(HandleResult::PagesOutOfAllocatorRange);
}

Handle PageAllocator::FreeByteRange(const paddr_t phys_start, const psize_t length)
{
    Handle res;
    PageAllocationSpace * space = this->FirstSpace;

    while (space != nullptr)
    {
        res = space->FreeByteRange(phys_start, length);

        if (!res.IsResult(HandleResult::PagesOutOfAllocatorRange))
            return res;

        space = space->Next;
    }

    return Handle(HandleResult::PagesOutOfAllocatorRange);
}

Handle PageAllocator::FreePageAtAddress(const paddr_t phys_addr)
{
    Handle res;
    PageAllocationSpace * space = this->FirstSpace;

    while (space != nullptr)
    {
        res = space->FreePageAtAddress(phys_addr);

        if (!res.IsResult(HandleResult::PagesOutOfAllocatorRange))
            return res;

        space = space->Next;
    }

    return Handle(HandleResult::PagesOutOfAllocatorRange);
}

paddr_t PageAllocator::AllocatePage(const PageAllocationOptions options, PageDescriptor * & desc)
{
    paddr_t ret = nullptr;
    PageAllocationSpace * space;

    if (0 != (options & PageAllocationOptions::ThirtyTwoBit))
    {
        space = this->LastSpace;

        while (space != nullptr)
        {
            if ((uint64_t)space->GetAllocationEnd() <= (1ULL << 32))
            {
                //  The condition checks that the allocation space ends
                //  at a 32-bit address. (all the other addresses are less,
                //  thus have to be 32-bit if the end is)

                ret = space->AllocatePage(desc);

                if (ret != nullptr)
                    return ret;
            }

            space = space->Previous;
        }
    }
    else
    {
        space = this->FirstSpace;

        while (space != nullptr)
        {
            ret = space->AllocatePage(desc);

            if (ret != nullptr)
                return ret;

            space = space->Next;
        }
    }

    desc = nullptr;

    return nullptr;
}

paddr_t PageAllocator::AllocatePages(const psize_t count, const PageAllocationOptions options)
{
    paddr_t ret = nullptr;
    PageAllocationSpace * space;

    if (0 != (options & PageAllocationOptions::ThirtyTwoBit))
    {
        space = this->LastSpace;

        while (space != nullptr)
        {
            if ((uint64_t)space->GetAllocationEnd() <= (1ULL << 32))
            {
                //  The condition checks that the allocation space ends
                //  at a 32-bit address. (all the other addresses are less,
                //  thus have to be 32-bit if the end is)

                ret = space->AllocatePages(count);

                if (ret != nullptr)
                    return ret;
            }

            space = space->Previous;
        }
    }
    else
    {
        space = this->FirstSpace;

        while (space != nullptr)
        {
            ret = space->AllocatePages(count);

            if (ret != nullptr)
                return ret;

            space = space->Next;
        }
    }

    return nullptr;
}

PageAllocationSpace * PageAllocator::GetSpaceContainingAddress(const paddr_t address)
{
    PageAllocationSpace * space = this->FirstSpace;

    while (space != nullptr)
    {
        if (space->ContainsRange(address, 1))
            return space;

        space = space->Next;
    }

    return nullptr;
}

bool PageAllocator::ContainsRange(const paddr_t phys_start, const psize_t length)
{
    const PageAllocationSpace * space = this->FirstSpace;

    while (space != nullptr)
    {
        if (space->ContainsRange(phys_start, length))
            return true;

        space = space->Next;
    }

    return false;
}

bool PageAllocator::TryGetPageDescriptor(const paddr_t paddr, PageDescriptor * & res)
{
    PageAllocationSpace * space = this->FirstSpace;

    while (space != nullptr)
    {
        if (space->TryGetPageDescriptor(paddr, res))
            return true;

        space = space->Next;
    }

    return false;
}

/*  Space Chaining  */

void PageAllocator::PreppendAllocationSpace(PageAllocationSpace * const space)
{
    this->Lock();

    this->FirstSpace->Previous = space;
    space->Next = this->FirstSpace;
    this->FirstSpace = space;

    this->Unlock();
}

void PageAllocator::AppendAllocationSpace(PageAllocationSpace * const space)
{
    this->Lock();

    this->LastSpace->Next = space;
    space->Previous = this->LastSpace;
    this->LastSpace = space;

    this->Unlock();
}

void PageAllocator::RemapLinks(const vaddr_t oldAddr, const vaddr_t newAddr)
{
    this->Lock();

    const vaddr_t firstAddr   = (vaddr_t)this->FirstSpace;
    const vaddr_t lastAddr    = (vaddr_t)this->LastSpace;
    const vaddr_t oldVaddr    = oldAddr;
    const vaddr_t newVaddr    = newAddr;
    const vaddr_t oldVaddrEnd = oldVaddr + 0x1000U;

    if (firstAddr > 0 && firstAddr >= oldVaddr && firstAddr < oldVaddrEnd)
        this->FirstSpace = (PageAllocationSpace *)((firstAddr - oldVaddr) + newVaddr);
    if (lastAddr > 0 && lastAddr >= oldVaddr && lastAddr < oldVaddrEnd)
        this->LastSpace = (PageAllocationSpace *)((lastAddr - oldVaddr) + newVaddr);

    //  Now makin' sure all the pointers are aligned.

    PageAllocationSpace * cur = this->LastSpace;

    while (cur != nullptr)
    {
        const vaddr_t nextAddr = (vaddr_t)cur->Next;
        const vaddr_t prevAddr = (vaddr_t)cur->Previous;

        if (nextAddr > 0 && nextAddr >= oldVaddr && nextAddr < oldVaddrEnd)
            this->FirstSpace = (PageAllocationSpace *)((nextAddr - oldVaddr) + newVaddr);
        if (prevAddr > 0 && prevAddr >= oldVaddr && prevAddr < oldVaddrEnd)
            this->LastSpace = (PageAllocationSpace *)((prevAddr - oldVaddr) + newVaddr);

        cur = cur->Previous;
    }

    this->Unlock();
}