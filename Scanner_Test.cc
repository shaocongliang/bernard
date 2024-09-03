#include <gtest/gtest.h>
#include <Scanner.h>

TEST(Scanner, test1) {
    const std::string src("2 * 3 * 465");
    Scanner sc(src);
    std::vector<Token> tokens;
    Token tok = sc.NextToken();
    while (tok.m_type != TokenType::Eof) {
        tokens.push_back(sc.CurToken());
        tok = sc.NextToken();
    }

    EXPECT_EQ(tokens.size(), 5);

    EXPECT_EQ(tokens[0].m_type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].m_val, "2");

    EXPECT_EQ(tokens[1].m_type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[1].m_val, "*");

    EXPECT_EQ(tokens[2].m_type, TokenType::NUMBER);
    EXPECT_EQ(tokens[2].m_val, "3");

    EXPECT_EQ(tokens[3].m_type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[3].m_val, "*");

    EXPECT_EQ(tokens[4].m_type, TokenType::NUMBER);
    EXPECT_EQ(tokens[4].m_val, "465");
}

TEST(Scanner, test2) {
    const std::string src("a=10.36;");
    Scanner sc(src);
    std::vector<Token> tokens;
    Token tok = sc.NextToken();
    while (tok.m_type != TokenType::Eof) {
        tokens.push_back(sc.CurToken());
        tok = sc.NextToken();
    }

    EXPECT_EQ(tokens.size(), 4);

    EXPECT_EQ(tokens[0].m_type, TokenType::VAR);
    EXPECT_EQ(tokens[0].m_val, "a");

    EXPECT_EQ(tokens[1].m_type, TokenType::OPERATOR);
    EXPECT_EQ(tokens[1].m_val, "=");

    EXPECT_EQ(tokens[2].m_type, TokenType::NUMBER);
    EXPECT_EQ(tokens[2].m_val, "10.36");

    EXPECT_EQ(tokens[3].m_type, TokenType::SEMICOLON);
    EXPECT_EQ(tokens[3].m_val, ";");
}

TEST(Scanner, parenthese) {
    const std::string src("(10+2);");

    Scanner sc(src);
    std::vector<Token> tokens;
    Token tok = sc.NextToken();
    while (tok.m_type != TokenType::Eof) {
        tokens.push_back(sc.CurToken());
        tok = sc.NextToken();
    }

    EXPECT_EQ(tokens.size(), 6);

    EXPECT_EQ(tokens[0].m_type, TokenType::LEFT_PARENT);
    EXPECT_EQ(tokens[0].m_val, "(");

    EXPECT_EQ(tokens[4].m_type, TokenType::RIGHT_PARENT);
    EXPECT_EQ(tokens[4].m_val, ")");
}

TEST(Scanner, keyword) {
    const std::string src("def foo extern;");

    Scanner sc(src);
    std::vector<Token> tokens;
    Token tok = sc.NextToken();
    while (tok.m_type != TokenType::Eof) {
        tokens.push_back(sc.CurToken());
        tok = sc.NextToken();
    }

    EXPECT_EQ(tokens.size(), 4);

    EXPECT_EQ(tokens[0].m_type, TokenType::DEF);

    EXPECT_EQ(tokens[2].m_type, TokenType::EXTERN);
    EXPECT_EQ(tokens[3].m_val, ";");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}