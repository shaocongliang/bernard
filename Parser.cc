#include <Parser.h>
#include <Scanner.h>
#include <iostream>
#include <map>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>

int Precedence(const char &tok) {
    if (tok == gPlus || tok == gSub) return 0;
    else if (tok == gMultiply || tok == gDiv) return 1;
    else return Eof;
}

void Log(const std::string &msg) {
    std::cout << msg << std::endl;
}

void PrintError(const std::string &error) {
    std::cout << error << std::endl;
}

std::unique_ptr<llvm::LLVMContext> g_Context;
std::unique_ptr<llvm::IRBuilder<>> g_Builder;
std::unique_ptr<llvm::Module> g_Module;
std::map<std::string, llvm::Value*> g_NameValues;

llvm::Value *NumberNode::CodeGen() {
    return llvm::ConstantFP::get(*g_Context, llvm::APFloat(m_number));
}

llvm::Value* VariableNode::CodeGen() {
    llvm::Value *pVal = g_NameValues[m_name];
    if (!pVal)
        std::cout << "unknown variable " << m_name << std::endl;
    return pVal;
}

llvm::Value* BinaryOpNode::CodeGen() {
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
        default:
            return nullptr;
    }
}

llvm::Function* FunctionDeclAst::CodeGen() {
    std::vector<llvm::Type *> Doubles(m_args.size(), llvm::Type::getDoubleTy(*g_Context));
    llvm::FunctionType *FT =
            llvm::FunctionType::get(llvm::Type::getDoubleTy(*g_Context), Doubles, false);

    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, m_name, g_Module.get());

    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(m_args[Idx++]);

    return F;
}

llvm::Function *FunctionDefAst::CodeGen() {
    llvm::Function *func = g_Module->getFunction(m_decl->Name());
    if (!func)
        func = m_decl->CodeGen();
    if (!func)
        return nullptr;

    // Create a new basic block to start insertion into.
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*g_Context, "entry", func);
    g_Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    g_NameValues.clear();
    std::cout << "func " << func << " args size " << func->arg_size() << std::endl;
    for (auto &Arg : func->args()) {
        g_NameValues[std::string(Arg.getName())] = &Arg;
    }

    if (llvm::Value *RetVal = m_body->CodeGen()) {
        g_Builder->CreateRet(RetVal);

        // Validate the generated code, checking for consistency.
        // llvm::verifyFunction(*TheFunction);

        return func;
    }
    // Error reading body, remove function.
    func->eraseFromParent();
    return nullptr;
}

