#ifndef MLSOLVER_PARSER_H
#define MLSOLVER_PARSER_H

#include <string>
#include <memory>
#include <unordered_map>
#include "formula.h"

namespace ctl {

/**
 * @brief Converts CTL formulas to MLSolver format
 * 
 * This class provides functionality to convert CTL formulas from the internal
 * representation to the format expected by the MLSolver tool:
 * https://github.com/tcsprojects/mlsolver
 * 
 * The MLSolver format uses:
 * - '|' for disjunction (OR)
 * - '&' for conjunction (AND)
 * - '!' for negation (NOT)
 * - '==>' for implication
 * - Standard CTL operators: E F, A F, E G, A G, E U, A U
 * - 'tt' for true, 'ff' for false
 * 
 * Important: MLSolver requires proper CTL nesting - temporal operators
 * must be preceded by path quantifiers, and boolean operators cannot
 * combine path formulas at the top level.
 * 
 * Comparisons are mapped to unique atomic propositions (p_1, p_2, etc.)
 */
class MLSolverParser {
public:
    /**
     * @brief Convert a CTL formula to MLSolver format string
     * @param formula The CTL formula to convert
     * @return String in MLSolver format
     */
    static std::string toMLSolverFormat(const CTLFormula& formula);
    
    /**
     * @brief Convert a CTL formula string to MLSolver format
     * 
     * This method parses a CTL formula string in standard notation and
     * converts it to MLSolver format.
     * 
     * @param formula_str The CTL formula string (e.g., "AG(p & q)")
     * @return String in MLSolver format (e.g., "A G (p & q)")
     */
    static std::string convertString(const std::string& formula_str);
    
    /**
     * @brief Get the mapping from comparison strings to atomic propositions
     * @return Map of comparison expressions to their assigned atoms
     */
    static const std::unordered_map<std::string, std::string>& getComparisonMapping();
    
    /**
     * @brief Clear the comparison mapping (useful for starting fresh conversions)
     */
    static void clearComparisonMapping();

private:
    /**
     * @brief Internal recursive conversion function
     * @param formula The formula to convert
     * @return String representation in MLSolver format
     */
    static std::string convertFormula(const CTLFormula& formula);
    
    /**
     * @brief Get or create an atom for a comparison expression
     * @param comparison The comparison expression as a string
     * @return The atomic proposition assigned to this comparison (e.g., "p_1")
     */
    static std::string getComparisonAtom(const std::string& comparison);
    
    /**
     * @brief Get or create an atom identifier for an atomic proposition
     * @param atom The atomic proposition as a string
     * @return The identifier assigned to this atom (e.g., "p_1", "p_2")
     */
    static std::string getAtomIdentifier(const std::string& atom);
    
    // Static members to track mappings
    static std::unordered_map<std::string, std::string> comparison_map_;
    static std::unordered_map<std::string, std::string> atom_map_;
    static int next_atom_id_;
};

} // namespace ctl

#endif // MLSOLVER_PARSER_H
