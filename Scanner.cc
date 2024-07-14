#include <Scanner.h>

#include <cctype>
#include <set>
#include <iostream>

const char gSpace = ' ';
const char gEqual = '=';
const char gSemicolon = ';';
const char gDot = '.';
const char gLeftParentheses = '(';
const char gRightParentheses = ')';
const std::set<std::string> gKeyWords = {
        "double",
};

enum WordStatus {
    BEGIN = 0,
    IN_NUMBER = 1,
    IN_VAR = 2,
};

bool IsOperator(const char tok) { return tok == gPlus || tok == gSub || tok == gMultiply || tok == gDiv; }

Scanner::Scanner(const std::string &src) : m_src(src), m_idx(0) {}

Scanner::~Scanner() {}

Error Scanner::NextToken() const {
    if (m_idx == m_src.length()) return Eof;
    char ch = 0;
    m_peek.Reset();
    do {
        ch = m_src[m_idx++];
        if (m_peek.m_type == UNSET) {
            if (ch == gSpace)
                continue;
            m_peek.m_val.push_back(ch);
            if (std::isdigit(ch))
                m_peek.m_type = NUMBER;
            else if (std::isalpha(ch))
                m_peek.m_type = VAR;
            else if (IsOperator(ch)) {
                m_peek.m_type = OPERATOR;
                return Success;
            } else if (ch == gEqual) {
                m_peek.m_type = OPERATOR;
                return Success;
            } else if (ch == gSemicolon) {
                m_peek.m_type = SEMICOLON;
                return Success;
            } else if (ch == gLeftParentheses) {
                m_peek.m_type = LEFT_PARENT;
                return Success;
            } else if (ch == gRightParentheses) {
                m_peek.m_type = RIGHT_PARENT;
                return Success;
            } else
                return UnExpect;
        } else if (m_peek.m_type == NUMBER) {
            if (!std::isdigit(ch) && ch != gDot) {
                if (ch != gSpace)
                    m_idx--;
                return Success;
            }
            m_peek.m_val.push_back(ch);
        } else if (m_peek.m_type == VAR) {
            if (!std::isalnum(ch)) {
                std::cout << m_peek.m_val << std::endl;
                if (m_peek.m_val == "extern")
                    m_peek.m_type = EXTERN;
                else if (m_peek.m_val == "def") {
                    m_peek.m_type = DEF;
                }
                if (ch != gSpace)
                    m_idx--;
                return Success;
            }
            m_peek.m_val.push_back(ch);
        }
    } while (m_idx < m_src.length());
    return m_peek.m_val.empty() ? Eof : Success;
}

const Token & Scanner::CurToken() const {
    return m_peek;
}