llvm::Value *FunctionCallNode::CodeGen() {
    // Look up the name in the global module table.
    llvm::Function *CalleeF = g_Module->getFunction(m_callee);
    if (!CalleeF) {
        printf("Unknown function referenced\n");
        return nullptr;
    }

    // If argument mismatch error.
    if (CalleeF->arg_size() != m_args.size()) {
        printf("Incorrect # arguments passed\n");
        return nullptr;
    }

    std::vector<llvm::Value*> ArgsV;
    for (unsigned i = 0, e = m_args.size(); i != e; ++i) {
        ArgsV.push_back(m_args[i]->CodeGen());
        if (!ArgsV.back())
            return nullptr;
    }

    return g_Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

void PrintToken(const Token &tok) {
    printf("token %s type %d\n", tok.m_val.c_str(), tok.m_type);
}

template<typename T>
T Sum(const T &lhs, const T &rhs, const char &op) {
    if (op == gPlus) return lhs + rhs;
    else if (op == gSub) return lhs - rhs;
    else if (op == gMultiply) return lhs * rhs;
    else if (op == gDiv) return lhs / rhs;
    else return 0;
}

ExprTree *term1(std::string op1, Scanner &scan);

ExprTree *term2(ExprTree *left, Scanner &scan) {
    Token tok = scan.CurToken();
    Error ret = scan.NextToken();
    if (ret == Eof) {
        PrintError("get eof.");
        return left;
    }
    if (tok.m_type != OPERATOR) {
        PrintError("Expect operator");
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
    Error ret = scan.NextToken();
    if (ret == Eof)
        return new ExprTree(tok1.m_val);
    tok2 = scan.CurToken();
    if (Precedence(op1[0]) >= Precedence(tok2.m_val[0]))
        return new ExprTree(tok1.m_val);
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
    if (root->mp_left)
        PrintTreeInOrder(root->mp_left);
    std::cout << "val " << root->m_val << std::endl;
    if (root->mp_right)
        PrintTreeInOrder(root->mp_right);
}

int Calc(ExprTree *root) {
    int val1 = 0;
    int val2 = 0;
    if (root->mp_left)
        val1 = Calc(root->mp_left);
    if (root->mp_right)
        val2 = Calc(root->mp_right);
    if (!IsOperator(root->m_val[0])) return std::stol(root->m_val);
    return Sum(val1, val2, root->m_val[0]);
}

std::unique_ptr<ExprNode> ParsePrimary(const Scanner &scanner);

std::unique_ptr<ExprNode> term1(const std::string &op1, const Scanner &scanner);

std::unique_ptr<ExprNode> term2(std::unique_ptr<ExprNode> &left, const Scanner &scan) {
    Token tok = scan.CurToken();
    PrintToken(tok);
    if (tok.m_type != OPERATOR) {
        PrintError("Expect operator");
        return std::move(left);
    }
    Error ret = scan.NextToken();
    if (ret == Eof) {
        PrintError("get eof.");
        return std::move(left);
    }
    std::unique_ptr<ExprNode> right = term1(tok.m_val, scan);
    std::unique_ptr<ExprNode> opNode(new BinaryOpNode(tok.m_val[0], left, right));
    return term2(opNode, scan);

}

std::unique_ptr<ExprNode> term1(const std::string &op1, const Scanner &scanner) {
    Token tok2;
    std::unique_ptr<ExprNode> node1 = ParsePrimary(scanner);
    tok2 = scanner.CurToken();
    PrintToken(tok2);
    if (tok2.m_type != OPERATOR)
        return node1;
    if (Precedence(op1[0]) >= Precedence(tok2.m_val[0]))
        return node1;
    return term2(node1, scanner);
}

std::unique_ptr<NumberNode> ParseNumber(const Scanner &scanner) {
    Token word = scanner.CurToken();
    scanner.NextToken();
    return std::unique_ptr<NumberNode>(new NumberNode(std::stod(word.m_val)));
}

std::unique_ptr<ExprNode> ParseExpression(const Scanner &scan) {
    std::unique_ptr<ExprNode> lhs = ParsePrimary(scan);
    if (!lhs) return nullptr;
    return term2(lhs, scan);
}

std::unique_ptr<ExprNode> ParseParentheses(const Scanner &scanner) {
    scanner.NextToken();
    std::unique_ptr<ExprNode> exprNode = ParseExpression(scanner);
    if (!exprNode) return nullptr;
    if (scanner.CurToken().m_type != RIGHT_PARENT)
        PrintError("Expected )");
    scanner.NextToken();
    return exprNode;
}

std::unique_ptr<ExprNode> ParseIdentifier(const Scanner &scanner) {
    Token word = scanner.CurToken();
    std::string name = word.m_val;
    scanner.NextToken();
    word = scanner.CurToken();

    // not function call
    if (word.m_type != LEFT_PARENT)
        return std::make_unique<VariableNode>(name);

    // eat (
    scanner.NextToken();
    std::vector<std::unique_ptr<ExprNode>> args;
    word = scanner.CurToken();

    // not no arg function all such as foo(a, b);
    if (word.m_type != RIGHT_PARENT) {
        while (true) {
            std::unique_ptr<ExprNode> arg = ParseExpression(scanner);
            args.push_back(std::move(arg));

            word = scanner.CurToken();
            if (word.m_type == RIGHT_PARENT)
                break;

            if (word.m_val != ",") {
                PrintError("Expect ) or , in function arg list");
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
        case VAR:
            return ParseIdentifier(scanner);
        case NUMBER:
            return ParseNumber(scanner);
        case LEFT_PARENT:
            return ParseParentheses((scanner));
        default:
            PrintError("unknown token");
            return nullptr;
    }
}

std::unique_ptr<FunctionDeclAst> ParseFunctionDecl(const Scanner &scanner) {
    if (scanner.CurToken().m_type != VAR) {
        PrintError("Expected function name.");
        return nullptr;
    }
    std::string fnName = scanner.CurToken().m_val;

    scanner.NextToken();

    const Token &word = scanner.CurToken();
    if (word.m_type != LEFT_PARENT) {
        PrintError("Expect ( in function decl.");
        return nullptr;
    }
    std::vector<std::string> argNames;

    scanner.NextToken();
    while (scanner.CurToken().m_type == VAR) {
        argNames.push_back(scanner.CurToken().m_val);
        scanner.NextToken();
    }

    if (scanner.CurToken().m_type != RIGHT_PARENT) {
        PrintError("Expect ) in function decl.");
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
    if (expr)
        return std::make_unique<FunctionDefAst>(std::move(decl), expr);
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
    std::unique_ptr<FunctionDeclAst> anonymous = std::make_unique<FunctionDeclAst>("__anon_expr__",
                                                                                   std::vector<std::string>());
    return std::make_unique<FunctionDefAst>(std::move(anonymous), expr);
}

void HandleExtern(const Scanner &scanner) {
    if (ParseExtern(scanner))
        std::cout << "Parsed an extern" << std::endl;
    else
        scanner.NextToken();
}

void HandleFunctionDef(const Scanner &scanner) {
    std::unique_ptr<FunctionDefAst> funcDef = ParseFunctionDef(scanner);
    if (funcDef) {
        Log("Parsed an function definition");
        auto funcDefIR =  funcDef->CodeGen();
        funcDefIR->print(llvm::errs());
    }
    else
        scanner.NextToken();
}

void HandleTopLevelExpr(const Scanner &scanner) {
    Log("Parsed an top level expr");
    std::unique_ptr<FunctionDefAst> fn = ParseTopLevelExpr(scanner);
    if (fn) {
        llvm::Function *funcIR = fn->CodeGen();
        if (!funcIR) return;
        funcIR->print(llvm::errs());
        fprintf(stderr, "\n");

        // Remove the anonymous expression.
        funcIR->eraseFromParent();
    }
    else
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

void InitLLVMOpt() {
    g_Context = std::make_unique<llvm::LLVMContext>();
    g_Module = std::make_unique<llvm::Module>("bernard jit", *g_Context);
    g_Builder = std::make_unique<llvm::IRBuilder<>>(*g_Context);
}

void MainLoop(const Scanner &scanner) {
    InitLLVMOpt();
    scanner.NextToken();
    const Token &word = scanner.CurToken();
    while (true) {
        switch (word.m_type) {
            case UNSET:
                return;
            case DEF:
                return HandleFunctionDef(scanner);
            case EXTERN:
                return HandleExtern(scanner);
            default:
                return HandleTopLevelExpr(scanner);
        }
    }
}

