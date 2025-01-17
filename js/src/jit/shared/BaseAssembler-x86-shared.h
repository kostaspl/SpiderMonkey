/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jit_shared_BaseAssembler_x86_shared_h
#define jit_shared_BaseAssembler_x86_shared_h

#include "mozilla/IntegerPrintfMacros.h"

#include "jit/shared/AssemblerBuffer-x86-shared.h"
#include "jit/shared/Encoding-x86-shared.h"
#include "jit/shared/Patching-x86-shared.h"

#define BLND_FUNC spew("@@@ %s:%d @@@", __func__, __LINE__)

namespace js {
namespace jit {

namespace X86Encoding {

class BaseAssembler : public GenericAssembler {
public:
    BaseAssembler()
      : useVEX_(true)
    { }

    void disableVEX() { useVEX_ = false; }

    size_t size() const { return m_formatter.size(); }
    const unsigned char* buffer() const { return m_formatter.buffer(); }
    unsigned char* data() { return m_formatter.data(); }
    bool oom() const { return m_formatter.oom(); }

    /* Variables (initialized to random values) used for
       constant blinding */
    int32_t blinding_value;
    int8_t  blinding_value8;
    int16_t blinding_value16;
    int64_t blinding_value64;

    /* Register used for constant blinding */
    RegisterID blindingReg = RegisterID::r15;

    unsigned char CFopcodes[38] = {
             0xc3,  // [ret] Return
             0xcb,  // [Far return] Far return
             0xcc,  // [int3] Interrupt Type 3
             0xcf,  // [iret] Interrupt Return
             0xc9,  // [leave]
             // Types of conditional jmp
             0x73, 0x77, 0x72, 0x76, 0xe3, 0x74, 0x7f,
             0x7d, 0x7c, 0x7e, 0x71, 0x7b, 0x70, 0x7a,
             0x75, 0x79, 0x0f, 0x82, 0x84, 0x8f, 0x8d,
             0x8c, 0x83, 0x8e, 0x87, 0x80, 0x81, 0x85,
             0x88,
             // Type of un-conditional jmp
             0xeb, 0xe9, 0xff, 0xea
    };

    /* Constant blinding version None
    bool shouldBlindConstant(int32_t imm) {
        return false;
    } */

    /* Constant blinding version Full */
    bool shouldBlindConstant(int32_t imm) {
        return imm != 0;
    }

    /* Constant blinding version Byte2
    bool shouldBlindConstant(int32_t imm) {
        return imm > 0xffff ? true : false;
    } */

    /* Constant blinding version Conditional
    bool shouldBlindConstant(int32_t imm) {
        if(imm > 0x69){
            for (int i = 0; i < 38; ++i) {
                if (reinterpret_cast<unsigned char *>(&imm)[0] == CFopcodes[i] ||
                    reinterpret_cast<unsigned char *>(&imm)[1] == CFopcodes[i] ||
                    reinterpret_cast<unsigned char *>(&imm)[2] == CFopcodes[i] ||
                    reinterpret_cast<unsigned char *>(&imm)[3] == CFopcodes[i] )
                {
                    return true;
                }
            }
        }
	return false;
    } */

    int8_t constantBlindingRand8()
    {
        return 0x7e;
    }

    int16_t constantBlindingRand16()
    {
        return 0x7ead;
    }

    int32_t constantBlindingRand()
    {
        return 0x7eadbeef;
    }

    int64_t constantBlindingRand64()
    {
        return 0x7eadbeeffeedface;
    }

    inline int32_t blindingValue()
    {
        //if (blinding_value)
	//    return blinding_value;
        return blinding_value = constantBlindingRand();
    }

    inline int8_t blindingValue8()
    {
        //if (blinding_value8)
        //    return blinding_value8;
        return blinding_value8 = constantBlindingRand8();
    }

    inline int16_t blindingValue16()
    {
        //if (blinding_value16)
        //    return blinding_value16;
        blinding_value16 = constantBlindingRand16();
        return blinding_value16;
    }

    inline int64_t blindingValue64()
    {
        //if (blinding_value64)
        //    return blinding_value64;
        blinding_value64 = constantBlindingRand64();
        return blinding_value64;
    }

    void nop()
    {
        spew("nop");
        m_formatter.oneByteOp(OP_NOP);
    }

    void twoByteNop()
    {
        spew("nop (2 byte)");
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_NOP);
    }

    // Stack operations:

    void push_r(RegisterID reg)
    {
        spew("push       %s", GPRegName(reg));
        m_formatter.oneByteOp(OP_PUSH_EAX, reg);
    }

    void pop_r(RegisterID reg)
    {
        spew("pop        %s", GPRegName(reg));
        m_formatter.oneByteOp(OP_POP_EAX, reg);
    }

    void push_i(int32_t imm)
    {
        if (shouldBlindConstant(imm)){
            push_i_blnd(imm);
        } else {
            push_i_norm(imm);
        }
    }

    void push_i_blnd(int32_t imm)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            push_i_norm(imm);   // TODO: is this ok?
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm ^ bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            push_r(blindingReg);
        }
    }

    void push_i_norm(int32_t imm)
    {
        spew("push       $%s0x%x", PRETTYHEX(imm));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_PUSH_Ib);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_PUSH_Iz);
            m_formatter.immediate32(imm);
        }
    }

    void push_i32(int32_t imm)
    {
        if (shouldBlindConstant(imm)){
            push_i32_blnd(imm);
        } else {
            push_i32_norm(imm);
        }
    }

    void push_i32_blnd(int32_t imm)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movl_i32r_norm(imm ^ bv, blindingReg);
        xorl_ir_norm(bv, blindingReg);
        push_r(blindingReg);
    }

    void push_i32_norm(int32_t imm)
    {
        spew("push       $%s0x%04x", PRETTYHEX(imm));
        m_formatter.oneByteOp(OP_PUSH_Iz);
        m_formatter.immediate32(imm);
    }

    void push_m(int32_t offset, RegisterID base)
    {
        spew("push       " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP5_Ev, offset, base, GROUP5_OP_PUSH);
    }

    void pop_m(int32_t offset, RegisterID base)
    {
        spew("pop        " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP1A_Ev, offset, base, GROUP1A_OP_POP);
    }

    void push_flags()
    {
        spew("pushf");
        m_formatter.oneByteOp(OP_PUSHFLAGS);
    }

    void pop_flags()
    {
        spew("popf");
        m_formatter.oneByteOp(OP_POPFLAGS);
    }

    // Arithmetic operations:

#ifdef JS_CODEGEN_X86
    void adcl_im(int32_t imm, const void* addr)
    {
        spew("adcl       %d, %p", imm, addr);
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, addr, GROUP1_OP_ADC);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, addr, GROUP1_OP_ADC);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void addl_rr(RegisterID src, RegisterID dst)
    {
        spew("addl       %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_ADD_GvEv, src, dst);
    }

    void addl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("addl       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_ADD_GvEv, offset, base, dst);
    }

    void addl_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("addl       %s, " MEM_ob, GPReg32Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_ADD_EvGv, offset, base, src);
    }

    void addl_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            addl_ir_blnd(imm, dst);
        else
            addl_ir_norm(imm, dst);
    }

    void addl_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            addl_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            addl_ir_norm(imm - bv, dst);
            addl_ir_norm(bv, dst);
        }
    }

    void addl_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("addl       $%d, %s", imm, GPReg32Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, dst, GROUP1_OP_ADD);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp(OP_ADD_EAXIv);
            else
                m_formatter.oneByteOp(OP_GROUP1_EvIz, dst, GROUP1_OP_ADD);
            m_formatter.immediate32(imm);
        }
    }

    void addl_i32r(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            addl_i32r_blnd(imm, dst);
        else
            addl_i32r_norm(imm, dst);
    }

    void addl_i32r_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv = blindingValue();
        addl_i32r_norm(imm - bv, dst);
        addl_i32r_norm(bv, dst);
    }

    void addl_i32r_norm(int32_t imm, RegisterID dst)
    {
        // 32-bit immediate always, for patching.
        spew("addl       $0x%04x, %s", imm, GPReg32Name(dst));
        if (dst == rax)
            m_formatter.oneByteOp(OP_ADD_EAXIv);
        else
            m_formatter.oneByteOp(OP_GROUP1_EvIz, dst, GROUP1_OP_ADD);
        m_formatter.immediate32(imm);
    }

    void addl_im(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            addl_im_blnd(imm, offset, base);
        else
            addl_im_norm(imm, offset, base);
    }

    void addl_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            addl_im_norm(imm, offset, base);
        } else {
            bv = blindingValue();
            addl_im_norm(imm - bv, offset, base);
            addl_im_norm(bv, offset, base);
        }
    }

    void addl_im_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("addl       $%d, " MEM_ob, imm, ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, GROUP1_OP_ADD);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_ADD);
            m_formatter.immediate32(imm);
        }
    }

#ifdef JS_CODEGEN_X64
    void addq_rr(RegisterID src, RegisterID dst)
    {
        spew("addq       %s, %s", GPReg64Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_ADD_GvEv, src, dst);
    }

    void addq_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("addq       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_ADD_GvEv, offset, base, dst);
    }

    void addq_mr(const void* addr, RegisterID dst)
    {
        spew("addq       %p, %s", addr, GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_ADD_GvEv, addr, dst);
    }

    void addq_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            addq_ir_blnd(imm, dst);
        else
            addq_ir_norm(imm, dst);
    }

    void addq_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            addq_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            addq_ir_norm(imm - bv, dst);
            addq_ir_norm(bv, dst);
        }
    }

    void addq_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("addq       $%d, %s", imm, GPReg64Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, dst, GROUP1_OP_ADD);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp64(OP_ADD_EAXIv);
            else
                m_formatter.oneByteOp64(OP_GROUP1_EvIz, dst, GROUP1_OP_ADD);
            m_formatter.immediate32(imm);
        }
    }

    void addq_im(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            addq_im_blnd(imm, offset, base);
        else
            addq_im_norm(imm, offset, base);
    }

    void addq_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            addq_im_norm(imm, offset, base);
        } else {
            bv = blindingValue();
            addq_im_norm(imm - bv, offset, base);
            addq_im_norm(bv, offset, base);
        }
    }

    void addq_im_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("addq       $%d, " MEM_ob, imm, ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, offset, base, GROUP1_OP_ADD);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, offset, base, GROUP1_OP_ADD);
            m_formatter.immediate32(imm);
        }
    }

    void addq_im(int32_t imm, const void* addr)
    {
        if (shouldBlindConstant(imm))
            addq_im_blnd(imm, addr);
        else
            addq_im_norm(imm, addr);
    }

    void addq_im_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            addq_im_norm(imm, addr);
        } else {
            bv = blindingValue();
            addq_im_norm(imm - bv, addr);
            addq_im_norm(bv, addr);
        }
    }

    void addq_im_norm(int32_t imm, const void* addr)
    {
        spew("addq       $%d, %p", imm, addr);
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, addr, GROUP1_OP_ADD);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, addr, GROUP1_OP_ADD);
            m_formatter.immediate32(imm);
        }
    }
#endif
    void addl_im(int32_t imm, const void* addr)
    {
        if (shouldBlindConstant(imm))
            addl_im_blnd(imm, addr);
        else
            addl_im_norm(imm, addr);
    }

    void addl_im_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            addl_im_norm(imm, addr);
        } else {
            bv = blindingValue();
            addl_im_norm(imm - bv, addr);
            addl_im_norm(bv, addr);
        }
    }

    void addl_im_norm(int32_t imm, const void* addr)
    {
        spew("addl       $%d, %p", imm, addr);
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, addr, GROUP1_OP_ADD);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, addr, GROUP1_OP_ADD);
            m_formatter.immediate32(imm);
        }
    }

    void lock_xaddb_rm(RegisterID srcdest, int32_t offset, RegisterID base)
    {
        spew("lock xaddl %s, " MEM_ob, GPReg8Name(srcdest), ADDR_ob(offset, base));
        m_formatter.oneByteOp(PRE_LOCK);
        m_formatter.twoByteOp(OP2_XADD_EbGb, offset, base, srcdest);
    }

    void lock_xaddb_rm(RegisterID srcdest, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("lock xaddl %s, " MEM_obs, GPReg8Name(srcdest), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(PRE_LOCK);
        m_formatter.twoByteOp(OP2_XADD_EbGb, offset, base, index, scale, srcdest);
    }

    void lock_xaddl_rm(RegisterID srcdest, int32_t offset, RegisterID base)
    {
        spew("lock xaddl %s, " MEM_ob, GPReg32Name(srcdest), ADDR_ob(offset, base));
        m_formatter.oneByteOp(PRE_LOCK);
        m_formatter.twoByteOp(OP2_XADD_EvGv, offset, base, srcdest);
    }

    void lock_xaddl_rm(RegisterID srcdest, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("lock xaddl %s, " MEM_obs, GPReg32Name(srcdest), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(PRE_LOCK);
        m_formatter.twoByteOp(OP2_XADD_EvGv, offset, base, index, scale, srcdest);
    }

    void vpaddd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpaddd", VEX_PD, OP2_PADDD_VdqWdq, src1, src0, dst);
    }
    void vpaddd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpaddd", VEX_PD, OP2_PADDD_VdqWdq, offset, base, src0, dst);
    }
    void vpaddd_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpaddd", VEX_PD, OP2_PADDD_VdqWdq, address, src0, dst);
    }

    void vpsubd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpsubd", VEX_PD, OP2_PSUBD_VdqWdq, src1, src0, dst);
    }
    void vpsubd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpsubd", VEX_PD, OP2_PSUBD_VdqWdq, offset, base, src0, dst);
    }
    void vpsubd_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpsubd", VEX_PD, OP2_PSUBD_VdqWdq, address, src0, dst);
    }

    void vpmuludq_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpmuludq", VEX_PD, OP2_PMULUDQ_VdqWdq, src1, src0, dst);
    }
    void vpmuludq_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpmuludq", VEX_PD, OP2_PMULUDQ_VdqWdq, offset, base, src0, dst);
    }

    void vpmulld_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        threeByteOpSimd("vpmulld", VEX_PD, OP3_PMULLD_VdqWdq, ESCAPE_38, src1, src0, dst);
    }
    void vpmulld_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        threeByteOpSimd("vpmulld", VEX_PD, OP3_PMULLD_VdqWdq, ESCAPE_38, offset, base, src0, dst);
    }
    void vpmulld_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        threeByteOpSimd("vpmulld", VEX_PD, OP3_PMULLD_VdqWdq, ESCAPE_38, address, src0, dst);
    }

    void vaddps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddps", VEX_PS, OP2_ADDPS_VpsWps, src1, src0, dst);
    }
    void vaddps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddps", VEX_PS, OP2_ADDPS_VpsWps, offset, base, src0, dst);
    }
    void vaddps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddps", VEX_PS, OP2_ADDPS_VpsWps, address, src0, dst);
    }

    void vsubps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsubps", VEX_PS, OP2_SUBPS_VpsWps, src1, src0, dst);
    }
    void vsubps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsubps", VEX_PS, OP2_SUBPS_VpsWps, offset, base, src0, dst);
    }
    void vsubps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsubps", VEX_PS, OP2_SUBPS_VpsWps, address, src0, dst);
    }

    void vmulps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmulps", VEX_PS, OP2_MULPS_VpsWps, src1, src0, dst);
    }
    void vmulps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmulps", VEX_PS, OP2_MULPS_VpsWps, offset, base, src0, dst);
    }
    void vmulps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmulps", VEX_PS, OP2_MULPS_VpsWps, address, src0, dst);
    }

    void vdivps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vdivps", VEX_PS, OP2_DIVPS_VpsWps, src1, src0, dst);
    }
    void vdivps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vdivps", VEX_PS, OP2_DIVPS_VpsWps, offset, base, src0, dst);
    }
    void vdivps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vdivps", VEX_PS, OP2_DIVPS_VpsWps, address, src0, dst);
    }

    void vmaxps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmaxps", VEX_PS, OP2_MAXPS_VpsWps, src1, src0, dst);
    }
    void vmaxps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmaxps", VEX_PS, OP2_MAXPS_VpsWps, offset, base, src0, dst);
    }
    void vmaxps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmaxps", VEX_PS, OP2_MAXPS_VpsWps, address, src0, dst);
    }

    void vminps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vminps", VEX_PS, OP2_MINPS_VpsWps, src1, src0, dst);
    }
    void vminps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vminps", VEX_PS, OP2_MINPS_VpsWps, offset, base, src0, dst);
    }
    void vminps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vminps", VEX_PS, OP2_MINPS_VpsWps, address, src0, dst);
    }

    void andl_rr(RegisterID src, RegisterID dst)
    {
        spew("andl       %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_AND_GvEv, src, dst);
    }

    void andl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("andl       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_AND_GvEv, offset, base, dst);
    }

    void andl_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("andl       %s, " MEM_ob, GPReg32Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_AND_EvGv, offset, base, src);
    }

    void andl_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            andl_ir_blnd(imm, dst);
        else
            andl_ir_norm(imm, dst);
    }

    void andl_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            andl_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            andl_rr(blindingReg, dst);
        }
    }

    void andl_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("andl       $0x%x, %s", imm, GPReg32Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, dst, GROUP1_OP_AND);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp(OP_AND_EAXIv);
            else
                m_formatter.oneByteOp(OP_GROUP1_EvIz, dst, GROUP1_OP_AND);
            m_formatter.immediate32(imm);
        }
    }

    void andl_im(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            andl_im_blnd(imm, offset, base);
        else
            andl_im_norm(imm, offset, base);
    }

    void andl_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            andl_im_norm(imm, offset, base);
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            andl_rm(blindingReg, offset, base);
        }
    }

    void andl_im_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("andl       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, GROUP1_OP_AND);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_AND);
            m_formatter.immediate32(imm);
        }
    }

#ifdef JS_CODEGEN_X64
    void andq_rr(RegisterID src, RegisterID dst)
    {
        spew("andq       %s, %s", GPReg64Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_AND_GvEv, src, dst);
    }

    void andq_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("andq       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_AND_GvEv, offset, base, dst);
    }

    void andq_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("andq       " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_AND_GvEv, offset, base, index, scale, dst);
    }

    void andq_mr(const void* addr, RegisterID dst)
    {
        spew("andq       %p, %s", addr, GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_AND_GvEv, addr, dst);
    }

    void orq_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("orq        " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_OR_GvEv, offset, base, dst);
    }

    void orq_mr(const void* addr, RegisterID dst)
    {
        spew("orq        %p, %s", addr, GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_OR_GvEv, addr, dst);
    }

    void andq_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            andq_ir_blnd(imm, dst);
        else
            andq_ir_norm(imm, dst);
    }

    void andq_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            andq_ir_norm(imm, dst); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movq_i32r_norm(imm ^ bv, blindingReg);
	    xorq_ir_norm(bv, blindingReg);
            andq_rr(blindingReg, dst);
        }
    }

    void andq_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("andq       $0x%" PRIx64 ", %s", int64_t(imm), GPReg64Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, dst, GROUP1_OP_AND);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp64(OP_AND_EAXIv);
            else
                m_formatter.oneByteOp64(OP_GROUP1_EvIz, dst, GROUP1_OP_AND);
            m_formatter.immediate32(imm);
        }
    }
#else
    void andl_im(int32_t imm, const void* addr)
    {
        spew("andl       $0x%x, %p", imm, addr);
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, addr, GROUP1_OP_AND);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, addr, GROUP1_OP_AND);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void fld_m(int32_t offset, RegisterID base)
    {
        spew("fld        " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_FPU6, offset, base, FPU6_OP_FLD);
    }
    void fld32_m(int32_t offset, RegisterID base)
    {
        spew("fld        " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_FPU6_F32, offset, base, FPU6_OP_FLD);
    }
    void fisttp_m(int32_t offset, RegisterID base)
    {
        spew("fisttp     " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_FPU6, offset, base, FPU6_OP_FISTTP);
    }
    void fstp_m(int32_t offset, RegisterID base)
    {
        spew("fstp       " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_FPU6, offset, base, FPU6_OP_FSTP);
    }
    void fstp32_m(int32_t offset, RegisterID base)
    {
        spew("fstp32       " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_FPU6_F32, offset, base, FPU6_OP_FSTP);
    }

    void negl_r(RegisterID dst)
    {
        spew("negl       %s", GPReg32Name(dst));
        m_formatter.oneByteOp(OP_GROUP3_Ev, dst, GROUP3_OP_NEG);
    }

    void negl_m(int32_t offset, RegisterID base)
    {
        spew("negl       " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP3_Ev, offset, base, GROUP3_OP_NEG);
    }

    void notl_r(RegisterID dst)
    {
        spew("notl       %s", GPReg32Name(dst));
        m_formatter.oneByteOp(OP_GROUP3_Ev, dst, GROUP3_OP_NOT);
    }

    void notl_m(int32_t offset, RegisterID base)
    {
        spew("notl       " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP3_Ev, offset, base, GROUP3_OP_NOT);
    }

    void orl_rr(RegisterID src, RegisterID dst)
    {
        spew("orl        %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_OR_GvEv, src, dst);
    }

    void orl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("orl        " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_OR_GvEv, offset, base, dst);
    }

    void orl_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("orl        %s, " MEM_ob, GPReg32Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_OR_EvGv, offset, base, src);
    }

    void orl_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            orl_ir_blnd(imm, dst);
        else
            orl_ir_norm(imm, dst);
    }

    void orl_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            orl_ir_norm(imm, dst); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm ^ bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            orl_rr(blindingReg, dst);
        }
    }

    void orl_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("orl        $0x%x, %s", imm, GPReg32Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, dst, GROUP1_OP_OR);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp(OP_OR_EAXIv);
            else
                m_formatter.oneByteOp(OP_GROUP1_EvIz, dst, GROUP1_OP_OR);
            m_formatter.immediate32(imm);
        }
    }


    void orl_im(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            orl_im_blnd(imm, offset, base);
        else
            orl_im_norm(imm, offset, base);
    }

    void orl_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            orl_im_norm(imm, offset, base); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm ^ bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            orl_rm(blindingReg, offset, base);
        }
    }

    void orl_im_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("orl        $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, GROUP1_OP_OR);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_OR);
            m_formatter.immediate32(imm);
        }
    }

#ifdef JS_CODEGEN_X64
    void negq_r(RegisterID dst)
    {
        spew("negq       %s", GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_GROUP3_Ev, dst, GROUP3_OP_NEG);
    }

    void orq_rr(RegisterID src, RegisterID dst)
    {
        spew("orq        %s, %s", GPReg64Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_OR_GvEv, src, dst);
    }

    void orq_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            orq_ir_blnd(imm, dst);
        else
            orq_ir_norm(imm, dst);
    }

    void orq_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            orq_ir_norm(imm, dst); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movq_i32r_norm(imm ^ bv, blindingReg);
            orq_rr(blindingReg, dst);
        }
    }

    void orq_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("orq        $0x%" PRIx64 ", %s", int64_t(imm), GPReg64Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, dst, GROUP1_OP_OR);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp64(OP_OR_EAXIv);
            else
                m_formatter.oneByteOp64(OP_GROUP1_EvIz, dst, GROUP1_OP_OR);
            m_formatter.immediate32(imm);
        }
    }

    void notq_r(RegisterID dst)
    {
        spew("notq       %s", GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_GROUP3_Ev, dst, GROUP3_OP_NOT);
    }
