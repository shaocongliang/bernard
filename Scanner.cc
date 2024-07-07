#include <Scanner.h>

#include <cctype>
#include <set>

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

bool IsAlphaOrNumber(const char ch) { return std::isalpha(ch) || std::isalnum(ch); }

Error Scanner::GetNextToken(Token &tok) {
    if (m_idx == m_src.length()) return Eof;
    char ch = 0;
    if (m_peek.m_type != UNSET) {
        tok = m_peek;
        m_peek.m_type = UNSET;
        return Success;
    }

    tok.m_type = UNSET;
    do {
        ch = m_src[m_idx++];
        if (tok.m_type == UNSET) {
            if (ch == gSpace)
                continue;
            tok.m_val.push_back(ch);
            if (std::isdigit(ch))
                tok.m_type = NUMBER;
            else if (std::isalpha(ch))
                tok.m_type = VAR;
            else if (IsOperator(ch)) {
                tok.m_type = OPERATOR;
                return Success;
            } else if (ch == gEqual) {
                tok.m_type = OPERATOR;
                return Success;
            } else if (ch == gSemicolon) {
                tok.m_type = SEMICOLON;
                return Success;
            } else if (ch == gLeftParentheses) {
                tok.m_type = LEFT_PARENT;
                return Success;
            } else if (ch == gRightParentheses) {
                tok.m_type = RIGHT_PARENT;
                return Success;
            } else
                return UnExpect;
        } else if (tok.m_type == NUMBER) {
            if (!std::isdigit(ch) && ch != gDot) {
                if (ch != gSpace)
                    m_idx--;
                return Success;
            }
            tok.m_val.push_back(ch);
        } else if (tok.m_type == VAR) {
            if (!std::isalnum(ch)) {
                if (ch != gSpace)
                    m_idx--;
                return Success;
            }
        }
    } while (m_idx < m_src.length());
    return tok.m_val.empty() ? Eof : Success;
}

void Scanner::PutBack(const Token &peek) { m_peek = peek; }