#include <cstdint>
#include <iostream>
#include <jni.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include "NRSSL.h"

using namespace llvm;

namespace {

cl::opt<NRSSL::Type> selected_nrs(
    "float_type", cl::desc("Choose the floating point representation"),
    cl::init(NRSSL::Type::POSIT1),
    cl::values(clEnumValN(NRSSL::Type::POSIT1, "posit1", "Posit1 representation"),
               clEnumValN(NRSSL::Type::POSIT2, "posit2", "Posit2 representation"),
               clEnumValN(NRSSL::Type::MORRIS, "morris", "Morris representation"),
               clEnumValN(NRSSL::Type::MORRIS_HEB, "morrisHeb", "Morris HEB representation"),
               clEnumValN(NRSSL::Type::MORRIS_UNARY_HEB, "morrisUnaryHeb",
                          "Morris unary HEB representation"),
               clEnumValN(NRSSL::Type::MORRIS_BIAS_HEB, "morrisBiasHeb",
                          "Morris bias HEB representation")));

class NRSIntrinsicSelector {
  public:
    explicit NRSIntrinsicSelector(NRSSL::Type nrs, Module &M) {

        static const std::unordered_map<NRSSL::Type, std::string> nrs_names = {
            {NRSSL::Type::POSIT1, "posit1"},
            {NRSSL::Type::POSIT2, "posit2"},
            {NRSSL::Type::MORRIS, "morris"},
            {NRSSL::Type::MORRIS_HEB, "morrisHeb"},
            {NRSSL::Type::MORRIS_BIAS_HEB, "morrisBiasHeb"},
            {NRSSL::Type::MORRIS_UNARY_HEB, "morrisUnaryHeb"},
        };

        const auto intrinsic_prefix = "llvm.riscv.nrssl.f32." + nrs_names.at(nrs) + ".";

        // Map binary/unary ops to their corresponding intrinsics
        {
            static const std::array<std::pair<const char *, std::vector<unsigned>>, 6> binop_names{
                std::make_pair("add", std::vector<unsigned>{Instruction::FAdd}),
                std::make_pair("sub", std::vector<unsigned>{Instruction::FSub}),
                std::make_pair("mul", std::vector<unsigned>{Instruction::FMul}),
                std::make_pair("div", std::vector<unsigned>{Instruction::FDiv}),
                std::make_pair("cvtToInt",
                               std::vector<unsigned>{Instruction::FPToSI, Instruction::FPToUI}),
                std::make_pair("cvtFromInt",
                               std::vector<unsigned>{Instruction::SIToFP, Instruction::UIToFP})};

            for (const auto &[op_name, op_opcodes] : binop_names) {
                const auto intrinsic_name = intrinsic_prefix + op_name;
                const auto intrinsic_id = Function::lookupIntrinsicID(intrinsic_name);
                std::cout << "Looking up intrinsic: " << intrinsic_name << std::endl;
                std::cout << "Intrinsic id: " << intrinsic_id << " for " << intrinsic_name
                          << std::endl;
                const auto intrinsic_func = Intrinsic::getDeclaration(&M, intrinsic_id);
                if (intrinsic_func) {
                    for (const auto op_opcode : op_opcodes) {
                        op_to_intrinsic_[op_opcode] = intrinsic_func;
                    }
                    std::cout << "Mapped binary operator " << op_name << " to intrinsic function "
                              << intrinsic_func->getName().str() << "\n";
                } else {
                    std::cerr << "Warning: Intrinsic " << intrinsic_name
                              << " not found in LLVM. This operation will not be converted.\n";
                }
            }
        }

        // Map fcmp ops to their corresponding intrinsics
        {
            static constexpr std::array fcmp_names{std::make_pair("eq", FCmpOpcode::FCMP_EQ),
                                                   std::make_pair("lt", FCmpOpcode::FCMP_LT),
                                                   std::make_pair("le", FCmpOpcode::FCMP_LE)};

            for (const auto &[fcmp_name, fcmp_opcode] : fcmp_names) {
                const auto intrinsic_name = intrinsic_prefix + fcmp_name;
                const auto intrinsic_id = Function::lookupIntrinsicID(intrinsic_name);
                const auto intrinsic_func = Intrinsic::getDeclaration(&M, intrinsic_id);
                if (intrinsic_func) {
                    fcmp_to_intrinsic_[fcmp_opcode] = intrinsic_func;
                    std::cout << "Mapped fcmp predicate " << fcmp_name << " to intrinsic function "
                              << intrinsic_func->getName().str() << "\n";
                } else {
                    std::cerr << "Warning: Intrinsic " << intrinsic_name
                              << " not found in LLVM. This operation will not be converted.\n";
                }
            }
        }

        // Map intrinsics (special ops) to their corresponding intrinsics
        {
            static const std::array intrinsic_names{
                std::make_pair("min", Intrinsic::minnum),
                std::make_pair("max", Intrinsic::maxnum),
            };

            for (const auto &[intrinsic_name, src_intrinsic_id] : intrinsic_names) {
                const auto full_intrinsic_name = intrinsic_prefix + intrinsic_name;
                const auto target_intrinsic_id = Function::lookupIntrinsicID(full_intrinsic_name);
                const auto target_intrinsic_func =
                    Intrinsic::getDeclaration(&M, target_intrinsic_id);
                if (target_intrinsic_func) {
                    intrinsic_to_intrinsic_[src_intrinsic_id] = target_intrinsic_func;
                    std::cout << "Mapped intrinsic " << full_intrinsic_name
                              << " to intrinsic function " << target_intrinsic_func->getName().str()
                              << "\n";
                } else {
                    std::cerr << "Warning: Intrinsic " << full_intrinsic_name
                              << " not found in LLVM. This operation will not be converted.\n";
                }
            }
        }
    }

