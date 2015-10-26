#pragma once

#include <utils/bitfields.hpp>

namespace Beelzebub { namespace System
{
    /**
     *  Flags in the (E|R)FLAGS register.
     */
    enum class FlagsRegisterFlags
        : size_t
    {
        Carry                   = 0x00000001,
        Reserved1               = 0x00000002,
        Parity                  = 0x00000004,
        Reserved2               = 0x00000008,
        Adjust                  = 0x00000010,
        Reserved3               = 0x00000020,
        Zero                    = 0x00000040,
        Sign                    = 0x00000080,
        Trap                    = 0x00000100,
        InterruptEnable         = 0x00000200,
        Direction               = 0x00000400,
        Overflow                = 0x00000800,
        IoPrivilegeLevel        = 0x00003000,
        NestedTask              = 0x00004000,
        Reserved4               = 0x00008000,

        Resume                  = 0x00010000,
        Virtual8086Mode         = 0x00020000,
        AlignmentCheck          = 0x00040000,
        VirtualInterrupt        = 0x00080000,
        VirtualInterruptPending = 0x00100000,
        Cpuid                   = 0x00200000,
    };

    ENUMOPS(FlagsRegisterFlags, size_t)

    /**
     *  Represents the value of a MSR.
     */
    union MsrValue
    {
        struct
        {
            uint32_t Low;
            uint32_t High;
        } Dwords;

        uint64_t Qword;

        //  Little endian, remember!
    };

    /**
     * Represents the contents of the IA32_EFER MSR.
     */
    struct Ia32Efer
    {
        /*  Bit structure:
         *       0       : SYSCALL Enable
         *       8       : Long Mode Enable
         *      10       : Long Mode Active
         *      11       : Execute Disable Bit Enable
         */

        static uint64_t const SyscallEnableBit    = 1ULL <<  0;
        static uint64_t const LongModeEnableBit   = 1ULL <<  8;
        static uint64_t const LongModeActiveBit   = 1ULL << 10;
        static uint64_t const NonExecuteEnableBit = 1ULL << 11;

        static uint64_t const SyscallEnableBitIndex       =  0;
        static uint64_t const LongModeEnableBitIndex      =  8;
        static uint64_t const LongModeActiveBitIndex      = 10;
        static uint64_t const NonExecuteEnableBitIndex    = 11;

        /*  Constructors  */

        /**
         *  Creates a new IA32_EFER structure from the given MSR value.
         */
        __bland inline explicit Ia32Efer(MsrValue const val)
        {
            this->Value = val.Qword;
        }

        /**
         *  Creates a new IA32_EFER structure from the given raw value.
         */
        __bland inline explicit Ia32Efer(uint64_t const val)
        {
            this->Value = val;
        }

        /**
         *  Creates a new IA32_EFER structure with the given flags.
         */
        __bland inline Ia32Efer(bool const syscallEnable
                                     , bool const longModeEnable
                                     , bool const nonExecuteEnable)
        {
            this->Value = (syscallEnable    ? SyscallEnableBit    : 0)
                        | (longModeEnable   ? LongModeEnableBit   : 0)
                        | (nonExecuteEnable ? NonExecuteEnableBit : 0);
        }

        /*  Properties  */

        BITFIELD_FLAG_RW(SyscallEnableBitIndex   , SyscallEnable   , uint64_t, this->Value, __bland, const)
        BITFIELD_FLAG_RW(LongModeEnableBitIndex  , LongModeEnable  , uint64_t, this->Value, __bland, const)
        BITFIELD_FLAG_RO(LongModeActiveBitIndex  , LongModeActive  , uint64_t, this->Value, __bland, const)
        BITFIELD_FLAG_RW(NonExecuteEnableBitIndex, NonExecuteEnable, uint64_t, this->Value, __bland, const)

        /*  Field(s)  */

    //private:

        uint64_t Value;
    };
}}
