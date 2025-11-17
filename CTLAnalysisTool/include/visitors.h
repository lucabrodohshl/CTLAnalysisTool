// Visitor pattern for traversing formula trees
#pragma once
#include "formula.h"
namespace ctl {


class CTLFormulaVisitor {
public:
    virtual ~CTLFormulaVisitor() = default;
    virtual void visit(const AtomicFormula& formula) = 0;
    virtual void visit(const ComparisonFormula& formula) = 0;
    virtual void visit(const BooleanLiteral& formula) = 0;
    virtual void visit(const NegationFormula& formula) = 0;
    virtual void visit(const BinaryFormula& formula) = 0;
    virtual void visit(const TemporalFormula& formula) = 0;
};



// Visitor implementations for common operations
class BinaryToAtomVisitor : public CTLFormulaVisitor {
private:
    CTLFormulaPtr result_;
    
public:
    void visit(const AtomicFormula& formula) override;
    void visit(const ComparisonFormula& formula) override;
    void visit(const BooleanLiteral& formula) override;
    void visit(const NegationFormula& formula) override;
    void visit(const BinaryFormula& formula) override;
    void visit(const TemporalFormula& formula) override;
    
    static CTLFormulaPtr convert(const CTLFormula& formula);
    CTLFormulaPtr getResult() const { return result_; }
};

// Visitor implementations for common operations
class AtomCollectorVisitor : public CTLFormulaVisitor {
private:
    std::unordered_set<std::string> atoms_;
    
public:
    void visit(const AtomicFormula& formula) override;
    void visit(const ComparisonFormula& formula) override;
    void visit(const BooleanLiteral& formula) override;
    void visit(const NegationFormula& formula) override;
    void visit(const BinaryFormula& formula) override;
    void visit(const TemporalFormula& formula) override;
    
    const std::unordered_set<std::string>& getAtoms() const { return atoms_; }
};

class NNFConverterVisitor : public CTLFormulaVisitor {
private:
    CTLFormulaPtr result_;
    bool negate_;
    
    void setNegate(bool neg) { negate_ = neg; }
    
public:
    explicit NNFConverterVisitor(bool negate = false) : negate_(negate) {}
    
    void visit(const AtomicFormula& formula) override;
    void visit(const ComparisonFormula& formula) override;
    void visit(const BooleanLiteral& formula) override;
    void visit(const NegationFormula& formula) override;
    void visit(const BinaryFormula& formula) override;
    void visit(const TemporalFormula& formula) override;
    
    CTLFormulaPtr getResult() const { return result_; }
    
    static CTLFormulaPtr convert(const CTLFormula& formula, bool negate = false);
};


/**
 * @brief Visitor that converts a CTL formula in NNF into Existential Normal Form (ENF).
 *
 * ENF ensures that only existential path quantifiers (E) appear.
 * All universal operators (A) are rewritten using standard dualities:
 *
 *   AX φ  ≡  ¬EX ¬φ
 *   AF φ  ≡  E[true U φ]
 *   AG φ  ≡  ¬E[true U ¬φ]
 *   A[φ U ψ] ≡ ¬E[(¬ψ) U (¬φ ∧ ¬ψ)] ∧ ¬EG(¬ψ)
 *   A[φ W ψ] ≡ ¬E[(¬ψ) U (¬φ ∧ ¬ψ)]
 *
 * After this transformation, the resulting formula will only use:
 *   EX, EG, EU, Boolean operators, and atomic formulas.
 */
class ENFConverterVisitor : public CTLFormulaVisitor {
private:
    CTLFormulaPtr result_;

    static CTLFormulaPtr neg(const CTLFormulaPtr& f);
    static CTLFormulaPtr t_true();
    static CTLFormulaPtr t_false();

public:
    ENFConverterVisitor() = default;

    void visit(const AtomicFormula& f) override;
    void visit(const ComparisonFormula& f) override;
    void visit(const BooleanLiteral& f) override;
    void visit(const NegationFormula& f) override;
    void visit(const BinaryFormula& f) override;
    void visit(const TemporalFormula& f) override;

    CTLFormulaPtr getResult() const { return result_; }

    static CTLFormulaPtr convert(const CTLFormula& formula);
};


/**
 * @brief Visitor that wraps purely propositional formulas into E(false U φ),
 *        unless they are inside a temporal operator.
 *
 * Examples:
 *   p           -> E(false U p)
 *   (p ∧ q)     -> E(false U (p ∧ q))
 *   (q ∧ AG(p)) -> (E(false U q)) ∧ AG(p)
 *   AG(p ∨ q)   -> AG(p ∨ q)   // unchanged, because inside temporal
 */
class WrapPropositionalVisitor : public CTLFormulaVisitor {
private:
    CTLFormulaPtr result_;
    bool inside_temporal_;  // true when currently visiting inside a temporal operator

public:
    explicit WrapPropositionalVisitor(bool inside_temporal = false);

    CTLFormulaPtr getResult() const { return result_; }

    static CTLFormulaPtr convert(const CTLFormula& formula);

    // --- Visitor method overrides ---
    void visit(const AtomicFormula& f) override;
    void visit(const ComparisonFormula& f) override;
    void visit(const BooleanLiteral& f) override;
    void visit(const NegationFormula& f) override;
    void visit(const BinaryFormula& f) override;
    void visit(const TemporalFormula& f) override;
};

/**
 * @brief Visitor that converts ComparisonFormula nodes to AtomicFormula nodes with simple names.
 *
 * This is useful for compatibility with solvers like mlsolver that don't support
 * comparison operators directly. Each comparison (e.g., "x <= 5") is converted
 * to a simple atomic proposition like "p0", "p1", etc.
 *
 * Example:
 *   ComparisonFormula("x", "<=", "5") -> AtomicFormula("p0")
 *   ComparisonFormula("y", ">", "10") -> AtomicFormula("p1")
 *
 * The mapping is stored and can be retrieved for later reference.
 */
class ComparisonRemoverVisitor : public CTLFormulaVisitor {
private:
    CTLFormulaPtr result_;
    std::unordered_map<std::string, std::string> comparison_map_; // original -> simple name
    std::unordered_map<std::string, std::string> reverse_map_;     // simple name -> original
    int counter_;

    std::string getSimpleName(const std::string& comparison);

public:
    ComparisonRemoverVisitor() : counter_(0) {}

    void visit(const AtomicFormula& f) override;
    void visit(const ComparisonFormula& f) override;
    void visit(const BooleanLiteral& f) override;
    void visit(const NegationFormula& f) override;
    void visit(const BinaryFormula& f) override;
    void visit(const TemporalFormula& f) override;

    CTLFormulaPtr getResult() const { return result_; }
    const std::unordered_map<std::string, std::string>& getComparisonMap() const { return comparison_map_; }
    const std::unordered_map<std::string, std::string>& getReverseMap() const { return reverse_map_; }

    static CTLFormulaPtr convert(const CTLFormula& formula);
    static CTLFormulaPtr convert(const CTLFormula& formula, 
                                  std::unordered_map<std::string, std::string>& out_comparison_map,
                                  std::unordered_map<std::string, std::string>& out_reverse_map);
};

}