#else
/*
    void orl_im(int32_t imm, const void* addr)
    {
        if (shouldBlindConstant(imm))
            orl_im_blnd(imm, addr);
        else
            orl_im_norm(imm, addr);
    }

    void orl_im_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            bv = blindingValue8();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            orl_rm(blindingReg, addr);
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            orl_rm(blindingReg, addr);
        }
    }

    void orl_im_norm(int32_t imm, const void* addr)
    {
        spew("orl        $0x%x, %p", imm, addr);
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, addr, GROUP1_OP_OR);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, addr, GROUP1_OP_OR);
            m_formatter.immediate32(imm);
        }
    }

    void orl_rm(RegisterID src, const void* addr)
    {
        spew("orl        %s, " MEM_ob, GPReg32Name(src), addr);
        m_formatter.oneByteOp(OP_OR_EvGv, addr, src);
    }
*/
#endif

    void subl_rr(RegisterID src, RegisterID dst)
    {
        spew("subl       %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_SUB_GvEv, src, dst);
    }

    void subl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("subl       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_SUB_GvEv, offset, base, dst);
    }

    void subl_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("subl       %s, " MEM_ob, GPReg32Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_SUB_EvGv, offset, base, src);
    }

    void subl_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            subl_ir_blnd(imm, dst);
        else
            subl_ir_norm(imm, dst);
    }

    void subl_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            subl_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            subl_ir_norm(imm - bv, dst);
            subl_ir_norm(bv, dst);
        }
    }

    void subl_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("subl       $%d, %s", imm, GPReg32Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, dst, GROUP1_OP_SUB);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp(OP_SUB_EAXIv);
            else
                m_formatter.oneByteOp(OP_GROUP1_EvIz, dst, GROUP1_OP_SUB);
            m_formatter.immediate32(imm);
        }
    }

    void subl_im(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            subl_im_blnd(imm, offset, base);
        else
            subl_im_norm(imm, offset, base);
    }

    void subl_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            subl_im_norm(imm, offset, base);
        } else {
            bv = blindingValue();
            subl_im_norm(imm-bv, offset, base);
            subl_im_norm(bv, offset, base);
        }
    }

    void subl_im_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("subl       $%d, " MEM_ob, imm, ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, GROUP1_OP_SUB);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_SUB);
            m_formatter.immediate32(imm);
        }
    }

#ifdef JS_CODEGEN_X64
    void subq_rr(RegisterID src, RegisterID dst)
    {
        spew("subq       %s, %s", GPReg64Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_SUB_GvEv, src, dst);
    }

    void subq_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("subq       %s, " MEM_ob, GPReg64Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp64(OP_SUB_EvGv, offset, base, src);
    }

    void subq_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("subq       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_SUB_GvEv, offset, base, dst);
    }

    void subq_mr(const void* addr, RegisterID dst)
    {
        spew("subq       %p, %s", addr, GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_SUB_GvEv, addr, dst);
    }

    void subq_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            subq_ir_blnd(imm, dst);
        else
            subq_ir_norm(imm, dst);
    }

    void subq_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            subq_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            subq_ir_norm(imm-bv, dst);
            subq_ir_norm(bv, dst);
        }
    }

    void subq_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("subq       $%d, %s", imm, GPReg64Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, dst, GROUP1_OP_SUB);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp64(OP_SUB_EAXIv);
            else
                m_formatter.oneByteOp64(OP_GROUP1_EvIz, dst, GROUP1_OP_SUB);
            m_formatter.immediate32(imm);
        }
    }
#else
    void subl_im(int32_t imm, const void* addr)
    {
        if (shouldBlindConstant(imm))
            subl_im_blnd(imm, addr);
        else
            subl_im_norm(imm, addr);
    }

    void subl_im_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            subl_im_norm(imm, addr);
        } else {
            bv = blindingValue();
            subl_im_norm(imm - bv, addr);
            subl_im_norm(bv, addr);
        }
    }

    void subl_im_norm(int32_t imm, const void* addr)
    {
        spew("subl       $%d, %p", imm, addr);
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, addr, GROUP1_OP_SUB);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, addr, GROUP1_OP_SUB);
            m_formatter.immediate32(imm);
        }
    }
#endif

    /* Custom XOR for imm8. Not really tested */
    void xorb_i8r(int32_t imm, RegisterID dst)
    {
        spew("xorb       $%d, %s", imm, GPReg64Name(dst));
	m_formatter.oneByteOp(OP_GROUP1_EvIb, dst, GROUP1_OP_XOR);
	m_formatter.immediate8(imm);
    }

    /* Custom XOR for imm8. Not really tested */
    void xorb_i8m(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("xorb       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
	m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, GROUP1_OP_XOR);
	m_formatter.immediate8(imm);
    }

    /* Custom XOR for imm8. Not really tested */
    void xorb_i8m(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("xorb       $0x%x, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
	m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, index, scale, GROUP1_OP_XOR);
	m_formatter.immediate8(imm);
    }

    /* Custom XOR for imm16. Not really tested */
    void xorw_i16m(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("xorw       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_XOR);
        m_formatter.immediate16(imm);
    }

    /* Custom XOR for imm16. Not really tested */
    void xorw_i16m(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("xorw       $0x%x, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, index, scale, GROUP1_OP_XOR);
        m_formatter.immediate16(imm);
    }

    /* Custom XOR for imm32. Not really tested */
    void xorl_i32m(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("xorl       $0x%x, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, index, scale, GROUP1_OP_XOR);
        m_formatter.immediate32(imm);
    }

    /* Custom XOR for sign-extended 32imm as 64. Not really tested */
    void xorq_i32m(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("xorq32       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        m_formatter.oneByteOp64(OP_GROUP1_EvIz, offset, base, GROUP1_OP_XOR);
        m_formatter.immediate32(imm);
    }

    /* Custom XOR for sign-extended 32imm as 64. Not really tested */
    void xorq_i32m(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("xorq32       $0x%x, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp64(OP_GROUP1_EvIz, offset, base, index, scale, GROUP1_OP_XOR);
        m_formatter.immediate32(imm);
    }

    /* Custom XOR for 64-bit imm. Not really tested */
    void xorq_i64r(int64_t imm, RegisterID dst)
    {
        spew("xorq64       $0x%" PRIx64 ", %s", imm, GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_GROUP1_EvIz, dst, GROUP1_OP_XOR);
        m_formatter.immediate64(imm);
    }

    void xorl_rr(RegisterID src, RegisterID dst)
    {
        spew("xorl       %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_XOR_GvEv, src, dst);
    }

    void xorl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("xorl       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_XOR_GvEv, offset, base, dst);
    }

    void xorl_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("xorl       %s, " MEM_ob, GPReg32Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_XOR_EvGv, offset, base, src);
    }

    void xorl_im(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            xorl_im_blnd(imm, offset, base);
        else
            xorl_im_norm(imm, offset, base);
    }

    void xorl_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            xorl_im_norm(imm, offset, base);
        } else {
            bv = blindingValue();
            xorl_im_norm(imm ^ bv, offset, base);
            xorl_im_norm(bv, offset, base);
        } 
    }

    void xorl_im_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("xorl       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, GROUP1_OP_XOR);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_XOR);
            m_formatter.immediate32(imm);
        }
    }

    void xorl_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            xorl_ir_blnd(imm, dst);
        else
            xorl_ir_norm(imm, dst);
    }

    void xorl_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            xorl_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            xorl_ir_norm(imm ^ bv, dst);
            xorl_ir_norm(bv, dst);
        }
    }

    void xorl_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("xorl       $%d, %s", imm, GPReg32Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, dst, GROUP1_OP_XOR);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp(OP_XOR_EAXIv);
            else
                m_formatter.oneByteOp(OP_GROUP1_EvIz, dst, GROUP1_OP_XOR);
            m_formatter.immediate32(imm);
        }
    }

#ifdef JS_CODEGEN_X64
    void xorq_rr(RegisterID src, RegisterID dst)
    {
        spew("xorq       %s, %s", GPReg64Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_XOR_GvEv, src, dst);
    }

    void xorq_ir(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            xorq_ir_blnd(imm, dst);
        else
            xorq_ir_norm(imm, dst);
    }

    void xorq_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            xorq_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            xorq_ir_norm(imm ^ bv, dst);
            xorq_ir_norm(bv, dst);
        }
    }

    void xorq_ir_norm(int32_t imm, RegisterID dst)
    {
        spew("xorq       $0x%" PRIx64 ", %s", int64_t(imm), GPReg64Name(dst));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, dst, GROUP1_OP_XOR);
            m_formatter.immediate8s(imm);
        } else {
            if (dst == rax)
                m_formatter.oneByteOp64(OP_XOR_EAXIv);
            else
                m_formatter.oneByteOp64(OP_GROUP1_EvIz, dst, GROUP1_OP_XOR);
            m_formatter.immediate32(imm);
        }
    }
#endif

    void sarl_ir(int32_t imm, RegisterID dst)
    {
        MOZ_ASSERT(imm < 32);
        spew("sarl       $%d, %s", imm, GPReg32Name(dst));
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, dst, GROUP2_OP_SAR);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, dst, GROUP2_OP_SAR);
            m_formatter.immediate8u(imm);
        }
    }

    void sarl_CLr(RegisterID dst)
    {
        spew("sarl       %%cl, %s", GPReg32Name(dst));
        m_formatter.oneByteOp(OP_GROUP2_EvCL, dst, GROUP2_OP_SAR);
    }

    void shrl_ir(int32_t imm, RegisterID dst)
    {
        MOZ_ASSERT(imm < 32);
        spew("shrl       $%d, %s", imm, GPReg32Name(dst));
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, dst, GROUP2_OP_SHR);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, dst, GROUP2_OP_SHR);
            m_formatter.immediate8u(imm);
        }
    }

    void shrl_CLr(RegisterID dst)
    {
        spew("shrl       %%cl, %s", GPReg32Name(dst));
        m_formatter.oneByteOp(OP_GROUP2_EvCL, dst, GROUP2_OP_SHR);
    }

    void shll_ir(int32_t imm, RegisterID dst)
    {
        MOZ_ASSERT(imm < 32);
        spew("shll       $%d, %s", imm, GPReg32Name(dst));
        if (imm == 1)
            m_formatter.oneByteOp(OP_GROUP2_Ev1, dst, GROUP2_OP_SHL);
        else {
            m_formatter.oneByteOp(OP_GROUP2_EvIb, dst, GROUP2_OP_SHL);
            m_formatter.immediate8u(imm);
        }
    }

    void shll_CLr(RegisterID dst)
    {
        spew("shll       %%cl, %s", GPReg32Name(dst));
        m_formatter.oneByteOp(OP_GROUP2_EvCL, dst, GROUP2_OP_SHL);
    }

#ifdef JS_CODEGEN_X64
    void sarq_CLr(RegisterID dst)
    {
        spew("sarq       %%cl, %s", GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_GROUP2_EvCL, dst, GROUP2_OP_SAR);
    }

    void sarq_ir(int32_t imm, RegisterID dst)
    {
        MOZ_ASSERT(imm < 64);
        spew("sarq       $%d, %s", imm, GPReg64Name(dst));
        if (imm == 1)
            m_formatter.oneByteOp64(OP_GROUP2_Ev1, dst, GROUP2_OP_SAR);
        else {
            m_formatter.oneByteOp64(OP_GROUP2_EvIb, dst, GROUP2_OP_SAR);
            m_formatter.immediate8u(imm);
        }
    }

    void shlq_ir(int32_t imm, RegisterID dst)
    {
        MOZ_ASSERT(imm < 64);
        spew("shlq       $%d, %s", imm, GPReg64Name(dst));
        if (imm == 1)
            m_formatter.oneByteOp64(OP_GROUP2_Ev1, dst, GROUP2_OP_SHL);
        else {
            m_formatter.oneByteOp64(OP_GROUP2_EvIb, dst, GROUP2_OP_SHL);
            m_formatter.immediate8u(imm);
        }
    }

    void shrq_ir(int32_t imm, RegisterID dst)
    {
        MOZ_ASSERT(imm < 64);
        spew("shrq       $%d, %s", imm, GPReg64Name(dst));
        if (imm == 1)
            m_formatter.oneByteOp64(OP_GROUP2_Ev1, dst, GROUP2_OP_SHR);
        else {
            m_formatter.oneByteOp64(OP_GROUP2_EvIb, dst, GROUP2_OP_SHR);
            m_formatter.immediate8u(imm);
        }
    }
#endif

    void bsr_rr(RegisterID src, RegisterID dst)
    {
        spew("bsr        %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_BSR_GvEv, src, dst);
    }

    void imull_rr(RegisterID src, RegisterID dst)
    {
        spew("imull      %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_IMUL_GvEv, src, dst);
    }

    void imull_r(RegisterID multiplier)
    {
        spew("imull      %s", GPReg32Name(multiplier));
        m_formatter.oneByteOp(OP_GROUP3_Ev, multiplier, GROUP3_OP_IMUL);
    }

    void imull_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("imull      " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_IMUL_GvEv, offset, base, dst);
    }

    void imull_ir(int32_t value, RegisterID src, RegisterID dst)
    {
        if (shouldBlindConstant(value)){
            imull_ir_blnd(value, src, dst);
        } else {
            imull_ir_norm(value, src, dst);
        }
    }

    void imull_ir_blnd(int32_t value, RegisterID src, RegisterID dst)
    {
        BLND_FUNC;
        if (CAN_SIGN_EXTEND_8_32(value)) {
            imull_ir_norm(value, src, dst);     // TODO: is this ok?
        } else {
            int bv = 2;                         // TODO: FIX THIS!!!
            movl_rr(src, blindingReg);          // store src's value in blindingReg, as sometimes src == dst
            imull_ir_norm(value + bv, src, dst);
            imull_ir_norm(bv, blindingReg, blindingReg);
            subl_rr(blindingReg, dst);
        }
    }

    void imull_ir_norm(int32_t value, RegisterID src, RegisterID dst)
    {
        spew("imull      $%d, %s, %s", value, GPReg32Name(src), GPReg32Name(dst));
        if (CAN_SIGN_EXTEND_8_32(value)) {
            m_formatter.oneByteOp(OP_IMUL_GvEvIb, src, dst);
            m_formatter.immediate8s(value);
        } else {
            m_formatter.oneByteOp(OP_IMUL_GvEvIz, src, dst);
            m_formatter.immediate32(value);
        }
    }

    void idivl_r(RegisterID divisor)
    {
        spew("idivl      %s", GPReg32Name(divisor));
        m_formatter.oneByteOp(OP_GROUP3_Ev, divisor, GROUP3_OP_IDIV);
    }

    void divl_r(RegisterID divisor)
    {
        spew("div        %s", GPReg32Name(divisor));
        m_formatter.oneByteOp(OP_GROUP3_Ev, divisor, GROUP3_OP_DIV);
    }

    void prefix_lock()
    {
        spew("lock");
        m_formatter.oneByteOp(PRE_LOCK);
    }

    void prefix_16_for_32()
    {
        m_formatter.prefix(PRE_OPERAND_SIZE);
    }

    void incl_m32(int32_t offset, RegisterID base)
    {
        spew("incl       " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP5_Ev, offset, base, GROUP5_OP_INC);
    }

    void decl_m32(int32_t offset, RegisterID base)
    {
        spew("decl       " MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP5_Ev, offset, base, GROUP5_OP_DEC);
    }

    // Note that CMPXCHG performs comparison against REG = %al/%ax/%eax.
    // If %REG == [%base+offset], then %src -> [%base+offset].
    // Otherwise, [%base+offset] -> %REG.
    // For the 8-bit operations src must also be an 8-bit register.

    void cmpxchg8(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("cmpxchg8   %s, " MEM_ob, GPRegName(src), ADDR_ob(offset, base));
        m_formatter.twoByteOp(OP2_CMPXCHG_GvEb, offset, base, src);
    }
    void cmpxchg8(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpxchg8   %s, " MEM_obs, GPRegName(src), ADDR_obs(offset, base, index, scale));
        m_formatter.twoByteOp(OP2_CMPXCHG_GvEb, offset, base, index, scale, src);
    }
    void cmpxchg16(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("cmpxchg16  %s, " MEM_ob, GPRegName(src), ADDR_ob(offset, base));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.twoByteOp(OP2_CMPXCHG_GvEw, offset, base, src);
    }
    void cmpxchg16(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpxchg16  %s, " MEM_obs, GPRegName(src), ADDR_obs(offset, base, index, scale));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.twoByteOp(OP2_CMPXCHG_GvEw, offset, base, index, scale, src);
    }
    void cmpxchg32(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("cmpxchg32  %s, " MEM_ob, GPRegName(src), ADDR_ob(offset, base));
        m_formatter.twoByteOp(OP2_CMPXCHG_GvEw, offset, base, src);
    }
    void cmpxchg32(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpxchg32  %s, " MEM_obs, GPRegName(src), ADDR_obs(offset, base, index, scale));
        m_formatter.twoByteOp(OP2_CMPXCHG_GvEw, offset, base, index, scale, src);
    }


    // Comparisons:

    void cmpl_rr(RegisterID rhs, RegisterID lhs)
    {
        spew("cmpl       %s, %s", GPReg32Name(rhs), GPReg32Name(lhs));
        m_formatter.oneByteOp(OP_CMP_GvEv, rhs, lhs);
    }

    void cmpl_rm(RegisterID rhs, int32_t offset, RegisterID base)
    {
        spew("cmpl       %s, " MEM_ob, GPReg32Name(rhs), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_CMP_EvGv, offset, base, rhs);
    }

    void cmpl_mr(int32_t offset, RegisterID base, RegisterID lhs)
    {
        spew("cmpl       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(lhs));
        m_formatter.oneByteOp(OP_CMP_GvEv, offset, base, lhs);
    }

    void cmpl_mr(const void* address, RegisterID lhs)
    {
        spew("cmpl       %p, %s", address, GPReg32Name(lhs));
        m_formatter.oneByteOp(OP_CMP_GvEv, address, lhs);
    }

    void cmpl_ir(int32_t imm, RegisterID dst)
    {
        if (imm == 0) {
            testl_rr(dst, dst);
            return;
        }

	if (shouldBlindConstant(imm))
            cmpl_ir_blnd(imm, dst);
        else
            cmpl_ir_norm(imm, dst);
    }

    void cmpl_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpl_ir_norm(imm, dst);
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            cmpl_rr(blindingReg, dst);
        }
    }

    void cmpl_ir_norm(int32_t rhs, RegisterID lhs)
    {
        if (rhs == 0) {
            testl_rr(lhs, lhs);
            return;
        }

        spew("cmpl       $0x%x, %s", rhs, GPReg32Name(lhs));
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, lhs, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            if (lhs == rax)
                m_formatter.oneByteOp(OP_CMP_EAXIv);
            else
                m_formatter.oneByteOp(OP_GROUP1_EvIz, lhs, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }

    void cmpl_i32r(int32_t imm, RegisterID dst)
    {
	if (shouldBlindConstant(imm))
            cmpl_i32r_blnd(imm, dst);
        else
            cmpl_i32r_norm(imm, dst);
    }

    void cmpl_i32r_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpl_i32r_norm(imm, dst); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm ^ bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            cmpl_rr(blindingReg, dst);
        }
    }

    void cmpl_i32r_norm(int32_t rhs, RegisterID lhs)
    {
        spew("cmpl       $0x%04x, %s", rhs, GPReg32Name(lhs));
        if (lhs == rax)
            m_formatter.oneByteOp(OP_CMP_EAXIv);
        else
            m_formatter.oneByteOp(OP_GROUP1_EvIz, lhs, GROUP1_OP_CMP);
        m_formatter.immediate32(rhs);
    }

    void cmpl_im(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            cmpl_im_blnd(imm, offset, base);
        else
            cmpl_im_norm(imm, offset, base);
    }

    void cmpl_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpl_im_norm(imm, offset, base);    // TODO: is this ok?
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            cmpl_rm(blindingReg, offset, base);
        }
    }

    void cmpl_im_norm(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("cmpl       $0x%x, " MEM_ob, rhs, ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }

    void cmpb_rm(RegisterID src, int offset, RegisterID base)
    {
        spew("cmpb       %s, " MEM_ob,
             GPReg8Name(src), ADDR_ob(offset, base));
	m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_CMP_EvGv, offset, base, src);
    }

    void cmpb_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpb       %s, " MEM_obs,
             GPReg8Name(src), ADDR_obs(offset, base, index, scale));
	m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_CMP_EvGv, offset, base, index, scale, src);
    }

    void cmpb_im(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("cmpb       $0x%x, " MEM_ob, rhs, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP1_EbIb, offset, base, GROUP1_OP_CMP);
        m_formatter.immediate8(rhs);
    }

    void cmpb_im(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpb       $0x%x, " MEM_obs, rhs, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_GROUP1_EbIb, offset, base, index, scale, GROUP1_OP_CMP);
        m_formatter.immediate8(rhs);
    }

    void cmpl_rm(RegisterID src, int offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpl       %s, " MEM_obs,
             GPReg16Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_CMP_EvGv, offset, base, index, scale, src);
    }

    void cmpl_im(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
	if (shouldBlindConstant(imm))
	    cmpl_im_blnd(imm, offset, base, index, scale);
	else
	    cmpl_im_norm(imm, offset, base, index, scale);
    }

    void cmpl_im_blnd(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
	    cmpl_im_norm(imm, offset, base, index, scale); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            cmpl_rm(blindingReg, offset, base, index, scale);
        }
    }

    void cmpl_im_norm(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpl       $0x%x, " MEM_o32b, rhs, ADDR_o32b(offset, base));
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, index, scale, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, index, scale, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }

    /*
    MOZ_WARN_UNUSED_RESULT JmpSrc
    cmpl_im_disp32(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("cmpl       $0x%x, " MEM_o32b, rhs, ADDR_o32b(offset, base));
        JmpSrc r;
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp_disp32(OP_GROUP1_EvIb, offset, base, GROUP1_OP_CMP);
            r = JmpSrc(m_formatter.size());
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp_disp32(OP_GROUP1_EvIz, offset, base, GROUP1_OP_CMP);
            r = JmpSrc(m_formatter.size());
            m_formatter.immediate32(rhs);
        }
        return r;
    }
    
    MOZ_WARN_UNUSED_RESULT JmpSrc
    cmpl_im_disp32(int32_t rhs, const void* addr)
    {
        spew("cmpl       $0x%x, %p", rhs, addr);
        JmpSrc r;
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp_disp32(OP_GROUP1_EvIb, addr, GROUP1_OP_CMP);
            r = JmpSrc(m_formatter.size());
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp_disp32(OP_GROUP1_EvIz, addr, GROUP1_OP_CMP);
            r = JmpSrc(m_formatter.size());
            m_formatter.immediate32(rhs);
        }
        return r;
    }
    */

    void cmpl_i32m(int32_t rhs, int32_t offset, RegisterID base)
    {
	if (shouldBlindConstant(rhs))
	    cmpl_i32m_blnd(rhs, offset, base);
	else
	    cmpl_i32m_norm(rhs, offset, base);
    }

    void cmpl_i32m_blnd(int32_t rhs, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
	int bv = blindingValue();
	movl_i32r_norm(rhs ^ bv, blindingReg);
	xorl_ir_norm(bv, blindingReg);
	cmpl_rm(blindingReg, offset, base);
    }

    void cmpl_i32m_norm(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("cmpl       $0x%04x, " MEM_ob, rhs, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, GROUP1_OP_CMP);
        m_formatter.immediate32(rhs);
    }

    void cmpl_i32m(int32_t imm, const void* addr)
    {
	if (shouldBlindConstant(imm))
	    cmpl_i32m_blnd(imm, addr);
	else
	    cmpl_i32m_norm(imm, addr);
    }

    void cmpl_i32m_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movl_i32r_norm(imm ^ bv, blindingReg);
        xorl_ir_norm(bv, blindingReg);
        cmpl_rm(blindingReg, addr);
    }

    void cmpl_i32m_norm(int32_t rhs, const void* addr)
    {
        spew("cmpl       $0x%04x, %p", rhs, addr);
        m_formatter.oneByteOp(OP_GROUP1_EvIz, addr, GROUP1_OP_CMP);
        m_formatter.immediate32(rhs);
    }

