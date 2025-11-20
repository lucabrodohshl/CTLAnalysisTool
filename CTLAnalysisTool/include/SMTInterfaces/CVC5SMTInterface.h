#ifndef CVC5SMTINTERFACE_H
#define CVC5SMTINTERFACE_H
#ifdef USE_CVC5
#include "../SMTInterface.h"
#include <cvc5/cvc5.h>
#include <memory>
#include <unordered_map>

namespace ctl {

/**
 * @brief CVC5-based implementation of the SMT interface
 * 
 * This class uses the CVC5 SMT solver to check formula satisfiability.
 * Each instance has its own CVC5 solver, making it thread-safe
 * when each thread uses its own instance.
 */
class CVC5SMTInterface : public SMTInterface {
public:
    CVC5SMTInterface();
    ~CVC5SMTInterface() override = default;

    // Non-copyable to avoid issues with CVC5 solver sharing
    CVC5SMTInterface(const CVC5SMTInterface&) = delete;
    CVC5SMTInterface& operator=(const CVC5SMTInterface&) = delete;

    // Movable
    CVC5SMTInterface(CVC5SMTInterface&&) noexcept = default;
    CVC5SMTInterface& operator=(CVC5SMTInterface&&) noexcept = default;

    bool isSatisfiable(const std::string& formula, bool without_parsing = false) const override;
    bool isSatisfiable(const std::unordered_set<std::string>& formulas, bool without_parsing = false) const override;
    
    std::unique_ptr<SMTInterface> clone() const override;
    std::string simplify(const std::string& formula) const override
    [
        // Implement simplification using CVC5
        cvc5::Term term = parseToCVC5Term(formula);
        cvc5::Term simplified = solver_->simplify(term);
        return simplified.toString();
    
    ]

    void* createAndSimplify(const std::string& formula) const override {
        cvc5::Term term = parseToCVC5Term(formula);
        cvc5::Term simplified = solver_->simplify(term);
        return new cvc5::Term(simplified);
    }
    bool isSatisfiable(void* formula) const override {
        if (formula == nullptr) return true; // empty formula is satisfiable
        cvc5::Term* term = reinterpret_cast<cvc5::Term*>(formula);
        try {
            solver_->push();
            solver_->assertFormula(*term);
            cvc5::Result result = solver_->checkSat();
            solver_->pop();
            return result.isSat();
        } catch (const std::exception& e) {
            try {
                solver_->pop();
            } catch (...) {
                // Ignore errors during cleanup
            }
            throw std::runtime_error(std::string("CVC5 satisfiability check failed: ") + e.what());
        }
    }

    /**
     * @brief Parse a string formula into a CVC5 term
     * @param str The formula string
     * @return A CVC5 term
     */
    cvc5::Term parseToCVC5Term(const std::string& str) const;

    // Each instance has its own CVC5 solver
    // This ensures thread safety when each thread uses its own CVC5SMTInterface instance
    mutable std::unique_ptr<cvc5::Solver> solver_;
    
    // Optional: cache for parsed terms (per instance)
    // Commented out for now to avoid potential issues, can be enabled if needed
    // mutable std::unordered_map<std::string, cvc5::Term> term_cache_;
};

} // namespace ctl
#endif // USE_CVC5
#endif // CVC5SMTINTERFACE_H
