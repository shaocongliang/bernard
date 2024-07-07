#pragma once

#include <string>

enum TokenType {
    VAR,
    NUMBER,
    OPERATOR,
    FUNCTION,
    SEMICOLON,
    LEFT_PARENT,
    RIGHT_PARENT,
    UNSET,
};

enum Error {
    Success = 0,
    Eof = -1,
    UnExpect = -2,
};

struct Token {
    Token(const std::string &val, const TokenType &t) : m_val(val), m_type(t) {}

    Token() : m_type(UNSET) {}

    ~Token() {}

    std::string m_val;
    TokenType m_type;
};

const char gPlus = '+';
const char gSub = '-';
const char gMultiply = '*';
const char gDiv = '/';

bool IsOperator(const char tok);

class Scanner {
public:
    explicit Scanner(const std::string &src);

    ~Scanner();

    Error GetNextToken(Token &tok);

    void PutBack(const Token &peek);

private:
    std::string m_src;
    std::size_t m_idx;
    Token m_peek;
};