    Function *get_intrinsic_replacement(const Instruction &I) const {
        if (I.isBinaryOp() || isa<CastInst>(I)) {
            auto it = op_to_intrinsic_.find(I.getOpcode());
            if (it != op_to_intrinsic_.end()) {
                return it->second;
            }
        } else if (const auto *FCmp = dyn_cast<FCmpInst>(&I)) {
            FCmpOpcode fcmp_opcode;
            switch (FCmp->getPredicate()) {
            case FCmpInst::FCMP_OEQ:
            case FCmpInst::FCMP_UEQ:
                fcmp_opcode = FCmpOpcode::FCMP_EQ;
                break;
            case FCmpInst::FCMP_OLT:
            case FCmpInst::FCMP_ULT:
                fcmp_opcode = FCmpOpcode::FCMP_LT;
                break;
            case FCmpInst::FCMP_OLE:
            case FCmpInst::FCMP_ULE:
                fcmp_opcode = FCmpOpcode::FCMP_LE;
                break;
            default:
                fcmp_opcode = FCmpOpcode::FCMP_INVALID;
                break;
            }
            auto it = fcmp_to_intrinsic_.find(fcmp_opcode);
            if (it != fcmp_to_intrinsic_.end()) {
                return it->second;
            }
        } else if (const auto *Call = dyn_cast<CallBase>(&I)) {
            Function *called_fun = Call->getCalledFunction();
            if (called_fun && called_fun->isIntrinsic()) {
                auto it = intrinsic_to_intrinsic_.find(called_fun->getIntrinsicID());
                if (it != intrinsic_to_intrinsic_.end()) {
                    return it->second;
                }
            }
        }
        return nullptr; // No replacement found
    }

  private:
    NRSIntrinsicSelector(const NRSIntrinsicSelector &) = delete;
    NRSIntrinsicSelector &operator=(const NRSIntrinsicSelector &) = delete;

    std::unordered_map<unsigned, Function *> op_to_intrinsic_{};
    enum class FCmpOpcode { FCMP_EQ, FCMP_LT, FCMP_LE, FCMP_INVALID };
    std::unordered_map<FCmpOpcode, Function *> fcmp_to_intrinsic_{};
    std::unordered_map<Intrinsic::ID, Function *> intrinsic_to_intrinsic_{};
};

} // namespace

class NRSConversionPass : public PassInfoMixin<NRSConversionPass> {

    NRSSL nrssl;

  public:
    NRSConversionPass() { std::cout << "Creating NRSConversionPass\n"; }

    ~NRSConversionPass() { std::cout << "Destroying NRSConversionPass\n"; }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
        std::cout << "Running NRSConversionPass on " << M.getName().str() << "\n";

        bool Modified = false;

        for (auto &Global : M.globals()) {
            if (Global.hasInitializer()) {
                Constant *Initializer = Global.getInitializer();
                std::cout << "Processing global variable: " << Global.getName().str() << "\n";

                Constant *NewInitializer = processConstant(Initializer, M.getContext());
                if (NewInitializer != Initializer) {
                    Global.setInitializer(NewInitializer);
                    Modified = true;
                }
            }
        }

        NRSIntrinsicSelector intrinsic_selector(selected_nrs, M);

