#include "sat_parsers/mlsolver_parser.h"
#include "parser.h"
#include <sstream>

namespace ctl {

// Initialize static members
std::unordered_map<std::string, std::string> MLSolverParser::comparison_map_;
std::unordered_map<std::string, std::string> MLSolverParser::atom_map_;
int MLSolverParser::next_atom_id_ = 1;

std::string MLSolverParser::toMLSolverFormat(const CTLFormula& formula) {
    return convertFormula(formula);
}

std::string MLSolverParser::convertString(const std::string& formula_str) {
    // Parse the formula string first using the static convenience method
    auto formula = Parser::parseFormula(formula_str);
    
    // Convert to MLSolver format
    return convertFormula(*formula);
}

const std::unordered_map<std::string, std::string>& MLSolverParser::getComparisonMapping() {
    return comparison_map_;
}

void MLSolverParser::clearComparisonMapping() {
    comparison_map_.clear();
    atom_map_.clear();
    next_atom_id_ = 1;
}

std::string MLSolverParser::getComparisonAtom(const std::string& comparison) {
    // Check if we already have a mapping for this comparison
    auto it = comparison_map_.find(comparison);
    if (it != comparison_map_.end()) {
        return it->second;
    }
    
    // Create a new atom for this comparison
    // Format: p_1, p_2, p_3, etc.
    std::string atom = "p_" + std::to_string(next_atom_id_);
    next_atom_id_++;
    comparison_map_[comparison] = atom;
    return atom;
}

std::string MLSolverParser::getAtomIdentifier(const std::string& atom) {
    // Check if we already have a mapping for this atom
    auto it = atom_map_.find(atom);
    if (it != atom_map_.end()) {
        return it->second;
    }
    
    // Create a new identifier for this atom
    // Format: p_1, p_2, p_3, etc.
    std::string identifier = "p_" + std::to_string(next_atom_id_);
    next_atom_id_++;
    atom_map_[atom] = identifier;
    return identifier;
}

std::string MLSolverParser::convertFormula(const CTLFormula& formula) {
    // Handle comparison formulas - map to unique atoms
    if (auto comp = dynamic_cast<const ComparisonFormula*>(&formula)) {
        std::string comparison_str = comp->toString();
        return getComparisonAtom(comparison_str);
    }
    
    // Handle boolean literals (true/false)
    if (auto boolean = dynamic_cast<const BooleanLiteral*>(&formula)) {
        return boolean->value ? "tt" : "ff";
    }
    
    // Handle atomic formulas
    if (auto atomic = dynamic_cast<const AtomicFormula*>(&formula)) {
        if (atomic->proposition == "false") {
            return "ff";
        }
        if (atomic->proposition == "true") {
            return "tt";
        }
        // Map atomic proposition to an identifier (keep simple names if possible)
        // For single letter atoms, keep them as is
        if (atomic->proposition.length() == 1) {
            return atomic->proposition;
        }
        // For longer names, use the mapping
        return getAtomIdentifier(atomic->proposition);
    }
    
    // Handle negation
    if (auto neg = dynamic_cast<const NegationFormula*>(&formula)) {
        std::string operand_str = convertFormula(*neg->operand);
        // Add parentheses for complex subformulas
        if (operand_str.find(' ') != std::string::npos || 
            operand_str.find('(') != std::string::npos) {
            return "! (" + operand_str + ")";
        }
        return "! " + operand_str;
    }
    
    // Handle binary formulas (AND, OR, IMPLIES)
    if (auto binary = dynamic_cast<const BinaryFormula*>(&formula)) {
        std::string left_str = convertFormula(*binary->left);
        std::string right_str = convertFormula(*binary->right);
        
        switch (binary->operator_) {
            case BinaryOperator::AND:
                return "(" + left_str + " & " + right_str + ")";
            case BinaryOperator::OR:
                return "(" + left_str + " | " + right_str + ")";
            case BinaryOperator::IMPLIES:
                return "(" + left_str + " ==> " + right_str + ")";
        }
    }
    
    // Handle temporal formulas
    if (auto temporal = dynamic_cast<const TemporalFormula*>(&formula)) {
        std::string operand_str = convertFormula(*temporal->operand);
        
        switch (temporal->operator_) {
            case TemporalOperator::EX:
                return "E X " + operand_str;
            case TemporalOperator::AX:
                return "A X " + operand_str;
            case TemporalOperator::EF:
                return "E F " + operand_str;
            case TemporalOperator::AF:
                return "A F " + operand_str;
            case TemporalOperator::EG:
                return "E G " + operand_str;
            case TemporalOperator::AG:
                return "A G " + operand_str;
            case TemporalOperator::EU: {
                std::string second_str = convertFormula(*temporal->second_operand);
                // Add parentheses around operands to ensure proper parsing
                return "E ((" + operand_str + ") U (" + second_str + "))";
            }
            case TemporalOperator::AU: {
                std::string second_str = convertFormula(*temporal->second_operand);
                // Add parentheses around operands to ensure proper parsing
                return "A ((" + operand_str + ") U (" + second_str + "))";
            }
            case TemporalOperator::EW: {
                // E[φ W ψ] = ¬A[(¬ψ) U (¬φ ∧ ¬ψ)]
                // This is semantically equivalent and valid CTL
                std::string second_str = convertFormula(*temporal->second_operand);
                return "! (A ((! (" + second_str + ")) U ((! (" + operand_str + ")) & (! (" + second_str + ")))))";
            }
            case TemporalOperator::AW: {
                // A[φ W ψ] = ¬E[(¬ψ) U (¬φ ∧ ¬ψ)]
                // This is semantically equivalent and valid CTL
                std::string second_str = convertFormula(*temporal->second_operand);
                return "! (E ((! (" + second_str + ")) U ((! (" + operand_str + ")) & (! (" + second_str + ")))))";
            }
        }
    }
    
    // Default case: return formula string representation
    return formula.toString();
}

} // namespace ctl
