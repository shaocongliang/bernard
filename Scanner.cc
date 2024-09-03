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

bool IsOperator(const char tok) {
    return tok == gPlus || tok == gSub || tok == gMultiply || tok == gDiv || tok == gLess;
}

Scanner::Scanner(const std::string &src) : m_src(src), m_idx(0) {}

Scanner::~Scanner() {}

Token Scanner::NextToken() const {
    char ch = 0;
    m_peek.Reset();
    if (m_idx == m_src.length()) return m_peek;
    do {
        ch = m_src[m_idx++];
        if (m_peek.m_type == TokenType::Eof) {
            if (ch == gSpace) continue;
            m_peek.m_val.push_back(ch);
            if (std::isdigit(ch))
                m_peek.m_type = TokenType::NUMBER;
            else if (std::isalpha(ch))
                m_peek.m_type =  TokenType::VAR;
            else if (IsOperator(ch)) {
                m_peek.m_type = TokenType::OPERATOR;
                return m_peek;
            } else if (ch == gEqual) {
                m_peek.m_type = TokenType::OPERATOR;
                return m_peek;
            } else if (ch == gSemicolon) {
                m_peek.m_type = TokenType::SEMICOLON;
                return m_peek;
            } else if (ch == gLeftParentheses) {
                m_peek.m_type = TokenType::LEFT_PARENT;
                return m_peek;
            } else if (ch == gRightParentheses) {
                m_peek.m_type = TokenType::RIGHT_PARENT;
                return m_peek;
            } else if (ch == gSemicolon) {
                m_peek.m_type = TokenType::SEMICOLON;
                return m_peek;
            } else
                return m_peek;
        } else if (m_peek.m_type == TokenType::NUMBER) {
            if (!std::isdigit(ch) && ch != gDot) {
                if (ch != gSpace) m_idx--;
                return m_peek;
            }
            m_peek.m_val.push_back(ch);
        } else if (m_peek.m_type == TokenType::VAR) {
            if (!std::isalnum(ch)) {
                if (m_peek.m_val == "extern")
                    m_peek.m_type = TokenType::EXTERN;
                else if (m_peek.m_val == "def")
                    m_peek.m_type = TokenType::DEF;
                else if (m_peek.m_val == "if")
                    m_peek.m_type = TokenType::IF;
                else if (m_peek.m_val == "then")
                    m_peek.m_type = TokenType::THEN;
                else if (m_peek.m_val == "else")
                    m_peek.m_type = TokenType::ELSE;
                else if (m_peek.m_val == "for")
                    m_peek.m_type = TokenType::FOR;
                else if (m_peek.m_val == "in")
                    m_peek.m_type = TokenType::IN;
                if (ch != gSpace) m_idx--;
                return m_peek;
            }
            m_peek.m_val.push_back(ch);
        }
    } while (m_idx < m_src.length());
    return m_peek;
}

const Token &Scanner::CurToken() const { return m_peek; }