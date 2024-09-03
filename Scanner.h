#pragma once

#include <string>

enum class TokenType {
    VAR,
    NUMBER,
    OPERATOR,
    FUNCTION,
    SEMICOLON,
    LEFT_PARENT,
    RIGHT_PARENT,
    EXTERN,
    DEF,
    IF,
    THEN,
    ELSE,
    FOR,
    IN,
    Eof,
};

struct Token {
    Token(const std::string &val, const TokenType &t) : m_val(val), m_type(t) {}

    Token() : m_type(TokenType::Eof) {}

    void Reset() {
        m_type = TokenType::Eof;
        m_val.clear();
    }

    ~Token() {}

    std::string m_val;
    TokenType m_type;
};

const char gPlus = '+';
const char gSub = '-';
const char gMultiply = '*';
const char gDiv = '/';
const char gLess = '<';

bool IsOperator(const char tok);

class Scanner {
public:
    explicit Scanner(const std::string &src);

    ~Scanner();

    Token NextToken() const;
    const Token& CurToken() const;
private:
    std::string m_src;
    mutable std::size_t m_idx;
    mutable Token m_peek;
};