#ifdef JS_CODEGEN_X64
    void cmpq_rr(RegisterID rhs, RegisterID lhs)
    {
        spew("cmpq       %s, %s", GPReg64Name(rhs), GPReg64Name(lhs));
        m_formatter.oneByteOp64(OP_CMP_GvEv, rhs, lhs);
    }

    void cmpq_rm(RegisterID rhs, int32_t offset, RegisterID base)
    {
        spew("cmpq       %s, " MEM_ob, GPReg64Name(rhs), ADDR_ob(offset, base));
        m_formatter.oneByteOp64(OP_CMP_EvGv, offset, base, rhs);
    }

    void cmpq_mr(int32_t offset, RegisterID base, RegisterID lhs)
    {
        spew("cmpq       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(lhs));
        m_formatter.oneByteOp64(OP_CMP_GvEv, offset, base, lhs);
    }

    void cmpq_ir(int32_t imm, RegisterID dst)
    {
        if (imm == 0) {
            testq_rr(dst, dst);
            return;
        }

	if (shouldBlindConstant(imm))
	    cmpq_ir_blnd(imm, dst);
	else
	    cmpq_ir_norm(imm, dst);
    }

    void cmpq_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpq_ir_norm(imm, dst);     // TODO: is this ok?
        } else {
            bv = blindingValue();
            movq_i32r_norm(imm^bv, blindingReg);
            xorq_ir_norm(bv, blindingReg);
            cmpq_rr(blindingReg, dst);
        }
    }

    void cmpq_ir_norm(int32_t rhs, RegisterID lhs)
    {
        if (rhs == 0) {
            testq_rr(lhs, lhs);
            return;
        }

        spew("cmpq       $0x%" PRIx64 ", %s", int64_t(rhs), GPReg64Name(lhs));
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, lhs, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            if (lhs == rax)
                m_formatter.oneByteOp64(OP_CMP_EAXIv);
            else
                m_formatter.oneByteOp64(OP_GROUP1_EvIz, lhs, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }

    void cmpq_im(int32_t imm, int32_t offset, RegisterID base)
    {
	if (shouldBlindConstant(imm))
	    cmpq_im_blnd(imm, offset, base);
	else
	    cmpq_im_norm(imm, offset, base);
    }

    void cmpq_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpq_im_norm(imm, offset, base); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movq_i32r_norm(imm ^ bv, blindingReg);
            xorq_ir_norm(bv, blindingReg);
            cmpq_rm(blindingReg, offset, base);
        }
    }

    void cmpq_im_norm(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("cmpq       $0x%" PRIx64 ", " MEM_ob, int64_t(rhs), ADDR_ob(offset, base));
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, offset, base, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, offset, base, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }

    void cmpq_im(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
	if (shouldBlindConstant(imm))
	    cmpq_im_blnd(imm, offset, base, index, scale);
	else
	    cmpq_im_norm(imm, offset, base, index, scale);
    }

    void cmpq_im_blnd(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpq_im_norm(imm, offset, base, index, scale); // TODO: is this ok?
        } else {
            bv = blindingValue();
            movq_i32r_norm(imm ^ bv, blindingReg);
            xorq_ir_norm(bv, blindingReg);
            cmpq_rm(blindingReg, offset, base, index, scale);
        }
    }

    void cmpq_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpl       %s, " MEM_obs, GPReg32Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp64(OP_CMP_EvGv, offset, base, index, scale, src);
    }

    void cmpq_im_norm(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpq       $0x%x, " MEM_obs, rhs, ADDR_obs(offset, base, index, scale));
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, offset, base, index, scale, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, offset, base, index, scale, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }
    void cmpq_im(int32_t imm, const void* addr)
    {
	if (shouldBlindConstant(imm))
            cmpq_im_blnd(imm, addr);
	else
            cmpq_im_norm(imm, addr);
    }

    void cmpq_im_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
	int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpq_im_norm(imm, addr); // TODO: is this ok?
	} else {
            bv = blindingValue();
            movq_i32r_norm(imm^bv, blindingReg);
            xorq_ir_norm(bv, blindingReg);
            cmpq_rm(blindingReg, addr);
	}
    }

    void cmpq_im_norm(int32_t rhs, const void* addr)
    {
        spew("cmpq       $0x%" PRIx64 ", %p", int64_t(rhs), addr);
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp64(OP_GROUP1_EvIb, addr, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp64(OP_GROUP1_EvIz, addr, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }
    void cmpq_rm(RegisterID rhs, const void* addr)
    {
        spew("cmpq       %s, %p", GPReg64Name(rhs), addr);
        m_formatter.oneByteOp64(OP_CMP_EvGv, addr, rhs);
    }
#endif
    void cmpl_rm(RegisterID rhs, const void* addr)
    {
        spew("cmpl       %s, %p", GPReg32Name(rhs), addr);
        m_formatter.oneByteOp(OP_CMP_EvGv, addr, rhs);
    }

    void cmpl_rm_disp32(RegisterID rhs, const void* addr)
    {
        spew("cmpl       %s, %p", GPReg32Name(rhs), addr);
        m_formatter.oneByteOp_disp32(OP_CMP_EvGv, addr, rhs);
    }

    void cmpl_im(int32_t imm, const void* addr)
    {
	if (shouldBlindConstant(imm))
	    cmpl_im_blnd(imm, addr);
	else
	    cmpl_im_norm(imm, addr);
    }

    void cmpl_im_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
	int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpl_im_norm(imm, addr); // TODO: is this ok?
	} else {
            bv = blindingValue();
            movl_i32r_norm(imm^bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            cmpl_rm(blindingReg, addr);
	}
    }

    void cmpl_im_norm(int32_t rhs, const void* addr)
    {
        spew("cmpl       $0x%x, %p", rhs, addr);
        if (CAN_SIGN_EXTEND_8_32(rhs)) {
            m_formatter.oneByteOp(OP_GROUP1_EvIb, addr, GROUP1_OP_CMP);
            m_formatter.immediate8s(rhs);
        } else {
            m_formatter.oneByteOp(OP_GROUP1_EvIz, addr, GROUP1_OP_CMP);
            m_formatter.immediate32(rhs);
        }
    }

    void cmpw_rr(RegisterID rhs, RegisterID lhs)
    {
        spew("cmpw       %s, %s", GPReg16Name(rhs), GPReg16Name(lhs));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_CMP_GvEv, rhs, lhs);
    }

    void cmpw_rm(RegisterID rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpw       %s, " MEM_obs, GPReg16Name(rhs), ADDR_obs(offset, base, index, scale));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_CMP_EvGv, offset, base, index, scale, rhs);
    }

    void cmpw_im(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
	if (shouldBlindConstant(imm))
	    cmpw_im_blnd(imm, offset, base, index, scale);
	else
	    cmpw_im_norm(imm, offset, base, index, scale);
    }

    void cmpw_im_blnd(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
        int bv;
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            cmpw_im_norm(imm, offset, base, index, scale);
        } else {
            bv = blindingValue();
            movl_i32r_norm(imm ^ bv, blindingReg);
            xorl_ir_norm(bv, blindingReg);
            cmpw_rm(blindingReg, offset, base, index, scale);
        }
    }

    void cmpw_im_norm(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("cmpw       $%d, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            m_formatter.prefix(PRE_OPERAND_SIZE);
            m_formatter.oneByteOp(OP_GROUP1_EvIb, offset, base, index, scale, GROUP1_OP_CMP);
            m_formatter.immediate8s(imm);
        } else {
            m_formatter.prefix(PRE_OPERAND_SIZE);
            m_formatter.oneByteOp(OP_GROUP1_EvIz, offset, base, index, scale, GROUP1_OP_CMP);
            m_formatter.immediate16(imm);
        }
    }

    void testl_rr(RegisterID rhs, RegisterID lhs)
    {
        spew("testl      %s, %s", GPReg32Name(rhs), GPReg32Name(lhs));
        m_formatter.oneByteOp(OP_TEST_EvGv, lhs, rhs);
    }

    void testb_rr(RegisterID rhs, RegisterID lhs)
    {
        spew("testb      %s, %s", GPReg8Name(rhs), GPReg8Name(lhs));
        m_formatter.oneByteOp(OP_TEST_EbGb, lhs, rhs);
    }

    void testl_ir(int32_t rhs, RegisterID lhs)
    {
        // If the mask fits in an 8-bit immediate, we can use testb with an
        // 8-bit subreg.
        if (CAN_ZERO_EXTEND_8_32(rhs) && HasSubregL(lhs)) {
            testb_ir(rhs, lhs);
            return;
        }
        // If the mask is a subset of 0xff00, we can use testb with an h reg, if
        // one happens to be available.
        if (CAN_ZERO_EXTEND_8H_32(rhs) && HasSubregH(lhs)) {
            testb_ir_norex(rhs >> 8, GetSubregH(lhs));
            return;
        }

	if (shouldBlindConstant(rhs))
	    testl_ir_blnd(rhs, lhs);
	else
	    testl_ir_norm(rhs, lhs);
    }

    void testl_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
	int bv = blindingValue();
	movl_i32r_norm(imm^bv, blindingReg);
	xorl_ir_norm(bv, blindingReg);
	testl_rr(blindingReg,  dst);
    }

    void testl_ir_norm(int32_t rhs, RegisterID lhs)
    {
        // If the mask fits in an 8-bit immediate, we can use testb with an
        // 8-bit subreg.
        if (CAN_ZERO_EXTEND_8_32(rhs) && HasSubregL(lhs)) {
            testb_ir(rhs, lhs);
            return;
        }
        // If the mask is a subset of 0xff00, we can use testb with an h reg, if
        // one happens to be available.
        if (CAN_ZERO_EXTEND_8H_32(rhs) && HasSubregH(lhs)) {
            testb_ir_norex(rhs >> 8, GetSubregH(lhs));
            return;
        }
        spew("testl      $0x%x, %s", rhs, GPReg32Name(lhs));
        if (lhs == rax)
            m_formatter.oneByteOp(OP_TEST_EAXIv);
        else
            m_formatter.oneByteOp(OP_GROUP3_EvIz, lhs, GROUP3_OP_TEST);
        m_formatter.immediate32(rhs);
    }

    void testl_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("testl       %s, " MEM_ob, GPReg32Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_TEST_EvGv, offset, base, src);
    }

    void testl_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("testl       %s, " MEM_obs, GPReg32Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_TEST_EvGv, offset, base, index, scale, src);
    }

    /* Custom testl_rm, not sure if right */
    void testl_rm(RegisterID src, const void* addr)
    {
        spew("testl       %s, %p", GPReg32Name(src), addr);
        m_formatter.oneByteOp(OP_TEST_EvGv, addr, src);
    }

    void testb_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("testb       %s, " MEM_ob, GPReg8Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_TEST_EbGb, offset, base, src);
    }

    void testb_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("testb       %s, " MEM_obs, GPReg32Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_TEST_EbGb, offset, base, index, scale, src);
    }

    void testl_i32m(int32_t rhs, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(rhs))
            testl_i32m_blnd(rhs, offset, base);
        else
            testl_i32m_norm(rhs, offset, base);
    }

    void testl_i32m_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movl_i32r_norm(imm^bv, blindingReg);
        xorl_ir_norm(bv, blindingReg);
        testl_rm(blindingReg, offset, base);
    }

    void testl_i32m_norm(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("testl      $0x%x, " MEM_ob, rhs, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP3_EvIz, offset, base, GROUP3_OP_TEST);
        m_formatter.immediate32(rhs);
    }

    void testl_i32m(int32_t imm, const void* addr)
    {
	if (shouldBlindConstant(imm))
	    testl_i32m_blnd(imm, addr);
	else
	    testl_i32m_norm(imm, addr);
    }

    void testl_i32m_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
	int bv = blindingValue();
        movl_i32r_norm(imm^bv, blindingReg);
        xorl_ir_norm(bv, blindingReg);
        testl_rm(blindingReg, addr);
    }

    void testl_i32m_norm(int32_t rhs, const void* addr)
    {
        spew("testl      $0x%x, %p", rhs, addr);
        m_formatter.oneByteOp(OP_GROUP3_EvIz, addr, GROUP3_OP_TEST);
        m_formatter.immediate32(rhs);
    }

    void testb_im(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("testb      $0x%x, " MEM_ob, rhs, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP3_EbIb, offset, base, GROUP3_OP_TEST);
        m_formatter.immediate8(rhs);
    }

    void testb_im(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("testb      $0x%x, " MEM_obs, rhs, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_GROUP3_EbIb, offset, base, index, scale, GROUP3_OP_TEST);
        m_formatter.immediate8(rhs);
    }

    void testl_i32m(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
	if (shouldBlindConstant(rhs))
	    testl_i32m_blnd(rhs, offset, base, index, scale);
	else
	    testl_i32m_norm(rhs, offset, base, index, scale);
    }

    void testl_i32m_blnd(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
	int bv = blindingValue();
	movl_i32r_norm(rhs^bv, blindingReg);
	xorl_ir_norm(bv, blindingReg);
	testl_rm(blindingReg, offset, base, index, scale);
    }

    void testl_i32m_norm(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("testl      $0x%4x, " MEM_obs, rhs, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_GROUP3_EvIz, offset, base, index, scale, GROUP3_OP_TEST);
        m_formatter.immediate32(rhs);
    }

#ifdef JS_CODEGEN_X64
    void testq_rr(RegisterID rhs, RegisterID lhs)
    {
        spew("testq      %s, %s", GPReg64Name(rhs), GPReg64Name(lhs));
        m_formatter.oneByteOp64(OP_TEST_EvGv, lhs, rhs);
    }

    void testq_ir(int32_t rhs, RegisterID lhs)
    {
        // If the mask fits in a 32-bit immediate, we can use testl with a
        // 32-bit subreg.
        if (CAN_ZERO_EXTEND_32_64(rhs)) {
            testl_ir(rhs, lhs);
            return;
        }

	if (shouldBlindConstant(rhs))
	    testq_ir_blnd(rhs, lhs);
	else
	    testq_ir_norm(rhs, lhs);
    }

    void testq_ir_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
	int bv = blindingValue();
	movq_i32r_norm(imm^bv, blindingReg);
	xorq_ir_norm(bv, blindingReg);
	testq_rr(blindingReg,  dst);
    }

    void testq_ir_norm(int32_t rhs, RegisterID lhs)
    {
        // If the mask fits in a 32-bit immediate, we can use testl with a
        // 32-bit subreg.
        if (CAN_ZERO_EXTEND_32_64(rhs)) {
            testl_ir(rhs, lhs);
            return;
        }
        spew("testq      $0x%" PRIx64 ", %s", int64_t(rhs), GPReg64Name(lhs));
        if (lhs == rax)
            m_formatter.oneByteOp64(OP_TEST_EAXIv);
        else
            m_formatter.oneByteOp64(OP_GROUP3_EvIz, lhs, GROUP3_OP_TEST);
        m_formatter.immediate32(rhs);
    }

    void testq_i32m(int32_t rhs, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(rhs))
            testq_i32m_blnd(rhs, offset, base);
        else
            testq_i32m_norm(rhs, offset, base);
    }

    void testq_i32m_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movq_i32r_norm(imm ^ bv, blindingReg);
        xorq_ir_norm(bv, blindingReg);
        testq_rm(blindingReg, offset, base);
    }

    void testq_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("testq      %s, " MEM_ob, GPReg64Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp64(OP_TEST_EvGv, offset, base, src);
    }

    void testq_i32m_norm(int32_t rhs, int32_t offset, RegisterID base)
    {
        spew("testq      $0x%" PRIx64 ", " MEM_ob, int64_t(rhs), ADDR_ob(offset, base));
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, offset, base, GROUP3_OP_TEST);
        m_formatter.immediate32(rhs);
    }

    void testq_i32m(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        if (shouldBlindConstant(rhs))
            testq_i32m_blnd(rhs, offset, base, index, scale);
        else
            testq_i32m_norm(rhs, offset, base, index, scale);
    }

    void testq_i32m_blnd(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movq_i32r_norm(imm ^ bv, blindingReg);
        xorq_ir_norm(bv, blindingReg);
        testq_rm(blindingReg, offset, base, index, scale);
    }

    void testq_i32m_norm(int32_t rhs, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("testq      $0x%4x, " MEM_obs, rhs, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp64(OP_GROUP3_EvIz, offset, base, index, scale, GROUP3_OP_TEST);
        m_formatter.immediate32(rhs);
    }

    void testq_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("testq      %s, " MEM_obs, GPReg64Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp64(OP_TEST_EvGv, offset, base, index, scale, src);
    }
#endif

    void testw_rr(RegisterID rhs, RegisterID lhs)
    {
        spew("testw      %s, %s", GPReg16Name(rhs), GPReg16Name(lhs));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_TEST_EvGv, lhs, rhs);
    }

    void testb_ir(int32_t rhs, RegisterID lhs)
    {
        spew("testb      $0x%x, %s", rhs, GPReg8Name(lhs));
        if (lhs == rax)
            m_formatter.oneByteOp8(OP_TEST_EAXIb);
        else
            m_formatter.oneByteOp8(OP_GROUP3_EbIb, lhs, GROUP3_OP_TEST);
        m_formatter.immediate8(rhs);
    }

    // Like testb_ir, but never emits a REX prefix. This may be used to
    // reference ah..bh.
    void testb_ir_norex(int32_t rhs, HRegisterID lhs)
    {
        spew("testb      $0x%x, %s", rhs, HRegName8(lhs));
        m_formatter.oneByteOp8_norex(OP_GROUP3_EbIb, lhs, GROUP3_OP_TEST);
        m_formatter.immediate8(rhs);
    }

    void setCC_r(Condition cond, RegisterID lhs)
    {
        spew("set%s      %s", CCName(cond), GPReg8Name(lhs));
        m_formatter.twoByteOp8(setccOpcode(cond), lhs, (GroupOpcodeID)0);
    }

    void sete_r(RegisterID dst)
    {
        setCC_r(ConditionE, dst);
    }

    void setz_r(RegisterID dst)
    {
        sete_r(dst);
    }

    void setne_r(RegisterID dst)
    {
        setCC_r(ConditionNE, dst);
    }

    void setnz_r(RegisterID dst)
    {
        setne_r(dst);
    }

    // Various move ops:

    void cdq()
    {
        spew("cdq        ");
        m_formatter.oneByteOp(OP_CDQ);
    }

    void xchgl_rr(RegisterID src, RegisterID dst)
    {
        spew("xchgl      %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_XCHG_GvEv, src, dst);
    }

#ifdef JS_CODEGEN_X64
    void xchgq_rr(RegisterID src, RegisterID dst)
    {
        spew("xchgq      %s, %s", GPReg64Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_XCHG_GvEv, src, dst);
    }
#endif

    void movl_rr(RegisterID src, RegisterID dst)
    {
        spew("movl       %s, %s", GPReg32Name(src), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_MOV_GvEv, src, dst);
    }

    void movw_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movw       %s, " MEM_ob, GPReg16Name(src), ADDR_ob(offset, base));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_MOV_EvGv, offset, base, src);
    }

    void movw_rm_disp32(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movw       %s, " MEM_o32b, GPReg16Name(src), ADDR_o32b(offset, base));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp_disp32(OP_MOV_EvGv, offset, base, src);
    }

    void movw_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movw       %s, " MEM_obs, GPReg16Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_MOV_EvGv, offset, base, index, scale, src);
    }

    void movw_rm(RegisterID src, const void* addr)
    {
        spew("movw       %s, %p", GPReg16Name(src), addr);
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp_disp32(OP_MOV_EvGv, addr, src);
    }

    void movl_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movl       %s, " MEM_ob, GPReg32Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_MOV_EvGv, offset, base, src);
    }

    void movl_rm_disp32(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movl       %s, " MEM_o32b, GPReg32Name(src), ADDR_o32b(offset, base));
        m_formatter.oneByteOp_disp32(OP_MOV_EvGv, offset, base, src);
    }

    void movl_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movl       %s, " MEM_obs, GPReg32Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_MOV_EvGv, offset, base, index, scale, src);
    }

    void movl_mEAX(const void* addr)
    {
#ifdef JS_CODEGEN_X64
        if (IsAddressImmediate(addr)) {
            movl_mr(addr, rax);
            return;
        }
#endif

#ifdef JS_CODEGEN_X64
        spew("movabs     %p, %%eax", addr);
#else
        spew("movl       %p, %%eax", addr);
#endif
        m_formatter.oneByteOp(OP_MOV_EAXOv);
#ifdef JS_CODEGEN_X64
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
#else
        m_formatter.immediate32(reinterpret_cast<int32_t>(addr));
#endif
    }

    void movl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movl       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_MOV_GvEv, offset, base, dst);
    }

    void movl_mr_disp32(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movl       " MEM_o32b ", %s", ADDR_o32b(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp_disp32(OP_MOV_GvEv, offset, base, dst);
    }

    void movl_mr(const void* base, RegisterID index, int scale, RegisterID dst)
    {
        int32_t disp = AddressImmediate(base);

        spew("movl       " MEM_os ", %s", ADDR_os(disp, index, scale), GPReg32Name(dst));
        m_formatter.oneByteOp_disp32(OP_MOV_GvEv, disp, index, scale, dst);
    }

    void movl_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movl       " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_MOV_GvEv, offset, base, index, scale, dst);
    }

    void movl_mr(const void* addr, RegisterID dst)
    {
        if (dst == rax
#ifdef JS_CODEGEN_X64
            && !IsAddressImmediate(addr)
#endif
            )
        {
            movl_mEAX(addr);
            return;
        }

        spew("movl       %p, %s", addr, GPReg32Name(dst));
        m_formatter.oneByteOp(OP_MOV_GvEv, addr, dst);
    }

    void movl_i32r(int32_t imm, RegisterID dst)
    {
        if (shouldBlindConstant(imm))
            movl_i32r_blnd(imm, dst);
        else
            movl_i32r_norm(imm, dst);
    }

    void movl_i32r_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movl_i32r_norm(imm ^ bv, dst);
        xorl_ir_norm(bv, dst);
    }

    void movl_i32r_norm(int32_t imm, RegisterID dst)
    {
        spew("movl       $0x%x, %s", imm, GPReg32Name(dst));
        m_formatter.oneByteOp(OP_MOV_EAXIv, dst);
        m_formatter.immediate32(imm);
    }

    void movb_ir(int32_t imm, RegisterID reg)
    {
        spew("movb       $0x%x, %s", imm, GPReg8Name(reg));
        m_formatter.oneByteOp(OP_MOV_EbGv, reg);
        m_formatter.immediate8(imm);
    }

    void movb_im(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("movb       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP11_EvIb, offset, base, GROUP11_MOV);
        m_formatter.immediate8(imm);
    }

    void movb_im(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movb       $0x%x, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_GROUP11_EvIb, offset, base, index, scale, GROUP11_MOV);
        m_formatter.immediate8(imm);
    }

    void movb_im(int32_t imm, const void* addr)
    {
        spew("movb       $%d, %p", imm, addr);
        m_formatter.oneByteOp_disp32(OP_GROUP11_EvIb, addr, GROUP11_MOV);
        m_formatter.immediate8(imm);
    }

    void movw_im(int32_t imm, int32_t offset, RegisterID base)
    {
    	if (shouldBlindConstant(imm))
            movw_im_blnd(imm, offset, base);
    	else
            movw_im_norm(imm, offset, base);
    }

    void movw_im_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
    	int bv = blindingValue16();
    	movw_im_norm(imm ^ bv, offset, base);
    	xorw_i16m(bv, offset, base);
    }

    void movw_im_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("movw       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_GROUP11_EvIz, offset, base, GROUP11_MOV);
        m_formatter.immediate16(imm);
    }

    void movw_im(int32_t imm, const void* addr)
    {
        if (shouldBlindConstant(imm))
            movw_im_blnd(imm, addr);
        else
            movw_im_norm(imm, addr);
    }

    void movw_im_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
        int bv = blindingValue16();
        movw_im_norm(imm-bv, addr);
        addq_im_norm(bv, addr);         // TODO: addq?
    }

    void movw_im_norm(int32_t imm, const void* addr)
    {
        spew("movw       $%d, %p", imm, addr);
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp_disp32(OP_GROUP11_EvIz, addr, GROUP11_MOV);
        m_formatter.immediate16(imm);
    }

    void movl_i32m(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            movl_i32m_blnd(imm, offset, base);
        else
            movl_i32m_norm(imm, offset, base);
    }

    void movl_i32m_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movl_i32m_norm(imm ^ bv, offset, base);
        xorl_im_norm(bv, offset, base);
    }

    void movl_i32m_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("movl       $0x%x, " MEM_ob, imm, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP11_EvIz, offset, base, GROUP11_MOV);
        m_formatter.immediate32(imm);
    }

    void movw_im(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        if (shouldBlindConstant(imm))
            movw_im_blnd(imm, offset, base, index, scale);
        else
            movw_im_norm(imm, offset, base, index, scale);
    }

    void movw_im_blnd(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
        int bv = blindingValue16();
        movw_im_norm(imm ^ bv, offset, base, index, scale);
        xorw_i16m(bv, offset, base, index, scale);
    }

    void movw_im_norm(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movw       $0x%x, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        m_formatter.prefix(PRE_OPERAND_SIZE);
        m_formatter.oneByteOp(OP_GROUP11_EvIz, offset, base, index, scale, GROUP11_MOV);
        m_formatter.immediate16(imm);
    }

    void movl_i32m(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        if (shouldBlindConstant(imm))
            movl_i32m_blnd(imm, offset, base, index, scale);
        else
            movl_i32m_norm(imm, offset, base, index, scale);
    }

    void movl_i32m_blnd(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movl_i32m_norm(imm ^ bv, offset, base, index, scale);
        xorl_i32m(bv, offset, base, index, scale);
    }

    void movl_i32m_norm(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movl       $0x%x, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_GROUP11_EvIz, offset, base, index, scale, GROUP11_MOV);
        m_formatter.immediate32(imm);
    }

    void movl_EAXm(const void* addr)
    {
#ifdef JS_CODEGEN_X64
        if (IsAddressImmediate(addr)) {
            movl_rm(rax, addr);
            return;
        }
#endif

        spew("movl       %%eax, %p", addr);
        m_formatter.oneByteOp(OP_MOV_OvEAX);
#ifdef JS_CODEGEN_X64
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
#else
        m_formatter.immediate32(reinterpret_cast<int32_t>(addr));
#endif
    }

