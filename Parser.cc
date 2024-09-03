#include <BernardJIT.h>
#include <Parser.h>
#include <Scanner.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Support/Error.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Support/TargetSelect.h>

#include <iostream>
#include <map>

int Precedence(const char &tok) {
    if (tok == gPlus || tok == gSub)
        return 8;
    else if (tok == gMultiply || tok == gDiv)
        return 10;
    else if (tok == gLess)
        return 7;
    else
        return -1;
}

#define DLLEXPORT
extern "C" DLLEXPORT double putchard(double X) {
    fputc((char)X, stderr);
    return 0;
}

void Log(const std::string &msg) { std::cout << msg << std::endl; }

std::unique_ptr<llvm::LLVMContext> g_Context;
std::unique_ptr<llvm::IRBuilder<>> g_Builder;
std::unique_ptr<llvm::Module> g_Module;
std::map<std::string, llvm::Value *> g_NameValues;
std::unique_ptr<llvm::FunctionPassManager> g_FuncPassM;
std::unique_ptr<llvm::LoopAnalysisManager> g_LoopAnalyM;
std::unique_ptr<llvm::FunctionAnalysisManager> g_FuncAnalyM;
std::unique_ptr<llvm::CGSCCAnalysisManager> g_CGSCCM;
std::unique_ptr<llvm::ModuleAnalysisManager> g_ModuleAnaM;
std::unique_ptr<llvm::PassInstrumentationCallbacks> g_PassInstruCbM;
std::unique_ptr<llvm::StandardInstrumentations> g_StandardInstru;
std::map<std::string, std::unique_ptr<FunctionDeclAst>> g_FunctionDecls;
std::unique_ptr<llvm::orc::BernardJIT> g_JIT;
llvm::ExitOnError err;

void InitLLVMOpt() {
    g_Context = std::make_unique<llvm::LLVMContext>();
    g_Module = std::make_unique<llvm::Module>("bernard jit", *g_Context);
    g_Builder = std::make_unique<llvm::IRBuilder<>>(*g_Context);

    g_FuncPassM = std::make_unique<llvm::FunctionPassManager>();
    g_LoopAnalyM = std::make_unique<llvm::LoopAnalysisManager>();
    g_FuncAnalyM = std::make_unique<llvm::FunctionAnalysisManager>();
    g_CGSCCM = std::make_unique<llvm::CGSCCAnalysisManager>();
    g_ModuleAnaM = std::make_unique<llvm::ModuleAnalysisManager>();
    g_PassInstruCbM = std::make_unique<llvm::PassInstrumentationCallbacks>();
    g_StandardInstru = std::make_unique<llvm::StandardInstrumentations>(*g_Context, true);

    g_StandardInstru->registerCallbacks(*g_PassInstruCbM, g_ModuleAnaM.get());

    // add pass
    g_FuncPassM->addPass(llvm::InstCombinePass());
    g_FuncPassM->addPass(llvm::ReassociatePass());
    g_FuncPassM->addPass(llvm::GVNPass());
    g_FuncPassM->addPass(llvm::SimplifyCFGPass());

    llvm::PassBuilder pb;
    pb.registerModuleAnalyses(*g_ModuleAnaM);
    pb.registerFunctionAnalyses(*g_FuncAnalyM);
    pb.crossRegisterProxies(*g_LoopAnalyM, *g_FuncAnalyM, *g_CGSCCM, *g_ModuleAnaM);
}

llvm::Function *getFunction(const std::string &name) {
  // First, see if the function has already been added to the current module.
  if (llvm::Function *F = g_Module->getFunction(name))
    return F;

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = g_FunctionDecls.find(name);
  if (FI != g_FunctionDecls.end())
    return FI->second->CodeGen();

  // If no existing prototype exists, return null.
  return nullptr;
}

llvm::Value *NumberNode::CodeGen() { return llvm::ConstantFP::get(*g_Context, llvm::APFloat(m_number)); }

