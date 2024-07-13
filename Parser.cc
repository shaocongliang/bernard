#include <Parser.h>
#include <Scanner.h>
#include <iostream>


int Precedence(const char &tok) {
    if (tok == gPlus || tok == gSub) return 0;
    else if (tok == gMultiply || tok == gDiv) return 1;
    else return Eof;
}

void PrintError(const std::string &error) {
    std::cout << error << std::endl;
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
    Error ret = scan.GetNextToken();
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
    Error ret = scan.GetNextToken();
    if (ret == Eof)
        return new ExprTree(tok1.m_val);
    tok2 = scan.CurToken();
    if (Precedence(op1[0]) >= Precedence(tok2.m_val[0]))
        return new ExprTree(tok1.m_val);
    return term2(new ExprTree(tok1.m_val), scan);
}

ExprTree *BuildExprTree(Scanner &scan) {
    scan.GetNextToken();
    Token tok = scan.CurToken();
    ExprTree *lhs = new ExprTree(tok.m_val);
    scan.GetNextToken();
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
    Error ret = scan.GetNextToken();
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
    scanner.GetNextToken();
    return std::unique_ptr<NumberNode>(new NumberNode(std::stod(word.m_val)));
}

std::unique_ptr<ExprNode> ParseExpression(const Scanner &scan) {
    std::unique_ptr<ExprNode> lhs = ParsePrimary(scan);
    if (!lhs) return nullptr;
    return term2(lhs, scan);
}

std::unique_ptr<ExprNode> ParseParentheses(const Scanner &scanner) {
    scanner.GetNextToken();
    std::unique_ptr<ExprNode> exprNode = ParseExpression(scanner);
    if (!exprNode) return nullptr;
    if (scanner.CurToken().m_type != RIGHT_PARENT)
        PrintError("Expected )");
    scanner.GetNextToken();
    return exprNode;
}

std::unique_ptr<ExprNode> ParseIdentifier(const Scanner &scanner) {
    Token word = scanner.CurToken();
    std::string name = word.m_val;
    scanner.GetNextToken();
    word = scanner.CurToken();

    // not function call
    if (word.m_type != LEFT_PARENT)
        return std::make_unique<VariableNode>(name);

    // eat (
    scanner.GetNextToken();
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
            scanner.GetNextToken();
        }
    }

    scanner.GetNextToken();
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

    scanner.GetNextToken();

    const Token &word = scanner.CurToken();
    if (word.m_type != LEFT_PARENT) {
        PrintError("Expect ( in function decl.");
        return nullptr;
    }
    std::vector<std::string> argNames;

    scanner.GetNextToken();
    while (scanner.CurToken().m_type == VAR) {
        argNames.push_back(scanner.CurToken().m_val);
        scanner.GetNextToken();
    }

    if (scanner.CurToken().m_type != RIGHT_PARENT) {
        PrintError("Expect ) in function decl.");
        return nullptr;
    }

    scanner.GetNextToken();

    return std::make_unique<FunctionDeclAst>(fnName, argNames);
}

std::unique_ptr<FunctionDefAst> ParseFunctionDef(const Scanner &scanner) {
    scanner.GetNextToken();
    std::unique_ptr<FunctionDeclAst> decl = ParseFunctionDecl(scanner);
    if (!decl) return nullptr;

    std::unique_ptr<ExprNode> expr = ParseExpression(scanner);
    if (expr)
        return std::make_unique<FunctionDefAst>(std::move(decl), expr);
    return nullptr;
}

std::unique_ptr<FunctionDeclAst> ParseExtern(const Scanner &scanner) {
    // eat extern keyword
    scanner.GetNextToken();
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
        scanner.GetNextToken();
}

void HandleFunctionDef(const Scanner &scanner) {
    if (ParseFunctionDef(scanner))
        std::cout << "Parsed an function definition" << std::endl;
    else
        scanner.GetNextToken();
}

void HandleTopLevelExpr(const Scanner &scanner) {
    if (ParseTopLevelExpr(scanner))
        std::cout << "Parsed an top level expr" << std::endl;
    else
        scanner.GetNextToken();
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
    scanner.GetNextToken();
    const Token &word = scanner.CurToken();
    while (true) {
        switch (word.m_type) {
            case UNSET:
                return;
            case DEF:
                HandleFunctionDef(scanner);
                exit(0);
            case EXTERN:
                HandleExtern(scanner);
                break;
            default:
                HandleTopLevelExpr(scanner);
                break;
        }
    }
}