#ifdef JS_CODEGEN_X64
    void movq_rr(RegisterID src, RegisterID dst)
    {
        spew("movq       %s, %s", GPReg64Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOV_GvEv, src, dst);
    }

    void movq_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movq       %s, " MEM_ob, GPReg64Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp64(OP_MOV_EvGv, offset, base, src);
    }

    void movq_rm_disp32(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movq       %s, " MEM_o32b, GPReg64Name(src), ADDR_o32b(offset, base));
        m_formatter.oneByteOp64_disp32(OP_MOV_EvGv, offset, base, src);
    }

    void movq_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movq       %s, " MEM_obs, GPReg64Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp64(OP_MOV_EvGv, offset, base, index, scale, src);
    }

    void movq_rm(RegisterID src, const void* addr)
    {
        if (src == rax && !IsAddressImmediate(addr)) {
            movq_EAXm(addr);
            return;
        }

        spew("movq       %s, %p", GPReg64Name(src), addr);
        m_formatter.oneByteOp64(OP_MOV_EvGv, addr, src);
    }
#endif

    void vmovq_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        // vmovq_rm can be encoded either as a true vmovq or as a vmovd with a
        // REX prefix modifying it to be 64-bit. We choose the vmovq encoding
        // because it's smaller (when it doesn't need a REX prefix for other
        // reasons) and because it works on 32-bit x86 too.
        twoByteOpSimd("vmovq", VEX_PD, OP2_MOVQ_WdVd, offset, base, invalid_xmm, src);
    }

    void vmovq_rm_disp32(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd_disp32("vmovq", VEX_PD, OP2_MOVQ_WdVd, offset, base, invalid_xmm, src);
    }

    void vmovq_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovq", VEX_PD, OP2_MOVQ_WdVd, offset, base, index, scale, invalid_xmm, src);
    }

    void vmovq_rm(XMMRegisterID src, const void* addr)
    {
        twoByteOpSimd("vmovq", VEX_PD, OP2_MOVQ_WdVd, addr, invalid_xmm, src);
    }

#ifdef JS_CODEGEN_X64
    void movq_mEAX(const void* addr)
    {
        if (IsAddressImmediate(addr)) {
            movq_mr(addr, rax);
            return;
        }

        spew("movq       %p, %%rax", addr);
        m_formatter.oneByteOp64(OP_MOV_EAXOv);
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
    }

    void movq_EAXm(const void* addr)
    {
        if (IsAddressImmediate(addr)) {
            movq_rm(rax, addr);
            return;
        }

        spew("movq       %%rax, %p", addr);
        m_formatter.oneByteOp64(OP_MOV_OvEAX);
        m_formatter.immediate64(reinterpret_cast<int64_t>(addr));
    }

    void movq_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movq       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOV_GvEv, offset, base, dst);
    }

    void movq_mr_disp32(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movq       " MEM_o32b ", %s", ADDR_o32b(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64_disp32(OP_MOV_GvEv, offset, base, dst);
    }

    void movq_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movq       " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOV_GvEv, offset, base, index, scale, dst);
    }

    void movq_mr(const void* addr, RegisterID dst)
    {
        if (dst == rax && !IsAddressImmediate(addr)) {
            movq_mEAX(addr);
            return;
        }

        spew("movq       %p, %s", addr, GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOV_GvEv, addr, dst);
    }
#endif

    void vmovq_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        // vmovq_mr can be encoded either as a true vmovq or as a vmovd with a
        // REX prefix modifying it to be 64-bit. We choose the vmovq encoding
        // because it's smaller (when it doesn't need a REX prefix for other
        // reasons) and because it works on 32-bit x86 too.
        twoByteOpSimd("vmovq", VEX_SS, OP2_MOVQ_VdWd, offset, base, invalid_xmm, dst);
    }

    void vmovq_mr_disp32(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd_disp32("vmovq", VEX_SS, OP2_MOVQ_VdWd, offset, base, invalid_xmm, dst);
    }

    void vmovq_mr(int32_t offset, RegisterID base, RegisterID index, int32_t scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovq", VEX_SS, OP2_MOVQ_VdWd, offset, base, index, scale, invalid_xmm, dst);
    }

    void vmovq_mr(const void* addr, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovq", VEX_SS, OP2_MOVQ_VdWd, addr, invalid_xmm, dst);
    }

#ifdef JS_CODEGEN_X64
    void leaq_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("leaq       " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg64Name(dst)),
        m_formatter.oneByteOp64(OP_LEA, offset, base, index, scale, dst);
    }

    void movq_i32m(int32_t imm, int32_t offset, RegisterID base)
    {
        if (shouldBlindConstant(imm))
            movq_i32m_blnd(imm, offset, base);
        else
            movq_i32m_norm(imm, offset, base);
    }

    void movq_i32m_blnd(int32_t imm, int32_t offset, RegisterID base)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movq_i32m_norm(imm ^ bv, offset, base);
        xorq_i32m(bv, offset, base);
    }

    void movq_i32m_norm(int32_t imm, int32_t offset, RegisterID base)
    {
        spew("movq       $%d, " MEM_ob, imm, ADDR_ob(offset, base));
        m_formatter.oneByteOp64(OP_GROUP11_EvIz, offset, base, GROUP11_MOV);
        m_formatter.immediate32(imm);
    }

    void movq_i32m(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        if (shouldBlindConstant(imm))
            movq_i32m_blnd(imm, offset, base, index, scale);
        else
            movq_i32m_norm(imm, offset, base, index, scale);
    }

    void movq_i32m_blnd(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movq_i32m_norm(imm ^ bv, offset, base, index, scale);
        xorq_i32m(imm, offset, base, index, scale);
    }

    void movq_i32m_norm(int32_t imm, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movq       $%d, " MEM_obs, imm, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp64(OP_GROUP11_EvIz, offset, base, index, scale, GROUP11_MOV);
        m_formatter.immediate32(imm);
    }

    void movq_i32m(int32_t imm, const void* addr)
    {
        if (shouldBlindConstant(imm))
            movq_i32m_blnd(imm, addr);
        else
            movq_i32m_norm(imm, addr);
    }

    void movq_i32m_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movq_i32m_norm(imm - bv, addr);
        addq_im_norm(bv, addr);
    }

    void movq_i32m_norm(int32_t imm, const void* addr)
    {
        spew("movq       $%d, %p", imm, addr);
        m_formatter.oneByteOp64(OP_GROUP11_EvIz, addr, GROUP11_MOV);
        m_formatter.immediate32(imm);
    }

    // Note that this instruction sign-extends its 32-bit immediate field to 64
    // bits and loads the 64-bit value into a 64-bit register.
    //
    // Note also that this is similar to the movl_i32r instruction, except that
    // movl_i32r *zero*-extends its 32-bit immediate, and it has smaller code
    // size, so it's preferred for values which could use either.
    void movq_i32r(int32_t imm, RegisterID dst) {
        if (shouldBlindConstant(imm))
            movq_i32r_blnd(imm, dst);
        else
            movq_i32r_norm(imm, dst);
    }

    void movq_i32r_blnd(int32_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int bv = blindingValue();
        movq_i32r_norm(imm ^ bv, dst);
        xorq_ir_norm(bv, dst);
    }

    void movq_i32r_norm(int32_t imm, RegisterID dst) {
        spew("movq       $%d, %s", imm, GPRegName(dst));
        m_formatter.oneByteOp64(OP_GROUP11_EvIz, dst, GROUP11_MOV);
        m_formatter.immediate32(imm);
    }

    void movq_i64r(int64_t imm, RegisterID dst) {
        if (shouldBlindConstant(imm))
            movq_i64r_norm(imm, dst);
        else
            movq_i64r_norm(imm, dst);
    }

    void movq_i64r_blnd(int64_t imm, RegisterID dst)
    {
        BLND_FUNC;
        int64_t bv = blindingValue64();
        movq_i64r_norm(imm^bv, dst);
        movq_i64r_norm(bv, blindingReg);
        xorq_rr(blindingReg, dst);
    }

    void movq_i64r_norm(int64_t imm, RegisterID dst)
    {
        spew("movabsq    $0x%" PRIx64 ", %s", imm, GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOV_EAXIv, dst);
        m_formatter.immediate64(imm);
    }

    void movslq_rr(RegisterID src, RegisterID dst)
    {
        spew("movslq     %s, %s", GPReg32Name(src), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOVSXD_GvEv, src, dst);
    }
    void movslq_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movslq     " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOVSXD_GvEv, offset, base, dst);
    }
    void movslq_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movslq     " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_MOVSXD_GvEv, offset, base, index, scale, dst);
    }

    MOZ_WARN_UNUSED_RESULT JmpSrc
    movl_ripr(RegisterID dst)
    {
        m_formatter.oneByteRipOp(OP_MOV_GvEv, 0, (RegisterID)dst);
        JmpSrc label(m_formatter.size());
        spew("movl       " MEM_o32r ", %s", ADDR_o32r(label.offset()), GPReg32Name(dst));
        return label;
    }

    MOZ_WARN_UNUSED_RESULT JmpSrc
    movl_rrip(RegisterID src)
    {
        m_formatter.oneByteRipOp(OP_MOV_EvGv, 0, (RegisterID)src);
        JmpSrc label(m_formatter.size());
        spew("movl       %s, " MEM_o32r "", GPReg32Name(src), ADDR_o32r(label.offset()));
        return label;
    }

    MOZ_WARN_UNUSED_RESULT JmpSrc
    movq_ripr(RegisterID dst)
    {
        m_formatter.oneByteRipOp64(OP_MOV_GvEv, 0, dst);
        JmpSrc label(m_formatter.size());
        spew("movq       " MEM_o32r ", %s", ADDR_o32r(label.offset()), GPRegName(dst));
        return label;
    }
#endif
    void movl_rm(RegisterID src, const void* addr)
    {
        if (src == rax
#ifdef JS_CODEGEN_X64
            && !IsAddressImmediate(addr)
#endif
            ) {
            movl_EAXm(addr);
            return;
        }

        spew("movl       %s, %p", GPReg32Name(src), addr);
        m_formatter.oneByteOp(OP_MOV_EvGv, addr, src);
    }

    void movl_i32m(int32_t imm, const void* addr)
    {
	if (shouldBlindConstant(imm))
	    movl_i32m_blnd(imm, addr);
	else
	    movl_i32m_norm(imm, addr);
    }

    void movl_i32m_blnd(int32_t imm, const void* addr)
    {
        BLND_FUNC;
	int bv = blindingValue();
	movl_i32m_norm(imm-bv, addr);
	addl_im_norm(bv, addr);
    }

    void movl_i32m_norm(int32_t imm, const void* addr)
    {
        spew("movl       $%d, %p", imm, addr);
        m_formatter.oneByteOp(OP_GROUP11_EvIz, addr, GROUP11_MOV);
        m_formatter.immediate32(imm);
    }

    void movb_rm(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movb       %s, " MEM_ob, GPReg8Name(src), ADDR_ob(offset, base));
        m_formatter.oneByteOp8(OP_MOV_EbGv, offset, base, src);
    }

    void movb_rm_disp32(RegisterID src, int32_t offset, RegisterID base)
    {
        spew("movb       %s, " MEM_o32b, GPReg8Name(src), ADDR_o32b(offset, base));
        m_formatter.oneByteOp8_disp32(OP_MOV_EbGv, offset, base, src);
    }

    void movb_rm(RegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        spew("movb       %s, " MEM_obs, GPReg8Name(src), ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp8(OP_MOV_EbGv, offset, base, index, scale, src);
    }

    void movb_rm(RegisterID src, const void* addr)
    {
        spew("movb       %s, %p", GPReg8Name(src), addr);
        m_formatter.oneByteOp8(OP_MOV_EbGv, addr, src);
    }

    void movb_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movb       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg8Name(dst));
        m_formatter.oneByteOp(OP_MOV_GvEb, offset, base, dst);
    }

    void movb_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movb       " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg8Name(dst));
        m_formatter.oneByteOp(OP_MOV_GvEb, offset, base, index, scale, dst);
    }

    void movzbl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movzbl     " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEb, offset, base, dst);
    }

    void movzbl_mr_disp32(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movzbl     " MEM_o32b ", %s", ADDR_o32b(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp_disp32(OP2_MOVZX_GvEb, offset, base, dst);
    }

    void movzbl_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movzbl     " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEb, offset, base, index, scale, dst);
    }

    void movzbl_mr(const void* addr, RegisterID dst)
    {
        spew("movzbl     %p, %s", addr, GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEb, addr, dst);
    }

    void movsbl_rr(RegisterID src, RegisterID dst)
    {
        spew("movsbl     %s, %s", GPReg8Name(src), GPReg32Name(dst));
        m_formatter.twoByteOp8_movx(OP2_MOVSX_GvEb, src, dst);
    }

    void movsbl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movsbl     " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEb, offset, base, dst);
    }

    void movsbl_mr_disp32(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movsbl     " MEM_o32b ", %s", ADDR_o32b(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp_disp32(OP2_MOVSX_GvEb, offset, base, dst);
    }

    void movsbl_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movsbl     " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEb, offset, base, index, scale, dst);
    }

    void movsbl_mr(const void* addr, RegisterID dst)
    {
        spew("movsbl     %p, %s", addr, GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEb, addr, dst);
    }

    void movzwl_rr(RegisterID src, RegisterID dst)
    {
        spew("movzwl     %s, %s", GPReg16Name(src), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, src, dst);
    }

    void movzwl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movzwl     " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, offset, base, dst);
    }

    void movzwl_mr_disp32(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movzwl     " MEM_o32b ", %s", ADDR_o32b(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp_disp32(OP2_MOVZX_GvEw, offset, base, dst);
    }

    void movzwl_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movzwl     " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, offset, base, index, scale, dst);
    }

    void movzwl_mr(const void* addr, RegisterID dst)
    {
        spew("movzwl     %p, %s", addr, GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVZX_GvEw, addr, dst);
    }

    void movswl_rr(RegisterID src, RegisterID dst)
    {
        spew("movswl     %s, %s", GPReg16Name(src), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEw, src, dst);
    }

    void movswl_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movswl     " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEw, offset, base, dst);
    }

    void movswl_mr_disp32(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("movswl     " MEM_o32b ", %s", ADDR_o32b(offset, base), GPReg32Name(dst));
        m_formatter.twoByteOp_disp32(OP2_MOVSX_GvEw, offset, base, dst);
    }

    void movswl_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("movswl     " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEw, offset, base, index, scale, dst);
    }

    void movswl_mr(const void* addr, RegisterID dst)
    {
        spew("movswl     %p, %s", addr, GPReg32Name(dst));
        m_formatter.twoByteOp(OP2_MOVSX_GvEw, addr, dst);
    }

    void movzbl_rr(RegisterID src, RegisterID dst)
    {
        spew("movzbl     %s, %s", GPReg8Name(src), GPReg32Name(dst));
        m_formatter.twoByteOp8_movx(OP2_MOVZX_GvEb, src, dst);
    }

    void leal_mr(int32_t offset, RegisterID base, RegisterID index, int scale, RegisterID dst)
    {
        spew("leal       " MEM_obs ", %s", ADDR_obs(offset, base, index, scale), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_LEA, offset, base, index, scale, dst);
    }

    void leal_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("leal       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.oneByteOp(OP_LEA, offset, base, dst);
    }
#ifdef JS_CODEGEN_X64
    void leaq_mr(int32_t offset, RegisterID base, RegisterID dst)
    {
        spew("leaq       " MEM_ob ", %s", ADDR_ob(offset, base), GPReg64Name(dst));
        m_formatter.oneByteOp64(OP_LEA, offset, base, dst);
    }

    MOZ_WARN_UNUSED_RESULT JmpSrc
    leaq_rip(RegisterID dst)
    {
        m_formatter.oneByteRipOp64(OP_LEA, 0, dst);
        JmpSrc label(m_formatter.size());
        spew("leaq       " MEM_o32r ", %s", ADDR_o32r(label.offset()), GPRegName(dst));
        return label;
    }