llvm::Value *VariableNode::CodeGen() {
    llvm::Value *pVal = g_NameValues[m_name];
    if (!pVal) std::cout << "unknown variable " << m_name << std::endl;
    return pVal;
}

llvm::Value *BinaryOpNode::CodeGen() {
    llvm::Value *left = mp_lhs->CodeGen();
    llvm::Value *right = mp_rhs->CodeGen();
    if (!left || !right) return nullptr;

    switch (m_op) {
        case gPlus:
            return g_Builder->CreateFAdd(left, right, "addtmp");
        case gSub:
            return g_Builder->CreateSub(left, right, "subtmp");
        case gMultiply:
            return g_Builder->CreateMul(left, right, "multmp");
        case gLess:
            left = g_Builder->CreateFCmpULT(left, right, "cmptmp");
            return g_Builder->CreateUIToFP(left, llvm::Type::getDoubleTy(*g_Context), "booltmp");
        default:
            return nullptr;
    }
}

llvm::Value* ConditionNode::CodeGen() {
    llvm::Value *cond = m_cond->CodeGen();
    if (!cond) return nullptr;

    cond = g_Builder->CreateFCmpONE(cond, llvm::ConstantFP::get(*g_Context, llvm::APFloat(0.0)), "ifcond");

    llvm::Function *func = g_Builder->GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(*g_Context, "then", func);
    llvm::BasicBlock *elseBlock = llvm::BasicBlock::Create(*g_Context, "else");
    llvm::BasicBlock *mergeBlock = llvm::BasicBlock::Create(*g_Context, "ifcont");

    g_Builder->CreateCondBr(cond, thenBlock, elseBlock);

    // emit
    g_Builder->SetInsertPoint(thenBlock);

    llvm::Value *thenValue = m_then->CodeGen();
    if (!thenValue) return nullptr;

    g_Builder->CreateBr(mergeBlock);

    thenBlock = g_Builder->GetInsertBlock();

    // emit else block
    func->insert(func->end(), elseBlock);
    g_Builder->SetInsertPoint(elseBlock);

    llvm::Value *elseValue = m_else->CodeGen();
    if (!elseValue) return nullptr;

    g_Builder->CreateBr(mergeBlock);
    elseBlock = g_Builder->GetInsertBlock();

    func->insert(func->end(), mergeBlock);
    g_Builder->SetInsertPoint(mergeBlock);
    llvm::PHINode *phi = g_Builder->CreatePHI(llvm::Type::getDoubleTy(*g_Context), 2, "iftmp");
    phi->addIncoming(thenValue, thenBlock);
    phi->addIncoming(elseValue, elseBlock);
    return phi;
}

llvm::Value* ForLoopNode::CodeGen() {
    llvm::Value *start = mp_start->CodeGen();
    if (!start) return nullptr;

    llvm::Function *func = g_Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *preheaderBlock = g_Builder->GetInsertBlock();
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(*g_Context, "loop", func);

    g_Builder->CreateBr(loopBlock);

    g_Builder->SetInsertPoint(loopBlock);

    llvm::PHINode *variable = g_Builder->CreatePHI(llvm::Type::getDoubleTy(*g_Context), 2, m_valName);
    variable->addIncoming(start, preheaderBlock);

    llvm::Value *oldVal = g_NameValues[m_valName];
    g_NameValues[m_valName] = variable;

    // emit the body of loop
    if (!mp_body->CodeGen()) return nullptr;

    llvm::Value *stepVal;
    if (mp_step) {
        stepVal = mp_step->CodeGen();
        if (!stepVal) {
            Log("fail in step val code gen.");
            return nullptr;
        }
    } else {
        stepVal = llvm::ConstantFP::get(*g_Context, llvm::APFloat(1.0));
    }

    llvm::Value *nextVal = g_Builder->CreateFAdd(variable, stepVal, "nextVal");

    llvm::Value *endCond = mp_end->CodeGen();
    if (!endCond) return nullptr;

    endCond = g_Builder->CreateFCmpONE(endCond, llvm::ConstantFP::get(*g_Context, llvm::APFloat(0.0)), "loopCond");

    llvm::BasicBlock *loopEndBlock = g_Builder->GetInsertBlock();
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(*g_Context, "afterLoop", func);

    g_Builder->CreateCondBr(endCond, loopEndBlock, afterBlock);

    g_Builder->SetInsertPoint(afterBlock);

    variable->addIncoming(nextVal, loopEndBlock);

    if (oldVal)
        g_NameValues[m_valName] = oldVal;
    else
        g_NameValues.erase(m_valName);

    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*g_Context));
}

