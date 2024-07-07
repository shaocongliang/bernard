#include <iostream>
#include <Parser.h>
#include <gtest/gtest.h>

TEST(ast, case1) {
    Scanner scan("2 * 3 * 20");
    ExprTree *ast = BuildExprTree(scan);
}

TEST(ast, case2) {
    Scanner scan("2 + 3 * 4 + 5 * 6");
    ExprTree *ast = BuildExprTree(scan);
    EXPECT_EQ(Calc(ast), 44);
}

TEST(ast, case3) {
    Scanner scan("2 + 4 / 4 + 5 * 6");
    ExprTree *ast = BuildExprTree(scan);
    EXPECT_EQ(Calc(ast), 33);
}

TEST(ast, case4) {
    Scanner scan("7 + 3 * 4");
    ExprTree *ast = BuildExprTree(scan);
    EXPECT_EQ(Calc(ast), 19);
}

TEST(ast, case5) {
    Scanner scan("2 + 3");
    ExprTree *ast = BuildExprTree(scan);
    EXPECT_EQ(Calc(ast), 5);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}