#endif

    // Flow control:

    MOZ_WARN_UNUSED_RESULT JmpSrc
    call()
    {
        m_formatter.oneByteOp(OP_CALL_rel32);
        JmpSrc r = m_formatter.immediateRel32();
        spew("call       .Lfrom%d", r.offset());
        return r;
    }

    void call_r(RegisterID dst)
    {
        m_formatter.oneByteOp(OP_GROUP5_Ev, dst, GROUP5_OP_CALLN);
        spew("call       *%s", GPRegName(dst));
    }

    void call_m(int32_t offset, RegisterID base)
    {
        spew("call       *" MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP5_Ev, offset, base, GROUP5_OP_CALLN);
    }

    // Comparison of EAX against a 32-bit immediate. The immediate is patched
    // in as if it were a jump target. The intention is to toggle the first
    // byte of the instruction between a CMP and a JMP to produce a pseudo-NOP.
    MOZ_WARN_UNUSED_RESULT JmpSrc
    cmp_eax()
    {
        m_formatter.oneByteOp(OP_CMP_EAXIv);
        JmpSrc r = m_formatter.immediateRel32();
        spew("cmpl       %%eax, .Lfrom%d", r.offset());
        return r;
    }

    void jmp_i(JmpDst dst)
    {
        int32_t diff = dst.offset() - m_formatter.size();
        spew("jmp        .Llabel%d", dst.offset());

        // The jump immediate is an offset from the end of the jump instruction.
        // A jump instruction is either 1 byte opcode and 1 byte offset, or 1
        // byte opcode and 4 bytes offset.
        if (CAN_SIGN_EXTEND_8_32(diff - 2)) {
            m_formatter.oneByteOp(OP_JMP_rel8);
            m_formatter.immediate8s(diff - 2);
        } else {
            m_formatter.oneByteOp(OP_JMP_rel32);
            m_formatter.immediate32(diff - 5);
        }
    }
    MOZ_WARN_UNUSED_RESULT JmpSrc
    jmp()
    {
        m_formatter.oneByteOp(OP_JMP_rel32);
        JmpSrc r = m_formatter.immediateRel32();
        spew("jmp        .Lfrom%d", r.offset());
        return r;
    }

    void jmp_r(RegisterID dst)
    {
        spew("jmp        *%s", GPRegName(dst));
        m_formatter.oneByteOp(OP_GROUP5_Ev, dst, GROUP5_OP_JMPN);
    }

    void jmp_m(int32_t offset, RegisterID base)
    {
        spew("jmp        *" MEM_ob, ADDR_ob(offset, base));
        m_formatter.oneByteOp(OP_GROUP5_Ev, offset, base, GROUP5_OP_JMPN);
    }

    void jmp_m(int32_t offset, RegisterID base, RegisterID index, int scale) {
        spew("jmp        *" MEM_obs, ADDR_obs(offset, base, index, scale));
        m_formatter.oneByteOp(OP_GROUP5_Ev, offset, base, index, scale, GROUP5_OP_JMPN);
    }

#ifdef JS_CODEGEN_X64
    void jmp_rip(int ripOffset) {
        // rip-relative addressing.
        spew("jmp        *%d(%%rip)", ripOffset);
        m_formatter.oneByteRipOp(OP_GROUP5_Ev, ripOffset, GROUP5_OP_JMPN);
    }

    void immediate64(int64_t imm)
    {
        spew(".quad      %lld", (long long)imm);
        m_formatter.immediate64(imm);
    }
#endif

    void jCC_i(Condition cond, JmpDst dst)
    {
        int32_t diff = dst.offset() - m_formatter.size();
        spew("j%s        .Llabel%d", CCName(cond), dst.offset());

        // The jump immediate is an offset from the end of the jump instruction.
        // A conditional jump instruction is either 1 byte opcode and 1 byte
        // offset, or 2 bytes opcode and 4 bytes offset.
        if (CAN_SIGN_EXTEND_8_32(diff - 2)) {
            m_formatter.oneByteOp(jccRel8(cond));
            m_formatter.immediate8s(diff - 2);
        } else {
            m_formatter.twoByteOp(jccRel32(cond));
            m_formatter.immediate32(diff - 6);
        }
    }

    MOZ_WARN_UNUSED_RESULT JmpSrc
    jCC(Condition cond)
    {
        m_formatter.twoByteOp(jccRel32(cond));
        JmpSrc r = m_formatter.immediateRel32();
        spew("j%s        .Lfrom%d", CCName(cond), r.offset());
        return r;
    }

    // SSE operations:

    void vpcmpeqw_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpcmpeqw", VEX_PD, OP2_PCMPEQW, src1, src0, dst);
    }

    void vpcmpeqd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpcmpeqd", VEX_PD, OP2_PCMPEQD_VdqWdq, src1, src0, dst);
    }
    void vpcmpeqd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpcmpeqd", VEX_PD, OP2_PCMPEQD_VdqWdq, offset, base, src0, dst);
    }
    void vpcmpeqd_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpcmpeqd", VEX_PD, OP2_PCMPEQD_VdqWdq, address, src0, dst);
    }

    void vpcmpgtd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpcmpgtd", VEX_PD, OP2_PCMPGTD_VdqWdq, src1, src0, dst);
    }
    void vpcmpgtd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpcmpgtd", VEX_PD, OP2_PCMPGTD_VdqWdq, offset, base, src0, dst);
    }
    void vpcmpgtd_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpcmpgtd", VEX_PD, OP2_PCMPGTD_VdqWdq, address, src0, dst);
    }

    void vcmpps_rr(uint8_t order, XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vcmpps", VEX_PS, OP2_CMPPS_VpsWps, order, src1, src0, dst);
    }
    void vcmpps_mr(uint8_t order, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vcmpps", VEX_PS, OP2_CMPPS_VpsWps, order, offset, base, src0, dst);
    }
    void vcmpps_mr(uint8_t order, const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vcmpps", VEX_PS, OP2_CMPPS_VpsWps, order, address, src0, dst);
    }

    void vrcpps_rr(XMMRegisterID src, XMMRegisterID dst) {
        twoByteOpSimd("vrcpps", VEX_PS, OP2_RCPPS_VpsWps, src, invalid_xmm, dst);
    }
    void vrcpps_mr(int32_t offset, RegisterID base, XMMRegisterID dst) {
        twoByteOpSimd("vrcpps", VEX_PS, OP2_RCPPS_VpsWps, offset, base, invalid_xmm, dst);
    }
    void vrcpps_mr(const void* address, XMMRegisterID dst) {
        twoByteOpSimd("vrcpps", VEX_PS, OP2_RCPPS_VpsWps, address, invalid_xmm, dst);
    }

    void vrsqrtps_rr(XMMRegisterID src, XMMRegisterID dst) {
        twoByteOpSimd("vrsqrtps", VEX_PS, OP2_RSQRTPS_VpsWps, src, invalid_xmm, dst);
    }
    void vrsqrtps_mr(int32_t offset, RegisterID base, XMMRegisterID dst) {
        twoByteOpSimd("vrsqrtps", VEX_PS, OP2_RSQRTPS_VpsWps, offset, base, invalid_xmm, dst);
    }
    void vrsqrtps_mr(const void* address, XMMRegisterID dst) {
        twoByteOpSimd("vrsqrtps", VEX_PS, OP2_RSQRTPS_VpsWps, address, invalid_xmm, dst);
    }

    void vsqrtps_rr(XMMRegisterID src, XMMRegisterID dst) {
        twoByteOpSimd("vsqrtps", VEX_PS, OP2_SQRTPS_VpsWps, src, invalid_xmm, dst);
    }
    void vsqrtps_mr(int32_t offset, RegisterID base, XMMRegisterID dst) {
        twoByteOpSimd("vsqrtps", VEX_PS, OP2_SQRTPS_VpsWps, offset, base, invalid_xmm, dst);
    }
    void vsqrtps_mr(const void* address, XMMRegisterID dst) {
        twoByteOpSimd("vsqrtps", VEX_PS, OP2_SQRTPS_VpsWps, address, invalid_xmm, dst);
    }

    void vaddsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddsd", VEX_SD, OP2_ADDSD_VsdWsd, src1, src0, dst);
    }

    void vaddss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddss", VEX_SS, OP2_ADDSD_VsdWsd, src1, src0, dst);
    }

    void vaddsd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddsd", VEX_SD, OP2_ADDSD_VsdWsd, offset, base, src0, dst);
    }

    void vaddss_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddss", VEX_SS, OP2_ADDSD_VsdWsd, offset, base, src0, dst);
    }

    void vaddsd_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddsd", VEX_SD, OP2_ADDSD_VsdWsd, address, src0, dst);
    }
    void vaddss_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vaddss", VEX_SS, OP2_ADDSD_VsdWsd, address, src0, dst);
    }

    void vcvtss2sd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtss2sd", VEX_SS, OP2_CVTSS2SD_VsdEd, src1, src0, dst);
    }

    void vcvtsd2ss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtsd2ss", VEX_SD, OP2_CVTSD2SS_VsdEd, src1, src0, dst);
    }

    void vcvtsi2ss_rr(RegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpInt32Simd("vcvtsi2ss", VEX_SS, OP2_CVTSI2SD_VsdEd, src1, src0, dst);
    }

    void vcvtsi2sd_rr(RegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpInt32Simd("vcvtsi2sd", VEX_SD, OP2_CVTSI2SD_VsdEd, src1, src0, dst);
    }

    void vcvttps2dq_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvttps2dq", VEX_SS, OP2_CVTTPS2DQ_VdqWps, src, invalid_xmm, dst);
    }

    void vcvtdq2ps_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtdq2ps", VEX_PS, OP2_CVTDQ2PS_VpsWdq, src, invalid_xmm, dst);
    }

#ifdef JS_CODEGEN_X64
    void vcvtsq2sd_rr(RegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpInt64Simd("vcvtsi2sd", VEX_SD, OP2_CVTSI2SD_VsdEd, src1, src0, dst);
    }
    void vcvtsq2ss_rr(RegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpInt64Simd("vcvtsi2ss", VEX_SS, OP2_CVTSI2SD_VsdEd, src1, src0, dst);
    }
#endif

    void vcvtsi2sd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtsi2sd", VEX_SD, OP2_CVTSI2SD_VsdEd, offset, base, src0, dst);
    }

    void vcvtsi2sd_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtsi2sd", VEX_SD, OP2_CVTSI2SD_VsdEd, offset, base, index, scale, src0, dst);
    }

    void vcvtsi2ss_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtsi2ss", VEX_SS, OP2_CVTSI2SD_VsdEd, offset, base, src0, dst);
    }

    void vcvtsi2ss_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtsi2ss", VEX_SS, OP2_CVTSI2SD_VsdEd, offset, base, index, scale, src0, dst);
    }

#ifdef JS_CODEGEN_X86
    void vcvtsi2sd_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vcvtsi2sd", VEX_SD, OP2_CVTSI2SD_VsdEd, address, src0, dst);
    }
#endif

    void vcvttsd2si_rr(XMMRegisterID src, RegisterID dst)
    {
        twoByteOpSimdInt32("vcvttsd2si", VEX_SD, OP2_CVTTSD2SI_GdWsd, src, dst);
    }

    void vcvttss2si_rr(XMMRegisterID src, RegisterID dst)
    {
        twoByteOpSimdInt32("vcvttss2si", VEX_SS, OP2_CVTTSD2SI_GdWsd, src, dst);
    }

#ifdef JS_CODEGEN_X64
    void vcvttsd2sq_rr(XMMRegisterID src, RegisterID dst)
    {
        twoByteOpSimdInt64("vcvttsd2si", VEX_SD, OP2_CVTTSD2SI_GdWsd, src, dst);
    }

    void vcvttss2sq_rr(XMMRegisterID src, RegisterID dst)
    {
        twoByteOpSimdInt64("vcvttss2si", VEX_SS, OP2_CVTTSD2SI_GdWsd, src, dst);
    }
#endif

    void vunpcklps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vunpcklps", VEX_PS, OP2_UNPCKLPS_VsdWsd, src1, src0, dst);
    }
    void vunpcklps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vunpcklps", VEX_PS, OP2_UNPCKLPS_VsdWsd, offset, base, src0, dst);
    }
    void vunpcklps_mr(const void* addr, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vunpcklps", VEX_PS, OP2_UNPCKLPS_VsdWsd, addr, src0, dst);
    }

    void vunpckhps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vunpckhps", VEX_PS, OP2_UNPCKHPS_VsdWsd, src1, src0, dst);
    }
    void vunpckhps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vunpckhps", VEX_PS, OP2_UNPCKHPS_VsdWsd, offset, base, src0, dst);
    }
    void vunpckhps_mr(const void* addr, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vunpckhps", VEX_PS, OP2_UNPCKHPS_VsdWsd, addr, src0, dst);
    }

    void vpand_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpand", VEX_PD, OP2_PANDDQ_VdqWdq, src1, src0, dst);
    }
    void vpand_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpand", VEX_PD, OP2_PANDDQ_VdqWdq, offset, base, src0, dst);
    }
    void vpand_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpand", VEX_PD, OP2_PANDDQ_VdqWdq, address, src0, dst);
    }
    void vpor_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpor", VEX_PD, OP2_PORDQ_VdqWdq, src1, src0, dst);
    }
    void vpor_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpor", VEX_PD, OP2_PORDQ_VdqWdq, offset, base, src0, dst);
    }
    void vpor_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpor", VEX_PD, OP2_PORDQ_VdqWdq, address, src0, dst);
    }
    void vpxor_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpxor", VEX_PD, OP2_PXORDQ_VdqWdq, src1, src0, dst);
    }
    void vpxor_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpxor", VEX_PD, OP2_PXORDQ_VdqWdq, offset, base, src0, dst);
    }
    void vpxor_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpxor", VEX_PD, OP2_PXORDQ_VdqWdq, address, src0, dst);
    }
    void vpandn_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpandn", VEX_PD, OP2_PANDNDQ_VdqWdq, src1, src0, dst);
    }
    void vpandn_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpandn", VEX_PD, OP2_PANDNDQ_VdqWdq, offset, base, src0, dst);
    }
    void vpandn_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpandn", VEX_PD, OP2_PANDNDQ_VdqWdq, address, src0, dst);
    }

    void vpshufd_irr(uint32_t mask, XMMRegisterID src, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vpshufd", VEX_PD, OP2_PSHUFD_VdqWdqIb, mask, src, invalid_xmm, dst);
    }
    void vpshufd_imr(uint32_t mask, int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vpshufd", VEX_PD, OP2_PSHUFD_VdqWdqIb, mask, offset, base, invalid_xmm, dst);
    }
    void vpshufd_imr(uint32_t mask, const void* address, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vpshufd", VEX_PD, OP2_PSHUFD_VdqWdqIb, mask, address, invalid_xmm, dst);
    }

    void vshufps_irr(uint32_t mask, XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vshufps", VEX_PS, OP2_SHUFPS_VpsWpsIb, mask, src1, src0, dst);
    }
    void vshufps_imr(uint32_t mask, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vshufps", VEX_PS, OP2_SHUFPS_VpsWpsIb, mask, offset, base, src0, dst);
    }
    void vshufps_imr(uint32_t mask, const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpImmSimd("vshufps", VEX_PS, OP2_SHUFPS_VpsWpsIb, mask, address, src0, dst);
    }

    void vmovhlps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovhlps", VEX_PS, OP2_MOVHLPS_VqUq, src1, src0, dst);
    }

    void vmovlhps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovlhps", VEX_PS, OP2_MOVLHPS_VqUq, src1, src0, dst);
    }

    void vpsrldq_ir(uint32_t count, XMMRegisterID src, XMMRegisterID dst)
    {
        MOZ_ASSERT(count < 16);
        shiftOpImmSimd("vpsrldq", OP2_PSRLDQ_Vd, ShiftID::vpsrldq, count, src, dst);
    }

    void vpsllq_ir(uint32_t count, XMMRegisterID src, XMMRegisterID dst)
    {
        MOZ_ASSERT(count < 64);
        shiftOpImmSimd("vpsllq", OP2_PSRLDQ_Vd, ShiftID::vpsllq, count, src, dst);
    }

    void vpsrlq_ir(uint32_t count, XMMRegisterID src, XMMRegisterID dst)
    {
        MOZ_ASSERT(count < 64);
        shiftOpImmSimd("vpsrlq", OP2_PSRLDQ_Vd, ShiftID::vpsrlq, count, src, dst);
    }

    void vpslld_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpslld", VEX_PD, OP2_PSLLD_VdqWdq, src1, src0, dst);
    }

    void vpslld_ir(uint32_t count, XMMRegisterID src, XMMRegisterID dst)
    {
        MOZ_ASSERT(count < 32);
        shiftOpImmSimd("vpslld", OP2_PSLLD_UdqIb, ShiftID::vpslld, count, src, dst);
    }

    void vpsrad_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpsrad", VEX_PD, OP2_PSRAD_VdqWdq, src1, src0, dst);
    }

    void vpsrad_ir(int32_t count, XMMRegisterID src, XMMRegisterID dst)
    {
        MOZ_ASSERT(count < 32);
        shiftOpImmSimd("vpsrad", OP2_PSRAD_UdqIb, ShiftID::vpsrad, count, src, dst);
    }

    void vpsrld_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vpsrld", VEX_PD, OP2_PSRLD_VdqWdq, src1, src0, dst);
    }

    void vpsrld_ir(uint32_t count, XMMRegisterID src, XMMRegisterID dst)
    {
        MOZ_ASSERT(count < 32);
        shiftOpImmSimd("vpsrld", OP2_PSRLD_UdqIb, ShiftID::vpsrld, count, src, dst);
    }

    void vmovmskpd_rr(XMMRegisterID src, RegisterID dst)
    {
        twoByteOpSimdInt32("vmovmskpd", VEX_PD, OP2_MOVMSKPD_EdVd, src, dst);
    }

    void vmovmskps_rr(XMMRegisterID src, RegisterID dst)
    {
        twoByteOpSimdInt32("vmovmskps", VEX_PS, OP2_MOVMSKPD_EdVd, src, dst);
    }

    void vptest_rr(XMMRegisterID rhs, XMMRegisterID lhs) {
        threeByteOpSimd("vptest", VEX_PD, OP3_PTEST_VdVd, ESCAPE_38, rhs, invalid_xmm, lhs);
    }

    void vmovd_rr(XMMRegisterID src, RegisterID dst)
    {
        twoByteOpSimdInt32("vmovd", VEX_PD, OP2_MOVD_EdVd, (XMMRegisterID)dst, (RegisterID)src);
    }

    void vmovd_rr(RegisterID src, XMMRegisterID dst)
    {
        twoByteOpInt32Simd("vmovd", VEX_PD, OP2_MOVD_VdEd, src, invalid_xmm, dst);
    }

    void vmovd_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovd", VEX_PD, OP2_MOVD_VdEd, offset, base, invalid_xmm, dst);
    }

    void vmovd_mr(int32_t offset, RegisterID base, RegisterID index, int32_t scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovd", VEX_PD, OP2_MOVD_VdEd, offset, base, index, scale, invalid_xmm, dst);
    }

    void vmovd_mr_disp32(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd_disp32("vmovd", VEX_PD, OP2_MOVD_VdEd, offset, base, invalid_xmm, dst);
    }

    void vmovd_mr(const void* address, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovd", VEX_PD, OP2_MOVD_VdEd, address, invalid_xmm, dst);
    }

    void vmovd_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd("vmovd", VEX_PD, OP2_MOVD_EdVd, offset, base, invalid_xmm, src);
    }

    void vmovd_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovd", VEX_PD, OP2_MOVD_EdVd, offset, base, index, scale, invalid_xmm, src);
    }

    void vmovd_rm_disp32(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd_disp32("vmovd", VEX_PD, OP2_MOVD_EdVd, offset, base, invalid_xmm, src);
    }

    void vmovd_rm(XMMRegisterID src, const void* address)
    {
        twoByteOpSimd("vmovd", VEX_PD, OP2_MOVD_EdVd, address, invalid_xmm, src);
    }

#ifdef JS_CODEGEN_X64
    void vmovq_rr(XMMRegisterID src, RegisterID dst)
    {
        // While this is called "vmovq", it actually uses the vmovd encoding
        // with a REX prefix modifying it to be 64-bit.
        twoByteOpSimdInt64("vmovq", VEX_PD, OP2_MOVD_EdVd, (XMMRegisterID)dst, (RegisterID)src);
    }

    void vmovq_rr(RegisterID src, XMMRegisterID dst)
    {
        // While this is called "vmovq", it actually uses the vmovd encoding
        // with a REX prefix modifying it to be 64-bit.
        twoByteOpInt64Simd("vmovq", VEX_PD, OP2_MOVD_VdEd, src, invalid_xmm, dst);
    }
#endif

    void vmovsd_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd("vmovsd", VEX_SD, OP2_MOVSD_WsdVsd, offset, base, invalid_xmm, src);
    }

    void vmovsd_rm_disp32(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd_disp32("vmovsd", VEX_SD, OP2_MOVSD_WsdVsd, offset, base, invalid_xmm, src);
    }

    void vmovss_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd("vmovss", VEX_SS, OP2_MOVSD_WsdVsd, offset, base, invalid_xmm, src);
    }

    void vmovss_rm_disp32(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd_disp32("vmovss", VEX_SS, OP2_MOVSD_WsdVsd, offset, base, invalid_xmm, src);
    }

    void vmovss_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovss", VEX_SS, OP2_MOVSD_VsdWsd, offset, base, invalid_xmm, dst);
    }

    void vmovss_mr_disp32(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd_disp32("vmovss", VEX_SS, OP2_MOVSD_VsdWsd, offset, base, invalid_xmm, dst);
    }

    void vmovsd_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovsd", VEX_SD, OP2_MOVSD_WsdVsd, offset, base, index, scale, invalid_xmm, src);
    }

    void vmovss_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovss", VEX_SS, OP2_MOVSD_WsdVsd, offset, base, index, scale, invalid_xmm, src);
    }

    void vmovss_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovss", VEX_SS, OP2_MOVSD_VsdWsd, offset, base, index, scale, invalid_xmm, dst);
    }

    void vmovsd_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovsd", VEX_SD, OP2_MOVSD_VsdWsd, offset, base, invalid_xmm, dst);
    }

    void vmovsd_mr_disp32(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd_disp32("vmovsd", VEX_SD, OP2_MOVSD_VsdWsd, offset, base, invalid_xmm, dst);
    }

    void vmovsd_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovsd", VEX_SD, OP2_MOVSD_VsdWsd, offset, base, index, scale, invalid_xmm, dst);
    }

    // Note that the register-to-register form of vmovsd does not write to the
    // entire output register. For general-purpose register-to-register moves,
    // use vmovapd instead.
    void vmovsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovsd", VEX_SD, OP2_MOVSD_VsdWsd, src1, src0, dst);
    }

    // The register-to-register form of vmovss has the same problem as vmovsd
    // above. Prefer vmovaps for register-to-register moves.
    void vmovss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovss", VEX_SS, OP2_MOVSD_VsdWsd, src1, src0, dst);
    }

    void vmovsd_mr(const void* address, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovsd", VEX_SD, OP2_MOVSD_VsdWsd, address, invalid_xmm, dst);
    }

    void vmovss_mr(const void* address, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovss", VEX_SS, OP2_MOVSD_VsdWsd, address, invalid_xmm, dst);
    }

    void vmovups_mr(const void* address, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovups", VEX_PS, OP2_MOVPS_VpsWps, address, invalid_xmm, dst);
    }

    void vmovdqu_mr(const void* address, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovdqu", VEX_SS, OP2_MOVDQ_VdqWdq, address, invalid_xmm, dst);
    }

    void vmovsd_rm(XMMRegisterID src, const void* address)
    {
        twoByteOpSimd("vmovsd", VEX_SD, OP2_MOVSD_WsdVsd, address, invalid_xmm, src);
    }

    void vmovss_rm(XMMRegisterID src, const void* address)
    {
        twoByteOpSimd("vmovss", VEX_SS, OP2_MOVSD_WsdVsd, address, invalid_xmm, src);
    }

    void vmovdqa_rm(XMMRegisterID src, const void* address)
    {
        twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_WdqVdq, address, invalid_xmm, src);
    }

    void vmovaps_rm(XMMRegisterID src, const void* address)
    {
        twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_WsdVsd, address, invalid_xmm, src);
    }

    void vmovdqu_rm(XMMRegisterID src, const void* address)
    {
        twoByteOpSimd("vmovdqu", VEX_SS, OP2_MOVDQ_WdqVdq, address, invalid_xmm, src);
    }

    void vmovups_rm(XMMRegisterID src, const void* address)
    {
        twoByteOpSimd("vmovups", VEX_PS, OP2_MOVPS_WpsVps, address, invalid_xmm, src);
    }
