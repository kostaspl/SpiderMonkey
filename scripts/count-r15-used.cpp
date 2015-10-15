#include <stdio.h>
#include <iostream>
#include "pin.H"

int r15_appearances = 0;

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    uint32_t maxRRegs = INS_MaxNumRRegs(ins);
    uint32_t maxWRegs = INS_MaxNumWRegs(ins);
    uint32_t i = 0;

    for (i = 0; i < maxRRegs; i++){
        if (REG_StringShort(REG_FullRegName(INS_RegR(ins, i))) == "r15") r15_appearances++;
    }

    for (i = 0; i < maxWRegs; i++){
        if (REG_StringShort(REG_FullRegName(INS_RegW(ins, i))) == "r15") r15_appearances++;
    }
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    cout << "r15 was used " << r15_appearances << " times." << endl;
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool prints how many times register r15 has been"
		" used during execution.\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