llvm::Function *FunctionDeclAst::CodeGen() {
    std::vector<llvm::Type *> Doubles(m_args.size(), llvm::Type::getDoubleTy(*g_Context));
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*g_Context), Doubles, false);

    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, m_name, g_Module.get());
    unsigned Idx = 0;
    for (auto &Arg : F->args()) Arg.setName(m_args[Idx++]);
    return F;
}

llvm::Function *FunctionDefAst::CodeGen() {
    std::string funcName = m_decl->Name();
    g_FunctionDecls[funcName] = std::move(m_decl);
    llvm::Function *func = getFunction(funcName);
    if (!func) { 
        printf("get function %s fail\n", funcName.c_str());
        return nullptr;
    }

    // Create a new basic block to start insertion into.
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*g_Context, "entry", func);
    g_Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    g_NameValues.clear();
    for (auto &Arg : func->args()) {
        g_NameValues[std::string(Arg.getName())] = &Arg;
    }

    llvm::Value *retVal = m_body->CodeGen();
    if (retVal) {
        g_Builder->CreateRet(retVal);

        g_FuncPassM->run(*func, *g_FuncAnalyM);
        return func;
    }
    printf("function body ir generation fail.\n");
    // Error reading body, remove function.
    func->eraseFromParent();
    return nullptr;
}