#ifdef JS_CODEGEN_X64
    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovsd_ripr(XMMRegisterID dst)
    {
        return twoByteRipOpSimd("vmovsd", VEX_SD, OP2_MOVSD_VsdWsd, invalid_xmm, dst);
    }
    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovss_ripr(XMMRegisterID dst)
    {
        return twoByteRipOpSimd("vmovss", VEX_SS, OP2_MOVSD_VsdWsd, invalid_xmm, dst);
    }
    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovsd_rrip(XMMRegisterID src)
    {
        return twoByteRipOpSimd("vmovsd", VEX_SD, OP2_MOVSD_WsdVsd, invalid_xmm, src);
    }
    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovss_rrip(XMMRegisterID src)
    {
        return twoByteRipOpSimd("vmovss", VEX_SS, OP2_MOVSD_WsdVsd, invalid_xmm, src);
    }
    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovdqa_rrip(XMMRegisterID src)
    {
        return twoByteRipOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_WdqVdq, invalid_xmm, src);
    }
    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovaps_rrip(XMMRegisterID src)
    {
        return twoByteRipOpSimd("vmovdqa", VEX_PS, OP2_MOVAPS_WsdVsd, invalid_xmm, src);
    }
#endif

    void vmovaps_rr(XMMRegisterID src, XMMRegisterID dst)
    {
#ifdef JS_CODEGEN_X64
        // There are two opcodes that can encode this instruction. If we have
        // one register in [xmm8,xmm15] and one in [xmm0,xmm7], use the
        // opcode which swaps the operands, as that way we can get a two-byte
        // VEX in that case.
        if (src >= xmm8 && dst < xmm8) {
            twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_WsdVsd, dst, invalid_xmm, src);
            return;
        }
#endif
        twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_VsdWsd, src, invalid_xmm, dst);
    }
    void vmovaps_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_WsdVsd, offset, base, invalid_xmm, src);
    }
    void vmovaps_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_WsdVsd, offset, base, index, scale, invalid_xmm, src);
    }
    void vmovaps_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_VsdWsd, offset, base, invalid_xmm, dst);
    }
    void vmovaps_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_VsdWsd, offset, base, index, scale, invalid_xmm, dst);
    }

    void vmovups_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd("vmovups", VEX_PS, OP2_MOVPS_WpsVps, offset, base, invalid_xmm, src);
    }
    void vmovups_rm_disp32(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd_disp32("vmovups", VEX_PS, OP2_MOVPS_WpsVps, offset, base, invalid_xmm, src);
    }
    void vmovups_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovups", VEX_PS, OP2_MOVPS_WpsVps, offset, base, index, scale, invalid_xmm, src);
    }
    void vmovups_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovups", VEX_PS, OP2_MOVPS_VpsWps, offset, base, invalid_xmm, dst);
    }
    void vmovups_mr_disp32(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd_disp32("vmovups", VEX_PS, OP2_MOVPS_VpsWps, offset, base, invalid_xmm, dst);
    }
    void vmovups_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovups", VEX_PS, OP2_MOVPS_VpsWps, offset, base, index, scale, invalid_xmm, dst);
    }

    void vmovapd_rr(XMMRegisterID src, XMMRegisterID dst)
    {
#ifdef JS_CODEGEN_X64
        // There are two opcodes that can encode this instruction. If we have
        // one register in [xmm8,xmm15] and one in [xmm0,xmm7], use the
        // opcode which swaps the operands, as that way we can get a two-byte
        // VEX in that case.
        if (src >= xmm8 && dst < xmm8) {
            twoByteOpSimd("vmovapd", VEX_PD, OP2_MOVAPS_WsdVsd, dst, invalid_xmm, src);
            return;
        }
#endif
        twoByteOpSimd("vmovapd", VEX_PD, OP2_MOVAPD_VsdWsd, src, invalid_xmm, dst);
    }

#ifdef JS_CODEGEN_X64
    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovaps_ripr(XMMRegisterID dst)
    {
        return twoByteRipOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_VsdWsd, invalid_xmm, dst);
    }

    MOZ_WARN_UNUSED_RESULT JmpSrc
    vmovdqa_ripr(XMMRegisterID dst)
    {
        return twoByteRipOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_VdqWdq, invalid_xmm, dst);
    }
#else
    void vmovaps_mr(const void* address, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovaps", VEX_PS, OP2_MOVAPS_VsdWsd, address, invalid_xmm, dst);
    }

    void vmovdqa_mr(const void* address, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_VdqWdq, address, invalid_xmm, dst);
    }
#endif // JS_CODEGEN_X64

    void vmovdqu_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd("vmovdqu", VEX_SS, OP2_MOVDQ_WdqVdq, offset, base, invalid_xmm, src);
    }

    void vmovdqu_rm_disp32(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd_disp32("vmovdqu", VEX_SS, OP2_MOVDQ_WdqVdq, offset, base, invalid_xmm, src);
    }

    void vmovdqu_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovdqu", VEX_SS, OP2_MOVDQ_WdqVdq, offset, base, index, scale, invalid_xmm, src);
    }

    void vmovdqu_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovdqu", VEX_SS, OP2_MOVDQ_VdqWdq, offset, base, invalid_xmm, dst);
    }

    void vmovdqu_mr_disp32(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd_disp32("vmovdqu", VEX_SS, OP2_MOVDQ_VdqWdq, offset, base, invalid_xmm, dst);
    }

    void vmovdqu_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovdqu", VEX_SS, OP2_MOVDQ_VdqWdq, offset, base, index, scale, invalid_xmm, dst);
    }

    void vmovdqa_rr(XMMRegisterID src, XMMRegisterID dst)
    {
#ifdef JS_CODEGEN_X64
        // There are two opcodes that can encode this instruction. If we have
        // one register in [xmm8,xmm15] and one in [xmm0,xmm7], use the
        // opcode which swaps the operands, as that way we can get a two-byte
        // VEX in that case.
        if (src >= xmm8 && dst < xmm8) {
            twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_WdqVdq, dst, invalid_xmm, src);
            return;
        }
#endif
        twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_VdqWdq, src, invalid_xmm, dst);
    }

    void vmovdqa_rm(XMMRegisterID src, int32_t offset, RegisterID base)
    {
        twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_WdqVdq, offset, base, invalid_xmm, src);
    }

    void vmovdqa_rm(XMMRegisterID src, int32_t offset, RegisterID base, RegisterID index, int scale)
    {
        twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_WdqVdq, offset, base, index, scale, invalid_xmm, src);
    }

    void vmovdqa_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {

        twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_VdqWdq, offset, base, invalid_xmm, dst);
    }

    void vmovdqa_mr(int32_t offset, RegisterID base, RegisterID index, int scale, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovdqa", VEX_PD, OP2_MOVDQ_VdqWdq, offset, base, index, scale, invalid_xmm, dst);
    }

    void vmulsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmulsd", VEX_SD, OP2_MULSD_VsdWsd, src1, src0, dst);
    }

    void vmulss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmulss", VEX_SS, OP2_MULSD_VsdWsd, src1, src0, dst);
    }

    void vmulsd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmulsd", VEX_SD, OP2_MULSD_VsdWsd, offset, base, src0, dst);
    }

    void vmulss_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmulss", VEX_SS, OP2_MULSD_VsdWsd, offset, base, src0, dst);
    }

    void vpextrw_irr(uint32_t whichWord, XMMRegisterID src, RegisterID dst)
    {
        MOZ_ASSERT(whichWord < 8);
        twoByteOpImmSimdInt32("vpextrw", VEX_PD, OP2_PEXTRW_GdUdIb, whichWord, src, dst);
    }

    void vsubsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsubsd", VEX_SD, OP2_SUBSD_VsdWsd, src1, src0, dst);
    }

    void vsubss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsubss", VEX_SS, OP2_SUBSD_VsdWsd, src1, src0, dst);
    }

    void vsubsd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsubsd", VEX_SD, OP2_SUBSD_VsdWsd, offset, base, src0, dst);
    }

    void vsubss_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsubss", VEX_SS, OP2_SUBSD_VsdWsd, offset, base, src0, dst);
    }

    void vucomiss_rr(XMMRegisterID rhs, XMMRegisterID lhs)
    {
        twoByteOpSimdFlags("vucomiss", VEX_PS, OP2_UCOMISD_VsdWsd, rhs, lhs);
    }

    void vucomisd_rr(XMMRegisterID rhs, XMMRegisterID lhs)
    {
        twoByteOpSimdFlags("vucomisd", VEX_PD, OP2_UCOMISD_VsdWsd, rhs, lhs);
    }

    void vucomisd_mr(int32_t offset, RegisterID base, XMMRegisterID lhs)
    {
        twoByteOpSimdFlags("vucomisd", VEX_PD, OP2_UCOMISD_VsdWsd, offset, base, lhs);
    }

    void vdivsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vdivsd", VEX_SD, OP2_DIVSD_VsdWsd, src1, src0, dst);
    }

    void vdivss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vdivss", VEX_SS, OP2_DIVSD_VsdWsd, src1, src0, dst);
    }

    void vdivsd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vdivsd", VEX_SD, OP2_DIVSD_VsdWsd, offset, base, src0, dst);
    }

    void vdivss_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vdivss", VEX_SS, OP2_DIVSD_VsdWsd, offset, base, src0, dst);
    }

    void vxorpd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vxorpd", VEX_PD, OP2_XORPD_VpdWpd, src1, src0, dst);
    }

    void vorpd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vorpd", VEX_PD, OP2_ORPD_VpdWpd, src1, src0, dst);
    }

    void vandpd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vandpd", VEX_PD, OP2_ANDPD_VpdWpd, src1, src0, dst);
    }

    void vandps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vandps", VEX_PS, OP2_ANDPS_VpsWps, src1, src0, dst);
    }

    void vandps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vandps", VEX_PS, OP2_ANDPS_VpsWps, offset, base, src0, dst);
    }

    void vandps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vandps", VEX_PS, OP2_ANDPS_VpsWps, address, src0, dst);
    }

    void vandnps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vandnps", VEX_PS, OP2_ANDNPS_VpsWps, src1, src0, dst);
    }

    void vandnps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vandnps", VEX_PS, OP2_ANDNPS_VpsWps, offset, base, src0, dst);
    }

    void vandnps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vandnps", VEX_PS, OP2_ANDNPS_VpsWps, address, src0, dst);
    }

    void vorps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vorps", VEX_PS, OP2_ORPS_VpsWps, src1, src0, dst);
    }

    void vorps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vorps", VEX_PS, OP2_ORPS_VpsWps, offset, base, src0, dst);
    }

    void vorps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vorps", VEX_PS, OP2_ORPS_VpsWps, address, src0, dst);
    }

    void vxorps_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vxorps", VEX_PS, OP2_XORPS_VpsWps, src1, src0, dst);
    }

    void vxorps_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vxorps", VEX_PS, OP2_XORPS_VpsWps, offset, base, src0, dst);
    }

    void vxorps_mr(const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vxorps", VEX_PS, OP2_XORPS_VpsWps, address, src0, dst);
    }

    void vsqrtsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsqrtsd", VEX_SD, OP2_SQRTSD_VsdWsd, src1, src0, dst);
    }

    void vsqrtss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vsqrtss", VEX_SS, OP2_SQRTSS_VssWss, src1, src0, dst);
    }

    void vroundsd_irr(RoundingMode mode, XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        threeByteOpImmSimd("vroundsd", VEX_PD, OP3_ROUNDSD_VsdWsd, ESCAPE_3A, mode, src1, src0, dst);
    }

    void vroundss_irr(RoundingMode mode, XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        threeByteOpImmSimd("vroundss", VEX_PD, OP3_ROUNDSS_VsdWsd, ESCAPE_3A, mode, src1, src0, dst);
    }

    void vinsertps_irr(uint32_t mask, XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        threeByteOpImmSimd("vinsertps", VEX_PD, OP3_INSERTPS_VpsUps, ESCAPE_3A, mask, src1, src0, dst);
    }
    void vinsertps_imr(uint32_t mask, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        threeByteOpImmSimd("vinsertps", VEX_PD, OP3_INSERTPS_VpsUps, ESCAPE_3A, mask, offset, base, src0, dst);
    }

    void vpinsrd_irr(unsigned lane, RegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        MOZ_ASSERT(lane < 4);
        threeByteOpImmInt32Simd("vpinsrd", VEX_PD, OP3_PINSRD_VdqEdIb, ESCAPE_3A, lane, src1, src0, dst);
    }

    void vpinsrd_imr(unsigned lane, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        MOZ_ASSERT(lane < 4);
        threeByteOpImmInt32Simd("vpinsrd", VEX_PD, OP3_PINSRD_VdqEdIb, ESCAPE_3A, lane, offset, base, src0, dst);
    }

    void vpextrd_irr(unsigned lane, XMMRegisterID src, RegisterID dst)
    {
        MOZ_ASSERT(lane < 4);
        threeByteOpImmSimdInt32("vpextrd", VEX_PD, OP3_PEXTRD_EdVdqIb, ESCAPE_3A, lane, (XMMRegisterID)dst, (RegisterID)src);
    }

    void vpextrd_irm(unsigned lane, XMMRegisterID src, int32_t offset, RegisterID base)
    {
        MOZ_ASSERT(lane < 4);
        spew("pextrd     $0x%x, %s, " MEM_ob, lane, XMMRegName(src), ADDR_ob(offset, base));
        m_formatter.prefix(PRE_SSE_66);
        m_formatter.threeByteOp(OP3_PEXTRD_EdVdqIb, ESCAPE_3A, offset, base, (RegisterID)src);
        m_formatter.immediate8u(lane);
    }

    void vblendps_irr(unsigned imm, XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        MOZ_ASSERT(imm < 16);
        // Despite being a "ps" instruction, vblendps is encoded with the "pd" prefix.
        threeByteOpImmSimd("vblendps", VEX_PD, OP3_BLENDPS_VpsWpsIb, ESCAPE_3A, imm, src1, src0, dst);
    }

    void vblendps_imr(unsigned imm, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        MOZ_ASSERT(imm < 16);
        // Despite being a "ps" instruction, vblendps is encoded with the "pd" prefix.
threeByteOpImmSimd("vblendps", VEX_PD, OP3_BLENDPS_VpsWpsIb, ESCAPE_3A, imm, offset, base, src0, dst);
    }

    void vblendvps_rr(XMMRegisterID mask, XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst) {
        vblendvOpSimd(mask, src1, src0, dst);
    }
    void vblendvps_mr(XMMRegisterID mask, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst) {
        vblendvOpSimd(mask, offset, base, src0, dst);
    }

    void vmovsldup_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovsldup", VEX_SS, OP2_MOVSLDUP_VpsWps, src, invalid_xmm, dst);
    }
    void vmovsldup_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovsldup", VEX_SS, OP2_MOVSLDUP_VpsWps, offset, base, invalid_xmm, dst);
    }

    void vmovshdup_rr(XMMRegisterID src, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovshdup", VEX_SS, OP2_MOVSHDUP_VpsWps, src, invalid_xmm, dst);
    }
    void vmovshdup_mr(int32_t offset, RegisterID base, XMMRegisterID dst)
    {
        twoByteOpSimd("vmovshdup", VEX_SS, OP2_MOVSHDUP_VpsWps, offset, base, invalid_xmm, dst);
    }

    void vminsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vminsd", VEX_SD, OP2_MINSD_VsdWsd, src1, src0, dst);
    }
    void vminsd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vminsd", VEX_SD, OP2_MINSD_VsdWsd, offset, base, src0, dst);
    }

    void vminss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vminss", VEX_SS, OP2_MINSS_VssWss, src1, src0, dst);
    }

    void vmaxsd_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmaxsd", VEX_SD, OP2_MAXSD_VsdWsd, src1, src0, dst);
    }
    void vmaxsd_mr(int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmaxsd", VEX_SD, OP2_MAXSD_VsdWsd, offset, base, src0, dst);
    }

    void vmaxss_rr(XMMRegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        twoByteOpSimd("vmaxss", VEX_SS, OP2_MAXSS_VssWss, src1, src0, dst);
    }

    // Misc instructions:

    void int3()
    {
        spew("int3");
        m_formatter.oneByteOp(OP_INT3);
    }

    void ud2()
    {
        spew("ud2");
        m_formatter.twoByteOp(OP2_UD2);
    }

    void ret()
    {
        spew("ret");
        m_formatter.oneByteOp(OP_RET);
    }

    void ret_i(int32_t imm)
    {
        spew("ret        $%d", imm);
        m_formatter.oneByteOp(OP_RET_Iz);
        m_formatter.immediate16u(imm);
    }

#ifdef JS_CODEGEN_X86
    void pusha()
    {
        spew("pusha");
        m_formatter.oneByteOp(OP_PUSHA);
    }

    void popa()
    {
        spew("popa");
        m_formatter.oneByteOp(OP_POPA);
    }
#endif

    void mfence() {
        spew("mfence");
        m_formatter.twoByteOp(OP_FENCE, (RegisterID)0, 6);
    }

    // Assembler admin methods:

    JmpDst label()
    {
        JmpDst r = JmpDst(m_formatter.size());
        spew(".set .Llabel%d, .", r.offset());
        return r;
    }

    size_t currentOffset() const {
        return m_formatter.size();
    }

    static JmpDst labelFor(JmpSrc jump, intptr_t offset = 0)
    {
        return JmpDst(jump.offset() + offset);
    }

    void align(int alignment)
    {
        spew(".balign %d, 0x%x   # hlt", alignment, OP_HLT);
        while (!m_formatter.isAligned(alignment))
            m_formatter.oneByteOp(OP_HLT);
    }

    void jumpTablePointer(uintptr_t ptr)
    {
#ifdef JS_CODEGEN_X64
        spew(".quad 0x%" PRIxPTR, ptr);
#else
        spew(".int 0x%" PRIxPTR, ptr);
#endif
        m_formatter.jumpTablePointer(ptr);
    }

    void doubleConstant(double d)
    {
        spew(".double %.20f", d);
        m_formatter.doubleConstant(d);
    }
    void floatConstant(float f)
    {
        spew(".float %.20f", f);
        m_formatter.floatConstant(f);
    }

    void int32x4Constant(const int32_t s[4])
    {
        spew(".int %d,%d,%d,%d", s[0], s[1], s[2], s[3]);
        MOZ_ASSERT(m_formatter.isAligned(16));
        m_formatter.int32x4Constant(s);
    }
    void float32x4Constant(const float f[4])
    {
        spew(".float %f,%f,%f,%f", f[0], f[1], f[2], f[3]);
        MOZ_ASSERT(m_formatter.isAligned(16));
        m_formatter.float32x4Constant(f);
    }

    void int64Constant(int64_t i)
    {
        spew(".quad %lld", (long long)i);
        m_formatter.int64Constant(i);
    }

    // Linking & patching:

    bool nextJump(const JmpSrc& from, JmpSrc* next)
    {
        // Sanity check - if the assembler has OOM'd, it will start overwriting
        // its internal buffer and thus our links could be garbage.
        if (oom())
            return false;

        const unsigned char* code = m_formatter.data();
        int32_t offset = GetInt32(code + from.offset());
        if (offset == -1)
            return false;
        *next = JmpSrc(offset);
        return true;
    }
    void setNextJump(const JmpSrc& from, const JmpSrc& to)
    {
        // Sanity check - if the assembler has OOM'd, it will start overwriting
        // its internal buffer and thus our links could be garbage.
        if (oom())
            return;

        unsigned char* code = m_formatter.data();
        SetInt32(code + from.offset(), to.offset());
    }

    void linkJump(JmpSrc from, JmpDst to)
    {
        MOZ_ASSERT(from.offset() != -1);
        MOZ_ASSERT(to.offset() != -1);

        // Sanity check - if the assembler has OOM'd, it will start overwriting
        // its internal buffer and thus our links could be garbage.
        if (oom())
            return;

        spew(".set .Lfrom%d, .Llabel%d", from.offset(), to.offset());
        unsigned char* code = m_formatter.data();
        SetRel32(code + from.offset(), code + to.offset());
    }

    void executableCopy(void* buffer)
    {
        memcpy(buffer, m_formatter.buffer(), size());
    }

  private:
    static bool CAN_SIGN_EXTEND_8_32(int32_t value) { return value == (int32_t)(int8_t)value; }
    static bool CAN_SIGN_EXTEND_16_32(int32_t value) { return value == (int32_t)(int16_t)value; }
    static bool CAN_ZERO_EXTEND_8_32(int32_t value) { return value == (int32_t)(uint8_t)value; }
    static bool CAN_ZERO_EXTEND_8H_32(int32_t value) { return value == (value & 0xff00); }
    static bool CAN_ZERO_EXTEND_16_32(int32_t value) { return value == (int32_t)(uint16_t)value; }
    static bool CAN_ZERO_EXTEND_32_64(int32_t value) { return value >= 0; }

    // Methods for encoding SIMD instructions via either legacy SSE encoding or
    // VEX encoding.

    bool useLegacySSEEncoding(XMMRegisterID src0, XMMRegisterID dst)
    {
        // If we don't have AVX or it's disabled, use the legacy SSE encoding.
        if (!useVEX_) {
            MOZ_ASSERT(src0 == invalid_xmm || src0 == dst,
                       "Legacy SSE (pre-AVX) encoding requires the output register to be "
                       "the same as the src0 input register");
            return true;
        }

        // If src0 is the same as the output register, we might as well use
        // the legacy SSE encoding, since it is smaller. However, this is only
        // beneficial as long as we're not using ymm registers anywhere.
        return src0 == dst;
    }

    bool useLegacySSEEncodingForVblendv(XMMRegisterID mask, XMMRegisterID src0, XMMRegisterID dst)
    {
        // Similar to useLegacySSEEncoding, but for vblendv the Legacy SSE
        // encoding also requires the mask to be in xmm0.

        if (!useVEX_) {
            MOZ_ASSERT(src0 == dst,
                       "Legacy SSE (pre-AVX) encoding requires the output register to be "
                       "the same as the src0 input register");
            MOZ_ASSERT(mask == xmm0,
                       "Legacy SSE (pre-AVX) encoding for blendv requires the mask to be "
                       "in xmm0");
            return true;
        }

        return src0 == dst && mask == xmm0;
    }

    bool useLegacySSEEncodingForOtherOutput()
    {
        return !useVEX_;
    }

    const char* legacySSEOpName(const char* name)
    {
        MOZ_ASSERT(name[0] == 'v');
        return name + 1;
    }

 #ifdef JS_CODEGEN_X64
    MOZ_WARN_UNUSED_RESULT JmpSrc
    twoByteRipOpSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                     XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteRipOp(opcode, 0, dst);
            JmpSrc label(m_formatter.size());
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, " MEM_o32r "", legacySSEOpName(name), XMMRegName(dst), ADDR_o32r(label.offset()));
            else
                spew("%-11s" MEM_o32r ", %s", legacySSEOpName(name), ADDR_o32r(label.offset()), XMMRegName(dst));
            return label;
        }

        m_formatter.twoByteRipOpVex(ty, opcode, 0, src0, dst);
        JmpSrc label(m_formatter.size());
        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, " MEM_o32r "", name, XMMRegName(dst), ADDR_o32r(label.offset()));
            else
                spew("%-11s" MEM_o32r ", %s", name, ADDR_o32r(label.offset()), XMMRegName(dst));
        } else {
            spew("%-11s" MEM_o32r ", %s, %s", name, ADDR_o32r(label.offset()), XMMRegName(src0), XMMRegName(dst));
        }
        return label;
    }