        for (auto &F : M) {
            for (auto &BB : F) {
                for (auto it = BB.begin(); it != BB.end();) {
                    Instruction &I = *it;
                    // Advance the iterator before potential erases
                    ++it;

                    std::cout << "Processing instruction: " << I.getOpcodeName() << "\n";
                    if (auto *Store = dyn_cast<StoreInst>(&I)) {
                        Value *Value = Store->getValueOperand();
                        if (auto *Const = dyn_cast<Constant>(Value)) {
                            Constant *NewConst = processConstant(Const, M.getContext());
                            if (NewConst != Const) {
                                Store->setOperand(0, NewConst);
                                Modified = true;
                            }
                        }
                    } else if (auto *intrinsic_replacement =
                                   intrinsic_selector.get_intrinsic_replacement(I)) {
                        IRBuilder builder(&I);
                        std::vector<Value *> operands = [&I] {
                            const auto operands = I.operands();
                            std::vector<Value *> operand_vec(operands.begin(),
                                                             std::next(operands.begin(), 2));
                            return operand_vec;
                        }();
                        Value *new_call = builder.CreateCall(intrinsic_replacement, operands);
                        I.replaceAllUsesWith(new_call);
                        I.eraseFromParent();
                        Modified = true;
                    }
                }
            }
        }

        return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }

    static void nrsslShutdown() { NRSSL::shutdown(); }

  private:
    Constant *convertConstantFP(ConstantFP *FpConst, LLVMContext &Context) {
        APFloat OldAPF = FpConst->getValueAPF();
        APInt OldBits = OldAPF.bitcastToAPInt();

        if (FpConst->getType()->isFloatTy()) {
            float IeeeValue;
            memcpy(&IeeeValue, OldBits.getRawData(), sizeof(float));

            uint32_t new_value = nrssl.convertDoubleToUint<uint32_t>(IeeeValue, selected_nrs);
            APInt NewBits(32, new_value);

            APFloat NewAPF(APFloat::IEEEsingle(), NewBits);
            return ConstantFP::get(Context, NewAPF);
        }

        if (FpConst->getType()->isDoubleTy()) {
            double IeeeValue;
            memcpy(&IeeeValue, OldBits.getRawData(), sizeof(double));

            uint64_t new_value = nrssl.convertDoubleToUint<uint64_t>(IeeeValue, selected_nrs);
            APInt NewBits(64, new_value);

            APFloat NewAPF(APFloat::IEEEdouble(), NewBits);
            return ConstantFP::get(Context, NewAPF);
        }

        return nullptr;
    }

    Constant *processConstant(Constant *C, LLVMContext &Context) {

        std::cout << "Processing constant" << "\n";

        if (ConstantFP *FpConst = dyn_cast<ConstantFP>(C)) {
            return convertConstantFP(FpConst, Context);
        }

        if (ConstantDataArray *DataArrayConst = dyn_cast<ConstantDataArray>(C)) {
            std::cout << "Processing ConstantDataArray with " << DataArrayConst->getNumElements()
                      << " elements\n";
            std::vector<Constant *> NewElements;
            for (int i = 0; i < DataArrayConst->getNumElements(); ++i) {
                Constant *Elem = DataArrayConst->getElementAsConstant(i);
                Constant *NewElem = processConstant(Elem, Context);
                if (NewElem != Elem) {
                    NewElements.push_back(NewElem);
                } else {
                    NewElements.push_back(Elem);
                }
            }
            return ConstantArray::get(DataArrayConst->getType(), NewElements);
        }

        if (ConstantArray *ArrConst = dyn_cast<ConstantArray>(C)) {
            std::cout << "Processing ConstantArray with " << ArrConst->getNumOperands()
                      << " elements\n";
            std::vector<Constant *> NewElements;
            for (int i = 0; i < ArrConst->getNumOperands(); ++i) {
                Constant *Elem = ArrConst->getOperand(i);
                Constant *NewElem = processConstant(Elem, Context);
                if (NewElem != Elem) {
                    NewElements.push_back(NewElem);
                } else {
                    NewElements.push_back(Elem);
                }
            }
            return ConstantArray::get(ArrConst->getType(), NewElements);
        }

        return C;
    }
};

PassPluginLibraryInfo getNRSConversionPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "NRSConversionPass", LLVM_VERSION_STRING, [](PassBuilder &PB) {
                // For clang integration - adds the pass at the start of the pipeline
                PB.registerPipelineStartEPCallback(
                    [](ModulePassManager &MPM, llvm::OptimizationLevel Level) {
                        std::cout << "Starting pipeline for NRSConversionPass\n";
                        MPM.addPass(NRSConversionPass());
                    });

                // For opt integration - allows the pass to be found by name
                PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM,
                                                      ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "ieee-to-posit") {
                        std::cout << "Adding NRSConversionPass to pipeline\n";
                        MPM.addPass(NRSConversionPass());
                        return true;
                    }
                    return false;
                });

                std::atexit(NRSConversionPass::nrsslShutdown);
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getNRSConversionPassPluginInfo();
}