llvm::Value *FunctionCallNode::CodeGen() {
    // Look up the name in the global module table.
    llvm::Function *CalleeF = g_Module->getFunction(m_callee);
    if (!CalleeF) {
        printf("Unknown function %s referenced\n", m_callee.c_str());
        return nullptr;
    }

    // If argument mismatch error.
    if (CalleeF->arg_size() != m_args.size()) {
        printf("Incorrect # arguments passed\n");
        return nullptr;
    }

    std::vector<llvm::Value *> ArgsV;
    for (unsigned i = 0, e = m_args.size(); i != e; ++i) {
        ArgsV.push_back(m_args[i]->CodeGen());
        if (!ArgsV.back()) return nullptr;
    }

    return g_Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

template <typename T>
T Sum(const T &lhs, const T &rhs, const char &op) {
    if (op == gPlus)
        return lhs + rhs;
    else if (op == gSub)
        return lhs - rhs;
    else if (op == gMultiply)
        return lhs * rhs;
    else if (op == gDiv)
        return lhs / rhs;
    else
        return 0;
}

ExprTree *term1(std::string op1, Scanner &scan);

ExprTree *term2(ExprTree *left, Scanner &scan) {
    Token tok = scan.CurToken();
    Token tok2 = scan.NextToken();
    if (tok2.m_type == TokenType::Eof) return left;
    if (tok.m_type != TokenType::OPERATOR) {
        Log("Expect operator");
        return nullptr;
    }
    ExprTree *operNode = new ExprTree(tok.m_val);
    operNode->mp_left = left;
    operNode->mp_right = term1(tok.m_val, scan);
    return term2(operNode, scan);
}

ExprTree *term1(std::string op1, Scanner &scan) {
    Token tok1, tok2;
    tok1 = scan.CurToken();
    tok2 = scan.NextToken();
    if (tok2.m_type == TokenType::Eof) return new ExprTree(tok1.m_val);
    tok2 = scan.CurToken();
    if (Precedence(op1[0]) >= Precedence(tok2.m_val[0])) return new ExprTree(tok1.m_val);
    return term2(new ExprTree(tok1.m_val), scan);
}

ExprTree *BuildExprTree(Scanner &scan) {
    scan.NextToken();
    Token tok = scan.CurToken();
    ExprTree *lhs = new ExprTree(tok.m_val);
    scan.NextToken();
    return term2(lhs, scan);
}

void PrintTreeInOrder(ExprTree *root) {
    if (root->mp_left) PrintTreeInOrder(root->mp_left);
    std::cout << "val " << root->m_val << std::endl;
    if (root->mp_right) PrintTreeInOrder(root->mp_right);
}

int Calc(ExprTree *root) {
    int val1 = 0;
    int val2 = 0;
    if (root->mp_left) val1 = Calc(root->mp_left);
    if (root->mp_right) val2 = Calc(root->mp_right);
    if (!IsOperator(root->m_val[0])) return std::stol(root->m_val);
    return Sum(val1, val2, root->m_val[0]);
}

std::unique_ptr<ExprNode> ParsePrimary(const Scanner &scanner);
std::unique_ptr<ExprNode> term2(std::unique_ptr<ExprNode> &left, const Scanner &scan);

std::unique_ptr<ExprNode> ParseExpression(const Scanner &scan) {
    std::unique_ptr<ExprNode> lhs = ParsePrimary(scan);
    if (!lhs) return nullptr;
    return term2(lhs, scan);
}

std::unique_ptr<ExprNode> term2(std::unique_ptr<ExprNode> &left, const Scanner &scan) {
    Token op1 = scan.CurToken();
    if (op1.m_type != TokenType::OPERATOR)
        return std::move(left);

    Token op2 = scan.NextToken();
    if (op2.m_type == TokenType::Eof) return nullptr;

    std::unique_ptr<ExprNode> right = ParsePrimary(scan);
    op2 = scan.CurToken();
    if (op2.m_type == TokenType::Eof || Precedence(op1.m_val[0]) >= Precedence(op2.m_val[0])) {
        std::unique_ptr<ExprNode> node = std::make_unique<BinaryOpNode>(BinaryOpNode(op1.m_val[0], left, right));
        return term2(node, scan);
    } else {
        right = term2(right, scan);
        return std::make_unique<BinaryOpNode>(op1.m_val[0], left, right);
    }
}

std::unique_ptr<NumberNode> ParseNumber(const Scanner &scanner) {
    Token word = scanner.CurToken();
    scanner.NextToken();
    return std::unique_ptr<NumberNode>(new NumberNode(std::stod(word.m_val)));
}


std::unique_ptr<ConditionNode> ParseIf(const Scanner &scanner) {
    scanner.NextToken();

    std::unique_ptr<ExprNode> cond = ParseExpression(scanner);
    if (!cond) return nullptr;

    if (scanner.CurToken().m_type != TokenType::THEN) {
        return nullptr;
    }
    scanner.NextToken();

    std::unique_ptr<ExprNode> then = ParseExpression(scanner);
    if (!then) return nullptr;

    if (scanner.CurToken().m_type != TokenType::ELSE) {
        Log("expected else");
        return nullptr;
    }

    scanner.NextToken();

    std::unique_ptr<ExprNode> elsePart = ParseExpression(scanner);
    if (!elsePart) return nullptr;

    return std::make_unique<ConditionNode>(cond, then, elsePart);
}

std::unique_ptr<ExprNode> ParseForLoop(const Scanner &scan) {
    scan.NextToken();

    if (scan.CurToken().m_type != TokenType::VAR) {
        Log("Expect variable name");
        return nullptr;
    }

    std::string varName = scan.CurToken().m_val;

    scan.NextToken();

    if (scan.CurToken().m_val != "=") {
        Log("expect = after for");
        return nullptr;
    }
    scan.NextToken();

    std::unique_ptr<ExprNode> start = ParseExpression(scan);
    if (!start) return nullptr;

    if (scan.CurToken().m_val != ",") {
        Log("Expect , after start value.");
        return nullptr;
    }
    scan.NextToken();

    std::unique_ptr<ExprNode> end = ParseExpression(scan);
    if (!end) return nullptr;

    std::unique_ptr<ExprNode> step;
    // has step
    if (scan.CurToken().m_val == ",") {
        scan.NextToken();
        step = ParseExpression(scan);
        if (!step) return nullptr;
    }
    if (scan.CurToken().m_type != TokenType::IN) {
        Log("Expect in after for.");
        return nullptr;
    }
    scan.NextToken();

    std::unique_ptr<ExprNode> body = ParseExpression(scan);
    if (!body) return nullptr;

    return std::make_unique<ForLoopNode>(varName, std::move(start), std::move(end), std::move(step), std::move(body));
}

std::unique_ptr<ExprNode> ParseParentheses(const Scanner &scanner) {
    scanner.NextToken();
    std::unique_ptr<ExprNode> exprNode = ParseExpression(scanner);
    if (!exprNode) return nullptr;
    if (scanner.CurToken().m_type != TokenType::RIGHT_PARENT) {
        Log("Expected )");
        return nullptr;
    }
    scanner.NextToken();
    return exprNode;
}

std::unique_ptr<ExprNode> ParseIdentifier(const Scanner &scanner) {
    Token word = scanner.CurToken();
    std::string name = word.m_val;
    scanner.NextToken();
    word = scanner.CurToken();

    // not function call
    if (word.m_type != TokenType::LEFT_PARENT) return std::make_unique<VariableNode>(name);

    // eat (
    scanner.NextToken();
    std::vector<std::unique_ptr<ExprNode>> args;
    word = scanner.CurToken();

    // not no arg function all such as foo(a, b);
    if (word.m_type != TokenType::RIGHT_PARENT) {
        while (true) {
            std::unique_ptr<ExprNode> arg = ParseExpression(scanner);
            args.push_back(std::move(arg));

            word = scanner.CurToken();
            if (word.m_type == TokenType::RIGHT_PARENT) break;

            if (word.m_val != ",") {
                Log("Expect ) or , in function arg list");
                return nullptr;
            }
            scanner.NextToken();
        }
    }

    scanner.NextToken();
    return std::make_unique<FunctionCallNode>(name, std::move(args));
}

std::unique_ptr<ExprNode> ParsePrimary(const Scanner &scanner) {
    switch (scanner.CurToken().m_type) {
        case TokenType::VAR:
            return ParseIdentifier(scanner);
        case TokenType::NUMBER:
            return ParseNumber(scanner);
        case TokenType::LEFT_PARENT:
            return ParseParentheses(scanner);
        case TokenType::FOR:
            return ParseForLoop(scanner);
        case TokenType::IF:
            return ParseIf(scanner);
        default:
            Log("unknown token");
            return nullptr;
    }
}

std::unique_ptr<FunctionDeclAst> ParseFunctionDecl(const Scanner &scanner) {
    if (scanner.CurToken().m_type != TokenType::VAR) {
        Log("Expected function name.");
        return nullptr;
    }
    std::string fnName = scanner.CurToken().m_val;

    const Token & word = scanner.NextToken();
    if (word.m_type != TokenType::LEFT_PARENT) {
        Log("Expect ( in function decl.");
        return nullptr;
    }
    std::vector<std::string> argNames;

    auto tok = scanner.NextToken();
    while (tok.m_type == TokenType::VAR) {
        argNames.push_back(tok.m_val);
        tok = scanner.NextToken();
    }

    if (tok.m_type != TokenType::RIGHT_PARENT) {
        Log("Expect ) in function decl.");
        return nullptr;
    }

    scanner.NextToken();
    return std::make_unique<FunctionDeclAst>(fnName, argNames);
}

std::unique_ptr<FunctionDefAst> ParseFunctionDef(const Scanner &scanner) {
    scanner.NextToken();
    std::unique_ptr<FunctionDeclAst> decl = ParseFunctionDecl(scanner);
    if (!decl) return nullptr;

    std::unique_ptr<ExprNode> expr = ParseExpression(scanner);
    if (expr) return std::make_unique<FunctionDefAst>(std::move(decl), expr);
    printf("parse expression in function define fail.\n");
    return nullptr;
}

std::unique_ptr<FunctionDeclAst> ParseExtern(const Scanner &scanner) {
    // eat extern keyword
    scanner.NextToken();
    return ParseFunctionDecl(scanner);
}

std::unique_ptr<FunctionDefAst> ParseTopLevelExpr(const Scanner &scanner) {
    std::unique_ptr<ExprNode> expr = ParseExpression(scanner);
    if (!expr) return nullptr;
    std::unique_ptr<FunctionDeclAst> anonymous =
        std::make_unique<FunctionDeclAst>("__anon_expr__", std::vector<std::string>());
    return std::make_unique<FunctionDefAst>(std::move(anonymous), expr);
}

void HandleExtern(const Scanner &scanner) {
    if (std::unique_ptr<FunctionDeclAst> func = ParseExtern(scanner)) {
        llvm::Function *ir = func->CodeGen();
        if (!ir) {
            return;
        }
        ir->print(llvm::errs());
        g_FunctionDecls[func->Name()] = std::move(func);
    } else
        scanner.NextToken();
}

void HandleFunctionDef(const Scanner &scanner) {
    std::unique_ptr<FunctionDefAst> funcDef = ParseFunctionDef(scanner);
    if (funcDef) {
        llvm::Function *funcDefIR = funcDef->CodeGen();
        if (!funcDefIR) {
            printf("func definition IR generate error.\n");
            return;
        }
        funcDefIR->print(llvm::errs());
        err(g_JIT->addModule(llvm::orc::ThreadSafeModule(std::move(g_Module), std::move(g_Context))));
        InitLLVMOpt();
    } else
        scanner.NextToken();
}

void HandleTopLevelExpr(const Scanner &scanner) {
    std::unique_ptr<FunctionDefAst> fn = ParseTopLevelExpr(scanner);
    if (fn) {
        llvm::Function *funcIR = fn->CodeGen();
        if (!funcIR) return;
        auto tracker = g_JIT->getMainJITDylib().createResourceTracker();
        auto thrSafeModule = llvm::orc::ThreadSafeModule(std::move(g_Module), std::move(g_Context));
        err(g_JIT->addModule(std::move(thrSafeModule), tracker));

        InitLLVMOpt();

        auto ExprSymbol = err(g_JIT->lookup("__anon_expr"));

        // Get the symbol's address and cast it to the right type (takes no
        // arguments, returns a double) so we can call it as a native function.
        double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>();
        fprintf(stderr, "Evaluated to %f\n", FP());

        funcIR->print(llvm::errs());
        fprintf(stderr, "\n");

        err(tracker->remove());
    } else
        scanner.NextToken();
}

double Calc(ExprNode *root) {
    if (dynamic_cast<BinaryOpNode *>(root)) {
        BinaryOpNode *pb = dynamic_cast<BinaryOpNode *>(root);
        double left = Calc(pb->mp_lhs.get());
        double right = Calc(pb->mp_rhs.get());
        return Sum(left, right, pb->m_op);
    } else if (dynamic_cast<NumberNode *>(root)) {
        return dynamic_cast<NumberNode *>(root)->m_number;
    } else {
        return 0.0;
    }
}

void MainLoop(const Scanner &scanner) {
    llvm::InitializeNativeTarget();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    g_JIT = err(llvm::orc::BernardJIT::Create());
    InitLLVMOpt();
    scanner.NextToken();
    while (true) {
        const Token &word = scanner.CurToken();
        switch (word.m_type) {
            case TokenType::Eof:
                return;
            case TokenType::SEMICOLON:
                scanner.NextToken();
                break;
            case TokenType::DEF:
                HandleFunctionDef(scanner);
                break;
            case TokenType::EXTERN:
                HandleExtern(scanner);
                break;
            default:
                HandleTopLevelExpr(scanner);
                break;
        }
    }
}