#endif

    void twoByteOpSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                       XMMRegisterID rm, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(dst), XMMRegName(rm));
            else
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(rm), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, (RegisterID)rm, dst);
            return;
        }

        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", name, XMMRegName(dst), XMMRegName(rm));
            else
                spew("%-11s%s, %s", name, XMMRegName(rm), XMMRegName(dst));
        } else {
            spew("%-11s%s, %s, %s", name, XMMRegName(rm), XMMRegName(src0), XMMRegName(dst));
        }
        m_formatter.twoByteOpVex(ty, opcode, (RegisterID)rm, src0, dst);
    }

    void twoByteOpImmSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                          uint32_t imm, XMMRegisterID rm, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s$0x%x, %s, %s", legacySSEOpName(name), imm, XMMRegName(rm), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, (RegisterID)rm, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        if (src0 == invalid_xmm)
            spew("%-11s$0x%x, %s, %s", name, imm, XMMRegName(rm), XMMRegName(dst));
        else
            spew("%-11s$0x%x, %s, %s, %s", name, imm, XMMRegName(rm), XMMRegName(src0), XMMRegName(dst));
        m_formatter.twoByteOpVex(ty, opcode, (RegisterID)rm, src0, dst);
        m_formatter.immediate8u(imm);
    }

    void twoByteOpSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                       int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            if (IsXMMReversedOperands(opcode)) {
                spew("%-11s%s, " MEM_ob, legacySSEOpName(name),
                     XMMRegName(dst), ADDR_ob(offset, base));
            } else {
                spew("%-11s" MEM_ob ", %s", legacySSEOpName(name),
                     ADDR_ob(offset, base), XMMRegName(dst));
            }
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, offset, base, dst);
            return;
        }

        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, " MEM_ob, name, XMMRegName(dst), ADDR_ob(offset, base));
            else
                spew("%-11s" MEM_ob ", %s", name, ADDR_ob(offset, base), XMMRegName(dst));
        } else {
            spew("%-11s" MEM_ob ", %s, %s", name,
                 ADDR_ob(offset, base), XMMRegName(src0), XMMRegName(dst));
        }
        m_formatter.twoByteOpVex(ty, opcode, offset, base, src0, dst);
    }

    void twoByteOpSimd_disp32(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                              int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, " MEM_o32b, legacySSEOpName(name), XMMRegName(dst), ADDR_o32b(offset, base));
            else
                spew("%-11s" MEM_o32b ", %s", legacySSEOpName(name), ADDR_o32b(offset, base), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp_disp32(opcode, offset, base, dst);
            return;
        }

        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, " MEM_o32b, name, XMMRegName(dst), ADDR_o32b(offset, base));
            else
                spew("%-11s" MEM_o32b ", %s", name, ADDR_o32b(offset, base), XMMRegName(dst));
        } else {
            spew("%-11s" MEM_o32b ", %s, %s", name,
                 ADDR_o32b(offset, base), XMMRegName(src0), XMMRegName(dst));
        }
        m_formatter.twoByteOpVex_disp32(ty, opcode, offset, base, src0, dst);
    }

    void twoByteOpImmSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                          uint32_t imm, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s$0x%x, " MEM_ob ", %s", legacySSEOpName(name), imm,
                 ADDR_ob(offset, base), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, offset, base, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, " MEM_ob ", %s, %s", name, imm, ADDR_ob(offset, base),
             XMMRegName(src0), XMMRegName(dst));
        m_formatter.twoByteOpVex(ty, opcode, offset, base, src0, dst);
        m_formatter.immediate8u(imm);
    }

    void twoByteOpSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                       int32_t offset, RegisterID base, RegisterID index, int scale,
                       XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            if (IsXMMReversedOperands(opcode)) {
                spew("%-11s%s, " MEM_obs, legacySSEOpName(name),
                     XMMRegName(dst), ADDR_obs(offset, base, index, scale));
            } else {
                spew("%-11s" MEM_obs ", %s", legacySSEOpName(name),
                     ADDR_obs(offset, base, index, scale), XMMRegName(dst));
            }
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, offset, base, index, scale, dst);
            return;
        }

        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode)) {
                spew("%-11s%s, " MEM_obs, name, XMMRegName(dst),
                     ADDR_obs(offset, base, index, scale));
            } else {
                spew("%-11s" MEM_obs ", %s", name, ADDR_obs(offset, base, index, scale),
                     XMMRegName(dst));
            }
        } else {
            spew("%-11s" MEM_obs ", %s, %s", name, ADDR_obs(offset, base, index, scale),
                 XMMRegName(src0), XMMRegName(dst));
        }
        m_formatter.twoByteOpVex(ty, opcode, offset, base, index, scale, src0, dst);
    }

    void twoByteOpSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                       const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %p", legacySSEOpName(name), XMMRegName(dst), address);
            else
                spew("%-11s%p, %s", legacySSEOpName(name), address, XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, address, dst);
            return;
        }

        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %p", name, XMMRegName(dst), address);
            else
                spew("%-11s%p, %s", name, address, XMMRegName(dst));
        } else {
            spew("%-11s%p, %s, %s", name, address, XMMRegName(src0), XMMRegName(dst));
        }
        m_formatter.twoByteOpVex(ty, opcode, address, src0, dst);
    }

    void twoByteOpImmSimd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                          uint32_t imm, const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s$0x%x, %p, %s", legacySSEOpName(name), imm, address, XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, address, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, %p, %s, %s", name, imm, address, XMMRegName(src0), XMMRegName(dst));
        m_formatter.twoByteOpVex(ty, opcode, address, src0, dst);
        m_formatter.immediate8u(imm);
    }

    void twoByteOpInt32Simd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                            RegisterID rm, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(dst), GPReg32Name(rm));
            else
                spew("%-11s%s, %s", legacySSEOpName(name), GPReg32Name(rm), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, rm, dst);
            return;
        }

        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", name, XMMRegName(dst), GPReg32Name(rm));
            else
                spew("%-11s%s, %s", name, GPReg32Name(rm), XMMRegName(dst));
        } else {
            spew("%-11s%s, %s, %s", name, GPReg32Name(rm), XMMRegName(src0), XMMRegName(dst));
        }
        m_formatter.twoByteOpVex(ty, opcode, rm, src0, dst);
    }

#ifdef JS_CODEGEN_X64
    void twoByteOpInt64Simd(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                            RegisterID rm, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(dst), GPRegName(rm));
            else
                spew("%-11s%s, %s", legacySSEOpName(name), GPRegName(rm), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp64(opcode, rm, dst);
            return;
        }

        if (src0 == invalid_xmm) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", name, XMMRegName(dst), GPRegName(rm));
            else
                spew("%-11s%s, %s", name, GPRegName(rm), XMMRegName(dst));
        } else {
            spew("%-11s%s, %s, %s", name, GPRegName(rm), XMMRegName(src0), XMMRegName(dst));
        }
        m_formatter.twoByteOpVex64(ty, opcode, rm, src0, dst);
    }
#endif

    void twoByteOpSimdInt32(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                            XMMRegisterID rm, RegisterID dst)
    {
        if (useLegacySSEEncodingForOtherOutput()) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", legacySSEOpName(name), GPReg32Name(dst), XMMRegName(rm));
            else if (opcode == OP2_MOVD_EdVd)
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName((XMMRegisterID)dst), GPReg32Name((RegisterID)rm));
            else
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(rm), GPReg32Name(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, (RegisterID)rm, dst);
            return;
        }

        if (IsXMMReversedOperands(opcode))
            spew("%-11s%s, %s", name, GPReg32Name(dst), XMMRegName(rm));
        else if (opcode == OP2_MOVD_EdVd)
            spew("%-11s%s, %s", name, XMMRegName((XMMRegisterID)dst), GPReg32Name((RegisterID)rm));
        else
            spew("%-11s%s, %s", name, XMMRegName(rm), GPReg32Name(dst));
        m_formatter.twoByteOpVex(ty, opcode, (RegisterID)rm, invalid_xmm, dst);
    }

    void twoByteOpImmSimdInt32(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                               uint32_t imm, XMMRegisterID rm, RegisterID dst)
    {
        if (useLegacySSEEncodingForOtherOutput()) {
            spew("%-11s$0x%x, %s, %s", legacySSEOpName(name), imm, XMMRegName(rm), GPReg32Name(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, (RegisterID)rm, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, %s, %s", name, imm, XMMRegName(rm), GPReg32Name(dst));
        m_formatter.twoByteOpVex(ty, opcode, (RegisterID)rm, invalid_xmm, dst);
        m_formatter.immediate8u(imm);
    }

#ifdef JS_CODEGEN_X64
    void twoByteOpSimdInt64(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                            XMMRegisterID rm, RegisterID dst)
    {
        if (useLegacySSEEncodingForOtherOutput()) {
            if (IsXMMReversedOperands(opcode))
                spew("%-11s%s, %s", legacySSEOpName(name), GPRegName(dst), XMMRegName(rm));
            else if (opcode == OP2_MOVD_EdVd)
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName((XMMRegisterID)dst), GPRegName((RegisterID)rm));
            else
                spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(rm), GPRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp64(opcode, (RegisterID)rm, dst);
            return;
        }

        if (IsXMMReversedOperands(opcode))
            spew("%-11s%s, %s", name, GPRegName(dst), XMMRegName(rm));
        else if (opcode == OP2_MOVD_EdVd)
            spew("%-11s%s, %s", name, XMMRegName((XMMRegisterID)dst), GPRegName((RegisterID)rm));
        else
            spew("%-11s%s, %s", name, XMMRegName(rm), GPRegName(dst));
        m_formatter.twoByteOpVex64(ty, opcode, (RegisterID)rm, invalid_xmm, (XMMRegisterID)dst);
    }
#endif

    void twoByteOpSimdFlags(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                            XMMRegisterID rm, XMMRegisterID reg)
    {
        if (useLegacySSEEncodingForOtherOutput()) {
            spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(rm), XMMRegName(reg));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, (RegisterID)rm, reg);
            return;
        }

        spew("%-11s%s, %s", name, XMMRegName(rm), XMMRegName(reg));
        m_formatter.twoByteOpVex(ty, opcode, (RegisterID)rm, invalid_xmm, (XMMRegisterID)reg);
    }

    void twoByteOpSimdFlags(const char* name, VexOperandType ty, TwoByteOpcodeID opcode,
                            int32_t offset, RegisterID base, XMMRegisterID reg)
    {
        if (useLegacySSEEncodingForOtherOutput()) {
            spew("%-11s" MEM_ob ", %s", legacySSEOpName(name),
                 ADDR_ob(offset, base), XMMRegName(reg));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.twoByteOp(opcode, offset, base, reg);
            return;
        }

        spew("%-11s" MEM_ob ", %s", name,
             ADDR_ob(offset, base), XMMRegName(reg));
        m_formatter.twoByteOpVex(ty, opcode, offset, base, invalid_xmm, (XMMRegisterID)reg);
    }

    void threeByteOpSimd(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                         ThreeByteEscape escape,
                         XMMRegisterID rm, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s%s, %s", legacySSEOpName(name), XMMRegName(rm), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, (RegisterID)rm, dst);
            return;
        }

        spew("%-11s%s, %s, %s", name, XMMRegName(rm), XMMRegName(src0), XMMRegName(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, (RegisterID)rm, src0, dst);
    }

    void threeByteOpImmSimd(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                            ThreeByteEscape escape,
                            uint32_t imm, XMMRegisterID rm, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s$0x%x, %s, %s", legacySSEOpName(name), imm, XMMRegName(rm), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, (RegisterID)rm, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, %s, %s, %s", name, imm, XMMRegName(rm), XMMRegName(src0), XMMRegName(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, (RegisterID)rm, src0, dst);
        m_formatter.immediate8u(imm);
    }

    void threeByteOpSimd(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                         ThreeByteEscape escape,
                         int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s" MEM_ob ", %s", legacySSEOpName(name),
                 ADDR_ob(offset, base), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, offset, base, dst);
            return;
        }

        spew("%-11s" MEM_ob ", %s, %s", name,
             ADDR_ob(offset, base), XMMRegName(src0), XMMRegName(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, offset, base, src0, dst);
    }

    void threeByteOpImmSimd(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                            ThreeByteEscape escape,
                            uint32_t imm, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s$0x%x, " MEM_ob ", %s", legacySSEOpName(name), imm,
                 ADDR_ob(offset, base), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, offset, base, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, " MEM_ob ", %s, %s", name, imm, ADDR_ob(offset, base),
             XMMRegName(src0), XMMRegName(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, offset, base, src0, dst);
        m_formatter.immediate8u(imm);
    }

    void threeByteOpSimd(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                         ThreeByteEscape escape,
                         const void* address, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s%p, %s", legacySSEOpName(name), address, XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, address, dst);
            return;
        }

        spew("%-11s%p, %s, %s", name, address, XMMRegName(src0), XMMRegName(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, address, src0, dst);
    }

    void threeByteOpImmInt32Simd(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                                 ThreeByteEscape escape, uint32_t imm,
                                 RegisterID src1, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s$0x%x, %s, %s", legacySSEOpName(name), imm, GPReg32Name(src1), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, src1, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, %s, %s, %s", name, imm, GPReg32Name(src1), XMMRegName(src0), XMMRegName(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, src1, src0, dst);
        m_formatter.immediate8u(imm);
    }

    void threeByteOpImmInt32Simd(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                                 ThreeByteEscape escape, uint32_t imm,
                                 int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src0, dst)) {
            spew("%-11s$0x%x, " MEM_ob ", %s", legacySSEOpName(name), imm, ADDR_ob(offset, base), XMMRegName(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, offset, base, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, " MEM_ob ", %s, %s", name, imm, ADDR_ob(offset, base), XMMRegName(src0), XMMRegName(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, offset, base, src0, dst);
        m_formatter.immediate8u(imm);
    }

    void threeByteOpImmSimdInt32(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                                 ThreeByteEscape escape, uint32_t imm,
                                 XMMRegisterID src, RegisterID dst)
    {
        if (useLegacySSEEncodingForOtherOutput()) {
            spew("%-11s$0x%x, %s, %s", legacySSEOpName(name), imm, XMMRegName(src), GPReg32Name(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, (RegisterID)src, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        if (opcode == OP3_PEXTRD_EdVdqIb)
            spew("%-11s$0x%x, %s, %s", name, imm, XMMRegName((XMMRegisterID)dst), GPReg32Name((RegisterID)src));
        else
            spew("%-11s$0x%x, %s, %s", name, imm, XMMRegName(src), GPReg32Name(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, (RegisterID)src, invalid_xmm, dst);
        m_formatter.immediate8u(imm);
    }

    void threeByteOpImmSimdInt32(const char* name, VexOperandType ty, ThreeByteOpcodeID opcode,
                                 ThreeByteEscape escape, uint32_t imm,
                                 int32_t offset, RegisterID base, RegisterID dst)
    {
        if (useLegacySSEEncodingForOtherOutput()) {
            spew("%-11s$0x%x, " MEM_ob ", %s", legacySSEOpName(name), imm, ADDR_ob(offset, base), GPReg32Name(dst));
            m_formatter.legacySSEPrefix(ty);
            m_formatter.threeByteOp(opcode, escape, offset, base, dst);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$0x%x, " MEM_ob ", %s", name, imm, ADDR_ob(offset, base), GPReg32Name(dst));
        m_formatter.threeByteOpVex(ty, opcode, escape, offset, base, invalid_xmm, dst);
        m_formatter.immediate8u(imm);
    }

    // Blendv is a three-byte op, but the VEX encoding has a different opcode
    // than the SSE encoding, so we handle it specially.
    void vblendvOpSimd(XMMRegisterID mask, XMMRegisterID rm, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncodingForVblendv(mask, src0, dst)) {
            spew("blendvps   %s, %s", XMMRegName(rm), XMMRegName(dst));
            // Even though a "ps" instruction, vblendv is encoded with the "pd" prefix.
            m_formatter.legacySSEPrefix(VEX_PD);
            m_formatter.threeByteOp(OP3_BLENDVPS_VdqWdq, ESCAPE_3A, (RegisterID)rm, dst);
            return;
        }

        spew("vblendvps  %s, %s, %s, %s",
             XMMRegName(mask), XMMRegName(rm), XMMRegName(src0), XMMRegName(dst));
        // Even though a "ps" instruction, vblendv is encoded with the "pd" prefix.
        m_formatter.vblendvOpVex(VEX_PD, OP3_VBLENDVPS_VdqWdq, ESCAPE_3A,
                                 mask, (RegisterID)rm, src0, dst);
    }

    void vblendvOpSimd(XMMRegisterID mask, int32_t offset, RegisterID base, XMMRegisterID src0, XMMRegisterID dst)
    {
        if (useLegacySSEEncodingForVblendv(mask, src0, dst)) {
            spew("blendvps   " MEM_ob ", %s", ADDR_ob(offset, base), XMMRegName(dst));
            // Even though a "ps" instruction, vblendv is encoded with the "pd" prefix.
            m_formatter.legacySSEPrefix(VEX_PD);
            m_formatter.threeByteOp(OP3_BLENDVPS_VdqWdq, ESCAPE_3A, offset, base, dst);
            return;
        }

        spew("vblendvps  %s, " MEM_ob ", %s, %s",
             XMMRegName(mask), ADDR_ob(offset, base), XMMRegName(src0), XMMRegName(dst));
        // Even though a "ps" instruction, vblendv is encoded with the "pd" prefix.
        m_formatter.vblendvOpVex(VEX_PD, OP3_VBLENDVPS_VdqWdq, ESCAPE_3A,
                                 mask, offset, base, src0, dst);
    }

    void shiftOpImmSimd(const char* name, TwoByteOpcodeID opcode, ShiftID shiftKind,
                        uint32_t imm, XMMRegisterID src, XMMRegisterID dst)
    {
        if (useLegacySSEEncoding(src, dst)) {
            spew("%-11s$%d, %s", legacySSEOpName(name), imm, XMMRegName(dst));
            m_formatter.legacySSEPrefix(VEX_PD);
            m_formatter.twoByteOp(opcode, (RegisterID)dst, (int)shiftKind);
            m_formatter.immediate8u(imm);
            return;
        }

        spew("%-11s$%d, %s, %s", name, imm, XMMRegName(src), XMMRegName(dst));
        m_formatter.twoByteOpVex(VEX_PD, opcode, (RegisterID)dst, src, (int)shiftKind);
        m_formatter.immediate8u(imm);
    }

    class X86InstructionFormatter {

    public:
        // Legacy prefix bytes:
        //
        // These are emmitted prior to the instruction.

        void prefix(OneByteOpcodeID pre)
        {
            m_buffer.putByte(pre);
        }

        void legacySSEPrefix(VexOperandType ty)
        {
            switch (ty) {
              case VEX_PS: break;
              case VEX_PD: prefix(PRE_SSE_66); break;
              case VEX_SS: prefix(PRE_SSE_F3); break;
              case VEX_SD: prefix(PRE_SSE_F2); break;
            }
        }

        // Word-sized operands / no operand instruction formatters.
        //
        // In addition to the opcode, the following operand permutations are supported:
        //   * None - instruction takes no operands.
        //   * One register - the low three bits of the RegisterID are added into the opcode.
        //   * Two registers - encode a register form ModRm (for all ModRm formats, the reg field is passed first, and a GroupOpcodeID may be passed in its place).
        //   * Three argument ModRM - a register, and a register and an offset describing a memory operand.
        //   * Five argument ModRM - a register, and a base register, an index, scale, and offset describing a memory operand.
        //
        // For 32-bit x86 targets, the address operand may also be provided as a
        // void*.  On 64-bit targets REX prefixes will be planted as necessary,
        // where high numbered registers are used.
        //
        // The twoByteOp methods plant two-byte Intel instructions sequences
        // (first opcode byte 0x0F).

        void oneByteOp(OneByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            m_buffer.putByteUnchecked(opcode);
        }

        void oneByteOp(OneByteOpcodeID opcode, RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(0, 0, reg);
            m_buffer.putByteUnchecked(opcode + (reg & 7));
        }

        void oneByteOp(OneByteOpcodeID opcode, RegisterID rm, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, reg);
        }

        void oneByteOp(OneByteOpcodeID opcode, int32_t offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, reg);
        }

        void oneByteOp_disp32(OneByteOpcodeID opcode, int32_t offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(offset, base, reg);
        }

        void oneByteOp(OneByteOpcodeID opcode, int32_t offset, RegisterID base, RegisterID index, int scale, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, index, scale, reg);
        }

        void oneByteOp_disp32(OneByteOpcodeID opcode, int32_t offset, RegisterID index, int scale, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, index, 0);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(offset, index, scale, reg);
        }

        void oneByteOp(OneByteOpcodeID opcode, const void* address, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, 0);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(address, reg);
        }

        void oneByteOp_disp32(OneByteOpcodeID opcode, const void* address, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, 0);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(address, reg);
        }
#ifdef JS_CODEGEN_X64
        void oneByteRipOp(OneByteOpcodeID opcode, int ripOffset, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, 0);
            m_buffer.putByteUnchecked(opcode);
            putModRm(ModRmMemoryNoDisp, noBase, reg);
            m_buffer.putIntUnchecked(ripOffset);
        }

        void oneByteRipOp64(OneByteOpcodeID opcode, int ripOffset, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, 0);
            m_buffer.putByteUnchecked(opcode);
            putModRm(ModRmMemoryNoDisp, noBase, reg);
            m_buffer.putIntUnchecked(ripOffset);
        }

        void twoByteRipOp(TwoByteOpcodeID opcode, int ripOffset, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, 0);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            putModRm(ModRmMemoryNoDisp, noBase, reg);
            m_buffer.putIntUnchecked(ripOffset);
        }

        void twoByteRipOpVex(VexOperandType ty, TwoByteOpcodeID opcode, int ripOffset,
                             XMMRegisterID src0, XMMRegisterID reg)
        {
            int r = (reg >> 3), x = 0, b = 0;
            int m = 1; // 0x0F
            int w = 0, v = src0, l = 0;
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            putModRm(ModRmMemoryNoDisp, noBase, reg);
            m_buffer.putIntUnchecked(ripOffset);
        }
