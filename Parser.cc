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


long Sum(const long lhs, const long &rhs, const char &op) {
    if (op == gPlus) return lhs + rhs;
    else if (op == gSub) return lhs - rhs;
    else if (op == gMultiply) return lhs * rhs;
    else if (op == gDiv) return lhs / rhs;
    else return 0;
}

ExprTree *term1(std::string op1, Scanner &scan);

ExprTree *term2(ExprTree *left, Scanner &scan) {
    Token tok;
    Error ret = scan.GetNextToken(tok);
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
    Error ret = scan.GetNextToken(tok1);
    if (ret == Eof)
        return nullptr;
    ret = scan.GetNextToken(tok2);
    if (ret == Eof)
        return new ExprTree(tok1.m_val);
    scan.PutBack(tok2);
    if (Precedence(op1[0]) >= Precedence(tok2.m_val[0]))
        return new ExprTree(tok1.m_val);
    return term2(new ExprTree(tok1.m_val), scan);
}

ExprTree *BuildExprTree(Scanner &scan) {
    Token tok;
    Error ret = scan.GetNextToken(tok);
    ExprTree *lhs = new ExprTree(tok.m_val);
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

std::unique_ptr<ExprAst> Parse(Scanner &scanner) {

}
