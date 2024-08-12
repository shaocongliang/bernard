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

bool IsOperator(const char tok) { return tok == gPlus || tok == gSub || tok == gMultiply || tok == gDiv; }

Scanner::Scanner(const std::string &src) : m_src(src), m_idx(0) {}

Scanner::~Scanner() {}

Error Scanner::NextToken() const {
    char ch = 0;
    m_peek.Reset();
    if (m_idx == m_src.length()) return Eof;
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
            } else if (ch == gSemicolon) {
                m_peek.m_type = SEMICOLON;
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
                if (m_peek.m_val == "extern")
                    m_peek.m_type = EXTERN;
                else if (m_peek.m_val == "def")
                    m_peek.m_type = DEF;
                else if (m_peek.m_val == "if")
                    m_peek.m_type = IF;
                else if (m_peek.m_val == "then")
                    m_peek.m_type = THEN;
                else if (m_peek.m_val == "else")
                    m_peek.m_type = ELSE;
                else if (m_peek.m_val == "for")
                    m_peek.m_type = FOR;
                else if (m_peek.m_val == "in")
                    m_peek.m_type = IN;
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