#endif

        void twoByteOp(TwoByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
        }

        void twoByteOp(TwoByteOpcodeID opcode, RegisterID rm, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, reg);
        }

        void twoByteOpVex(VexOperandType ty, TwoByteOpcodeID opcode,
                          RegisterID rm, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = (rm >> 3);
            int m = 1; // 0x0F
            int w = 0, v = src0, l = 0;
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            registerModRM(rm, reg);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int32_t offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, reg);
        }

        void twoByteOpVex(VexOperandType ty, TwoByteOpcodeID opcode,
                          int32_t offset, RegisterID base, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = (base >> 3);
            int m = 1; // 0x0F
            int w = 0, v = src0, l = 0;
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            memoryModRM(offset, base, reg);
        }

        void twoByteOp_disp32(TwoByteOpcodeID opcode, int32_t offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(offset, base, reg);
        }

        void twoByteOpVex_disp32(VexOperandType ty, TwoByteOpcodeID opcode,
                                 int32_t offset, RegisterID base, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = (base >> 3);
            int m = 1; // 0x0F
            int w = 0, v = src0, l = 0;
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            memoryModRM_disp32(offset, base, reg);
        }

        void twoByteOp(TwoByteOpcodeID opcode, int32_t offset, RegisterID base, RegisterID index, int scale, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, index, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, index, scale, reg);
        }

        void twoByteOpVex(VexOperandType ty, TwoByteOpcodeID opcode,
                          int32_t offset, RegisterID base, RegisterID index, int scale,
                          XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = (index >> 3), b = (base >> 3);
            int m = 1; // 0x0F
            int w = 0, v = src0, l = 0;
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            memoryModRM(offset, base, index, scale, reg);
        }

        void twoByteOp(TwoByteOpcodeID opcode, const void* address, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, 0);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(address, reg);
        }

        void twoByteOpVex(VexOperandType ty, TwoByteOpcodeID opcode,
                          const void* address, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = 0;
            int m = 1; // 0x0F
            int w = 0, v = src0, l = 0;
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            memoryModRM(address, reg);
        }

        void threeByteOp(ThreeByteOpcodeID opcode, ThreeByteEscape escape, RegisterID rm, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(escape);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, reg);
        }

        void threeByteOpVex(VexOperandType ty, ThreeByteOpcodeID opcode, ThreeByteEscape escape,
                            RegisterID rm, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = (rm >> 3);
            int m = 0, w = 0, v = src0, l = 0;
            switch (escape) {
              case ESCAPE_38: m = 2; break;
              case ESCAPE_3A: m = 3; break;
              default: MOZ_CRASH("unexpected escape");
            }
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            registerModRM(rm, reg);
        }

        void threeByteOp(ThreeByteOpcodeID opcode, ThreeByteEscape escape, int32_t offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(escape);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, reg);
        }

        void threeByteOpVex(VexOperandType ty, ThreeByteOpcodeID opcode, ThreeByteEscape escape,
                            int32_t offset, RegisterID base, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = (base >> 3);
            int m = 0, w = 0, v = src0, l = 0;
            switch (escape) {
              case ESCAPE_38: m = 2; break;
              case ESCAPE_3A: m = 3; break;
              default: MOZ_CRASH("unexpected escape");
            }
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            memoryModRM(offset, base, reg);
        }

        void threeByteOp(ThreeByteOpcodeID opcode, ThreeByteEscape escape, const void* address, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIfNeeded(reg, 0, 0);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(escape);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(address, reg);
        }

        void threeByteOpVex(VexOperandType ty, ThreeByteOpcodeID opcode, ThreeByteEscape escape,
                            const void* address, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = 0;
            int m = 0, w = 0, v = src0, l = 0;
            switch (escape) {
              case ESCAPE_38: m = 2; break;
              case ESCAPE_3A: m = 3; break;
              default: MOZ_CRASH("unexpected escape");
            }
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            memoryModRM(address, reg);
        }

        void vblendvOpVex(VexOperandType ty, ThreeByteOpcodeID opcode, ThreeByteEscape escape,
                          XMMRegisterID mask, RegisterID rm, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = (rm >> 3);
            int m = 0, w = 0, v = src0, l = 0;
            switch (escape) {
              case ESCAPE_38: m = 2; break;
              case ESCAPE_3A: m = 3; break;
              default: MOZ_CRASH("unexpected escape");
            }
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            registerModRM(rm, reg);
            immediate8u(mask << 4);
        }

        void vblendvOpVex(VexOperandType ty, ThreeByteOpcodeID opcode, ThreeByteEscape escape,
                          XMMRegisterID mask, int32_t offset, RegisterID base, XMMRegisterID src0, int reg)
        {
            int r = (reg >> 3), x = 0, b = (base >> 3);
            int m = 0, w = 0, v = src0, l = 0;
            switch (escape) {
              case ESCAPE_38: m = 2; break;
              case ESCAPE_3A: m = 3; break;
              default: MOZ_CRASH("unexpected escape");
            }
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            memoryModRM(offset, base, reg);
            immediate8u(mask << 4);
        }

#ifdef JS_CODEGEN_X64
        // Quad-word-sized operands:
        //
        // Used to format 64-bit operantions, planting a REX.w prefix.  When
        // planting d64 or f64 instructions, not requiring a REX.w prefix, the
        // normal (non-'64'-postfixed) formatters should be used.

        void oneByteOp64(OneByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(0, 0, 0);
            m_buffer.putByteUnchecked(opcode);
        }

        void oneByteOp64(OneByteOpcodeID opcode, RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(0, 0, reg);
            m_buffer.putByteUnchecked(opcode + (reg & 7));
        }

        void oneByteOp64(OneByteOpcodeID opcode, RegisterID rm, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, reg);
        }

        void oneByteOp64(OneByteOpcodeID opcode, int32_t offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, reg);
        }

        void oneByteOp64_disp32(OneByteOpcodeID opcode, int32_t offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(offset, base, reg);
        }

        void oneByteOp64(OneByteOpcodeID opcode, int32_t offset, RegisterID base, RegisterID index, int scale, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, index, scale, reg);
        }

        void oneByteOp64(OneByteOpcodeID opcode, const void* address, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, 0);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(address, reg);
        }

        void twoByteOp64(TwoByteOpcodeID opcode, RegisterID rm, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, reg);
        }

        void twoByteOp64(TwoByteOpcodeID opcode, int offset, RegisterID base, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, reg);
        }

        void twoByteOp64(TwoByteOpcodeID opcode, int offset, RegisterID base, RegisterID index, int scale, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, index, base);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, index, scale, reg);
        }

        void twoByteOp64(TwoByteOpcodeID opcode, const void* address, int reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexW(reg, 0, 0);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(address, reg);
        }

        void twoByteOpVex64(VexOperandType ty, TwoByteOpcodeID opcode,
                            RegisterID rm, XMMRegisterID src0, XMMRegisterID reg)
        {
            int r = (reg >> 3), x = 0, b = (rm >> 3);
            int m = 1; // 0x0F
            int w = 1, v = src0, l = 0;
            threeOpVex(ty, r, x, b, m, w, v, l, opcode);
            registerModRM(rm, reg);
        }
#endif

        // Byte-operands:
        //
        // These methods format byte operations.  Byte operations differ from
        // the normal formatters in the circumstances under which they will
        // decide to emit REX prefixes.  These should be used where any register
        // operand signifies a byte register.
        //
        // The disctinction is due to the handling of register numbers in the
        // range 4..7 on x86-64.  These register numbers may either represent
        // the second byte of the first four registers (ah..bh) or the first
        // byte of the second four registers (spl..dil).
        //
        // Address operands should still be checked using regRequiresRex(),
        // while byteRegRequiresRex() is provided to check byte register
        // operands.

        void oneByteOp8(OneByteOpcodeID opcode)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            m_buffer.putByteUnchecked(opcode);
        }

        void oneByteOp8(OneByteOpcodeID opcode, RegisterID rm, GroupOpcodeID groupOp)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(byteRegRequiresRex(rm), 0, 0, rm);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, groupOp);
        }

        // Like oneByteOp8, but never emits a REX prefix.
        void oneByteOp8_norex(OneByteOpcodeID opcode, HRegisterID rm, GroupOpcodeID groupOp)
        {
            MOZ_ASSERT(!regRequiresRex(RegisterID(rm)));
            m_buffer.ensureSpace(MaxInstructionSize);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(RegisterID(rm), groupOp);
        }

        void oneByteOp8(OneByteOpcodeID opcode, int32_t offset, RegisterID base, RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg), reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, reg);
        }

        void oneByteOp8_disp32(OneByteOpcodeID opcode, int32_t offset, RegisterID base,
                               RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg), reg, 0, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(offset, base, reg);
        }

        void oneByteOp8(OneByteOpcodeID opcode, int32_t offset, RegisterID base,
                        RegisterID index, int scale, RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg), reg, index, base);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM(offset, base, index, scale, reg);
        }

        void oneByteOp8(OneByteOpcodeID opcode, const void* address, RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg), reg, 0, 0);
            m_buffer.putByteUnchecked(opcode);
            memoryModRM_disp32(address, reg);
        }

        void twoByteOp8(TwoByteOpcodeID opcode, RegisterID rm, RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(byteRegRequiresRex(reg)|byteRegRequiresRex(rm), reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, reg);
        }

        // Like twoByteOp8 but doesn't add a REX prefix if the destination reg
        // is in esp..edi. This may be used when the destination is not an 8-bit
        // register (as in a movzbl instruction), so it doesn't need a REX
        // prefix to disambiguate it from ah..bh.
        void twoByteOp8_movx(TwoByteOpcodeID opcode, RegisterID rm, RegisterID reg)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(regRequiresRex(reg)|byteRegRequiresRex(rm), reg, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, reg);
        }

        void twoByteOp8(TwoByteOpcodeID opcode, RegisterID rm, GroupOpcodeID groupOp)
        {
            m_buffer.ensureSpace(MaxInstructionSize);
            emitRexIf(byteRegRequiresRex(rm), 0, 0, rm);
            m_buffer.putByteUnchecked(OP_2BYTE_ESCAPE);
            m_buffer.putByteUnchecked(opcode);
            registerModRM(rm, groupOp);
        }

        // Immediates:
        //
        // An immedaite should be appended where appropriate after an op has
        // been emitted.  The writes are unchecked since the opcode formatters
        // above will have ensured space.

        // A signed 8-bit immediate.
        void immediate8s(int32_t imm)
        {
            MOZ_ASSERT(CAN_SIGN_EXTEND_8_32(imm));
            m_buffer.putByteUnchecked(imm);
        }

        // An unsigned 8-bit immediate.
        void immediate8u(uint32_t imm)
        {
            MOZ_ASSERT(CAN_ZERO_EXTEND_8_32(imm));
            m_buffer.putByteUnchecked(int32_t(imm));
        }

        // An 8-bit immediate with is either signed or unsigned, for use in
        // instructions which actually only operate on 8 bits.
        void immediate8(int32_t imm)
        {
            m_buffer.putByteUnchecked(imm);
        }

        // A signed 16-bit immediate.
        void immediate16s(int32_t imm)
        {
            MOZ_ASSERT(CAN_SIGN_EXTEND_16_32(imm));
            m_buffer.putShortUnchecked(imm);
        }

        // An unsigned 16-bit immediate.
        void immediate16u(int32_t imm)
        {
            MOZ_ASSERT(CAN_ZERO_EXTEND_16_32(imm));
            m_buffer.putShortUnchecked(imm);
        }

        // A 16-bit immediate with is either signed or unsigned, for use in
        // instructions which actually only operate on 16 bits.
        void immediate16(int32_t imm)
        {
            m_buffer.putShortUnchecked(imm);
        }

        void immediate32(int32_t imm)
        {
            m_buffer.putIntUnchecked(imm);
        }

        void immediate64(int64_t imm)
        {
            m_buffer.putInt64Unchecked(imm);
        }

        MOZ_WARN_UNUSED_RESULT JmpSrc
        immediateRel32()
        {
            m_buffer.putIntUnchecked(0);
            return JmpSrc(m_buffer.size());
        }

        // Data:

        void jumpTablePointer(uintptr_t ptr)
        {
            m_buffer.ensureSpace(sizeof(uintptr_t));
#ifdef JS_CODEGEN_X64
            m_buffer.putInt64Unchecked(ptr);
#else
            m_buffer.putIntUnchecked(ptr);
#endif
        }

        void doubleConstant(double d)
        {
            m_buffer.ensureSpace(sizeof(double));
            m_buffer.putInt64Unchecked(mozilla::BitwiseCast<uint64_t>(d));
        }

        void floatConstant(float f)
        {
            m_buffer.ensureSpace(sizeof(float));
            m_buffer.putIntUnchecked(mozilla::BitwiseCast<uint32_t>(f));
        }

        void int32x4Constant(const int32_t s[4])
        {
            for (size_t i = 0; i < 4; ++i)
                int32Constant(s[i]);
        }

        void float32x4Constant(const float s[4])
        {
            for (size_t i = 0; i < 4; ++i)
                floatConstant(s[i]);
        }

        void int64Constant(int64_t i)
        {
            m_buffer.ensureSpace(sizeof(int64_t));
            m_buffer.putInt64Unchecked(i);
        }

        void int32Constant(int32_t i)
        {
            m_buffer.ensureSpace(sizeof(int32_t));
            m_buffer.putIntUnchecked(i);
        }

        // Administrative methods:

        size_t size() const { return m_buffer.size(); }
        const unsigned char* buffer() const { return m_buffer.buffer(); }
        bool oom() const { return m_buffer.oom(); }
        bool isAligned(int alignment) const { return m_buffer.isAligned(alignment); }
        unsigned char* data() { return m_buffer.data(); }

    private:

        // Internals; ModRm and REX formatters.

        // Byte operand register spl & above requir a REX prefix, which precludes
        // use of the h registers in the same instruction.
        static bool byteRegRequiresRex(RegisterID reg)
        {
#ifdef JS_CODEGEN_X64
            return reg >= rsp;
#else
            return false;
#endif
        }

        // For non-byte sizes, registers r8 & above always require a REX prefix.
        static bool regRequiresRex(RegisterID reg)
        {
#ifdef JS_CODEGEN_X64
            return reg >= r8;
#else
            return false;
#endif
        }

#ifdef JS_CODEGEN_X64
        // Format a REX prefix byte.
        void emitRex(bool w, int r, int x, int b)
        {
            m_buffer.putByteUnchecked(PRE_REX | ((int)w << 3) | ((r>>3)<<2) | ((x>>3)<<1) | (b>>3));
        }

        // Used to plant a REX byte with REX.w set (for 64-bit operations).
        void emitRexW(int r, int x, int b)
        {
            emitRex(true, r, x, b);
        }

        // Used for operations with byte operands - use byteRegRequiresRex() to
        // check register operands, regRequiresRex() to check other registers
        // (i.e. address base & index).
        //
        // NB: WebKit's use of emitRexIf() is limited such that the
        // reqRequiresRex() checks are not needed. SpiderMonkey extends
        // oneByteOp8 functionality such that r, x, and b can all be used.
        void emitRexIf(bool condition, int r, int x, int b)
        {
            if (condition ||
                regRequiresRex(RegisterID(r)) ||
                regRequiresRex(RegisterID(x)) ||
                regRequiresRex(RegisterID(b)))
            {
                emitRex(false, r, x, b);
            }
        }

        // Used for word sized operations, will plant a REX prefix if necessary
        // (if any register is r8 or above).
        void emitRexIfNeeded(int r, int x, int b)
        {
            emitRexIf(false, r, x, b);
        }
#else
        // No REX prefix bytes on 32-bit x86.
        void emitRexIf(bool condition, int, int, int)
        {
            MOZ_ASSERT(!condition, "32-bit x86 should never use a REX prefix");
        }
        void emitRexIfNeeded(int, int, int) {}
#endif

        void putModRm(ModRmMode mode, RegisterID rm, int reg)
        {
            m_buffer.putByteUnchecked((mode << 6) | ((reg & 7) << 3) | (rm & 7));
        }

        void putModRmSib(ModRmMode mode, RegisterID base, RegisterID index, int scale, int reg)
        {
            MOZ_ASSERT(mode != ModRmRegister);

            putModRm(mode, hasSib, reg);
            m_buffer.putByteUnchecked((scale << 6) | ((index & 7) << 3) | (base & 7));
        }

        void registerModRM(RegisterID rm, int reg)
        {
            putModRm(ModRmRegister, rm, reg);
        }

        void memoryModRM(int32_t offset, RegisterID base, int reg)
        {
            // A base of esp or r12 would be interpreted as a sib, so force a
            // sib with no index & put the base in there.
#ifdef JS_CODEGEN_X64
            if ((base == hasSib) || (base == hasSib2))
#else
            if (base == hasSib)
#endif
            {
                if (!offset) // No need to check if the base is noBase, since we know it is hasSib!
                    putModRmSib(ModRmMemoryNoDisp, base, noIndex, 0, reg);
                else if (CAN_SIGN_EXTEND_8_32(offset)) {
                    putModRmSib(ModRmMemoryDisp8, base, noIndex, 0, reg);
                    m_buffer.putByteUnchecked(offset);
                } else {
                    putModRmSib(ModRmMemoryDisp32, base, noIndex, 0, reg);
                    m_buffer.putIntUnchecked(offset);
                }
            } else {
#ifdef JS_CODEGEN_X64
                if (!offset && (base != noBase) && (base != noBase2))
#else
                if (!offset && (base != noBase))
#endif
                    putModRm(ModRmMemoryNoDisp, base, reg);
                else if (CAN_SIGN_EXTEND_8_32(offset)) {
                    putModRm(ModRmMemoryDisp8, base, reg);
                    m_buffer.putByteUnchecked(offset);
                } else {
                    putModRm(ModRmMemoryDisp32, base, reg);
                    m_buffer.putIntUnchecked(offset);
                }
            }
        }

        void memoryModRM_disp32(int32_t offset, RegisterID base, int reg)
        {
            // A base of esp or r12 would be interpreted as a sib, so force a
            // sib with no index & put the base in there.
#ifdef JS_CODEGEN_X64
            if ((base == hasSib) || (base == hasSib2))
#else
            if (base == hasSib)
#endif
            {
                putModRmSib(ModRmMemoryDisp32, base, noIndex, 0, reg);
                m_buffer.putIntUnchecked(offset);
            } else {
                putModRm(ModRmMemoryDisp32, base, reg);
                m_buffer.putIntUnchecked(offset);
            }
        }

        void memoryModRM(int32_t offset, RegisterID base, RegisterID index, int scale, int reg)
        {
            MOZ_ASSERT(index != noIndex);

#ifdef JS_CODEGEN_X64
            if (!offset && (base != noBase) && (base != noBase2))
#else
            if (!offset && (base != noBase))
#endif
                putModRmSib(ModRmMemoryNoDisp, base, index, scale, reg);
            else if (CAN_SIGN_EXTEND_8_32(offset)) {
                putModRmSib(ModRmMemoryDisp8, base, index, scale, reg);
                m_buffer.putByteUnchecked(offset);
            } else {
                putModRmSib(ModRmMemoryDisp32, base, index, scale, reg);
                m_buffer.putIntUnchecked(offset);
            }
        }

        void memoryModRM_disp32(int32_t offset, RegisterID index, int scale, int reg)
        {
            MOZ_ASSERT(index != noIndex);

            // NB: the base-less memoryModRM overloads generate different code
            // then the base-full memoryModRM overloads in the base == noBase
            // case. The base-less overloads assume that the desired effective
            // address is:
            //
            //   reg := [scaled index] + disp32
            //
            // which means the mod needs to be ModRmMemoryNoDisp. The base-full
            // overloads pass ModRmMemoryDisp32 in all cases and thus, when
            // base == noBase (== ebp), the effective address is:
            //
            //   reg := [scaled index] + disp32 + [ebp]
            //
            // See Intel developer manual, Vol 2, 2.1.5, Table 2-3.
            putModRmSib(ModRmMemoryNoDisp, noBase, index, scale, reg);
            m_buffer.putIntUnchecked(offset);
        }

        void memoryModRM_disp32(const void* address, int reg)
        {
            int32_t disp = AddressImmediate(address);

#ifdef JS_CODEGEN_X64
            // On x64-64, non-RIP-relative absolute mode requires a SIB.
            putModRmSib(ModRmMemoryNoDisp, noBase, noIndex, 0, reg);
#else
            // noBase + ModRmMemoryNoDisp means noBase + ModRmMemoryDisp32!
            putModRm(ModRmMemoryNoDisp, noBase, reg);
#endif
            m_buffer.putIntUnchecked(disp);
        }

        void memoryModRM(const void* address, int reg)
        {
            memoryModRM_disp32(address, reg);
        }

        void threeOpVex(VexOperandType p, int r, int x, int b, int m, int w, int v, int l,
                        int opcode)
        {
            m_buffer.ensureSpace(MaxInstructionSize);

            if (v == invalid_xmm)
                v = XMMRegisterID(0);

            if (x == 0 && b == 0 && m == 1 && w == 0) {
                // Two byte VEX.
                m_buffer.putByteUnchecked(PRE_VEX_C5);
                m_buffer.putByteUnchecked(((r << 7) | (v << 3) | (l << 2) | p) ^ 0xf8);
            } else {
                // Three byte VEX.
                m_buffer.putByteUnchecked(PRE_VEX_C4);
                m_buffer.putByteUnchecked(((r << 7) | (x << 6) | (b << 5) | m) ^ 0xe0);
                m_buffer.putByteUnchecked(((w << 7) | (v << 3) | (l << 2) | p) ^ 0x78);
            }

            m_buffer.putByteUnchecked(opcode);
        }

        AssemblerBuffer m_buffer;
    } m_formatter;

    bool useVEX_;
};

} // namespace X86Encoding

} // namespace jit
} // namespace js

#endif /* jit_shared_BaseAssembler_x86_shared_h */
