#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <Scanner.h>

class ExprAst {
   public:
    ExprAst() = default;
};

class NumberAst : public ExprAst {
   public:
    explicit NumberAst(const double &num) : m_number(num) {}

   private:
    double m_number;
};

class VariableAst : public ExprAst {
   public:
    explicit VariableAst(const std::string &name) : m_name(name) {}

   private:
    std::string m_name;
};

class BinaryOpAst : public ExprAst {
   public:
    BinaryOpAst(char op, std::unique_ptr<ExprAst> &lhs, std::unique_ptr<ExprAst> &rhs)
        : m_op(op), mp_lhs(std::move(lhs)), mp_rhs(std::move(rhs)) {}

   private:
    char m_op;
    std::unique_ptr<ExprAst> mp_lhs;
    std::unique_ptr<ExprAst> mp_rhs;
};

class FunctionDeclAst {
   public:
    FunctionDeclAst(const std::string &name, const std::vector<std::string> &args) : m_name(name), m_args(args) {}

   private:
    std::string m_name;
    std::vector<std::string> m_args;
};

class FunctionDefAst {
public:
    std::unique_ptr<FunctionDeclAst> m_decl;
    std::unique_ptr<ExprAst> m_body;
private:
    FunctionDefAst(std::unique_ptr<FunctionDeclAst> decl, std::unique_ptr<ExprAst> &body): m_decl(std::move(decl)), m_body(std::move(body)) {
    }
};

struct ExprTree {
    ExprTree(ExprTree *lhs, ExprTree *rhs, const std::string &val):
        mp_left(lhs), mp_right(rhs), m_val(val) {}
    ExprTree(const std::string &val) : mp_left(nullptr), mp_right(nullptr), m_val(val) {}
    ExprTree *mp_left;
    ExprTree *mp_right;
    std::string m_val;
};

ExprTree *BuildExprTree(Scanner &scan);

void PrintTreeInOrder(ExprTree *root);

int Calc(ExprTree *root);

std::unique_ptr<ExprAst> Parse(Scanner &scanner);