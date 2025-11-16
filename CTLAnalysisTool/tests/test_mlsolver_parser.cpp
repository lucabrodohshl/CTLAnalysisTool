#include <gtest/gtest.h>
#include "sat_parsers/mlsolver_parser.h"
#include "parser.h"

using namespace ctl;

// Test atomic formula
TEST(MLSolverParserTest, AtomicFormula) {
    MLSolverParser::clearComparisonMapping();
    auto formula = Parser::parseFormula("p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "p");
}

// Test comparison collapses to unique atoms
TEST(MLSolverParserTest, ComparisonToAtom) {
    MLSolverParser::clearComparisonMapping();  // Start fresh
    
    auto formula1 = Parser::parseFormula("x <= 5");
    std::string result1 = MLSolverParser::toMLSolverFormat(*formula1);
    EXPECT_EQ(result1, "p_1");  // First comparison gets p_1
    
    auto formula2 = Parser::parseFormula("y > 3");
    std::string result2 = MLSolverParser::toMLSolverFormat(*formula2);
    EXPECT_EQ(result2, "p_2");  // Second comparison gets p_2
    
    // Same comparison should reuse the same atom
    auto formula3 = Parser::parseFormula("x <= 5");
    std::string result3 = MLSolverParser::toMLSolverFormat(*formula3);
    EXPECT_EQ(result3, "p_1");  // Same as first
    
    // Check the mapping
    auto mapping = MLSolverParser::getComparisonMapping();
    EXPECT_EQ(mapping.size(), 2);
    EXPECT_EQ(mapping.at("x <= 5"), "p_1");
    EXPECT_EQ(mapping.at("y > 3"), "p_2");
}

// Test boolean literals
TEST(MLSolverParserTest, BooleanTrue) {
    auto formula = Parser::parseFormula("true");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "tt");
}

TEST(MLSolverParserTest, BooleanFalse) {
    auto formula = Parser::parseFormula("false");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "ff");
}

// Test negation
TEST(MLSolverParserTest, NegationSimple) {
    auto formula = Parser::parseFormula("!p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "!p");
}

TEST(MLSolverParserTest, NegationComplex) {
    auto formula = Parser::parseFormula("!(p & q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "!((p & q))");
}

// Test conjunction (AND)
TEST(MLSolverParserTest, Conjunction) {
    auto formula = Parser::parseFormula("p & q");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "(p & q)");
}

// Test disjunction (OR)
TEST(MLSolverParserTest, Disjunction) {
    auto formula = Parser::parseFormula("p | q");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "(p | q)");
}

// Test implication
TEST(MLSolverParserTest, Implication) {
    auto formula = Parser::parseFormula("p -> q");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "(p ==> q)");
}

// Test EX (exists next)
TEST(MLSolverParserTest, EX) {
    auto formula = Parser::parseFormula("EX p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "E X p");
}

// Test AX (always next)
TEST(MLSolverParserTest, AX) {
    auto formula = Parser::parseFormula("AX p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A X p");
}

// Test EF (exists finally)
TEST(MLSolverParserTest, EF) {
    auto formula = Parser::parseFormula("EF p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "E F p");
}

// Test AF (always finally)
TEST(MLSolverParserTest, AF) {
    auto formula = Parser::parseFormula("AF p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A F p");
}

// Test EG (exists globally)
TEST(MLSolverParserTest, EG) {
    auto formula = Parser::parseFormula("EG p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "E G p");
}

// Test AG (always globally)
TEST(MLSolverParserTest, AG) {
    auto formula = Parser::parseFormula("AG p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A G p");
}

// Test EU (exists until)
TEST(MLSolverParserTest, EU) {
    auto formula = Parser::parseFormula("E(p U q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "E (p U q)");
}

// Test AU (always until)
TEST(MLSolverParserTest, AU) {
    auto formula = Parser::parseFormula("A(p U q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A (p U q)");
}

// Test EW (exists weak until) - should rewrite as E[φ U ψ] | EG φ
TEST(MLSolverParserTest, EW) {
    auto formula = Parser::parseFormula("E(p W q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "(E (p U q) | E G p)");
}

// Test AW (always weak until) - should rewrite as A[φ U ψ] | AG φ
TEST(MLSolverParserTest, AW) {
    auto formula = Parser::parseFormula("A(p W q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "(A (p U q) | A G p)");
}

// Test complex nested formula
TEST(MLSolverParserTest, ComplexFormula) {
    auto formula = Parser::parseFormula("AG (p -> EF q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A G (p ==> E F q)");
}

// Test nested boolean operators
TEST(MLSolverParserTest, NestedBoolean) {
    auto formula = Parser::parseFormula("AG ((p & q) | r)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A G ((p & q) | r)");
}

// Test using convertString
TEST(MLSolverParserTest, ConvertString) {
    std::string result = MLSolverParser::convertString("AG (p -> EF q)");
    EXPECT_TRUE(result.find("A G") != std::string::npos);
    EXPECT_TRUE(result.find("==>") != std::string::npos);
    EXPECT_TRUE(result.find("E F") != std::string::npos);
}

// Test the problematic case: A(false W (EG (!(P1 <= P3))))
TEST(MLSolverParserTest, FalseWeakUntil) {
    MLSolverParser::clearComparisonMapping();
    auto formula = Parser::parseFormula("A(false W EG(!P1))");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    // Should rewrite as: (A(false U EG(!P1)) | AG(false))
    // With false -> ff: (A (ff U E G !P1) | A G ff)
    EXPECT_TRUE(result.find("ff") != std::string::npos);
    EXPECT_TRUE(result.find("A G") != std::string::npos);
    EXPECT_TRUE(result.find("E G") != std::string::npos);
    EXPECT_TRUE(result.find("|") != std::string::npos);
}

// Test formula with multiple atomic propositions
TEST(MLSolverParserTest, MultipleAtomicProps) {
    MLSolverParser::clearComparisonMapping();
    auto formula = Parser::parseFormula("AG (a & b & c)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_TRUE(result.find("a") != std::string::npos);
    EXPECT_TRUE(result.find("b") != std::string::npos);
    EXPECT_TRUE(result.find("c") != std::string::npos);
}

// Test that long atom names get mapped
TEST(MLSolverParserTest, LongAtomNames) {
    MLSolverParser::clearComparisonMapping();
    auto formula = Parser::parseFormula("AG longname");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A G p_1");  // Long names get mapped to p_1, p_2, etc.
}

// Test combination of temporal operators
TEST(MLSolverParserTest, CombinedTemporalOperators) {
    auto formula = Parser::parseFormula("AG (EF p -> AF q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_TRUE(result.find("A G") != std::string::npos);
    EXPECT_TRUE(result.find("E F") != std::string::npos);
    EXPECT_TRUE(result.find("A F") != std::string::npos);
    EXPECT_TRUE(result.find("==>") != std::string::npos);
}

// Test double negation
TEST(MLSolverParserTest, DoubleNegation) {
    auto formula = Parser::parseFormula("!!p");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "!!p");
}

// Test formula that should work with MLSolver
TEST(MLSolverParserTest, ValidMLSolverFormula) {
    MLSolverParser::clearComparisonMapping();
    auto formula = Parser::parseFormula("A(p U q)");
    std::string result = MLSolverParser::toMLSolverFormat(*formula);
    EXPECT_EQ(result, "A (p U q)");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
