#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <memory>
#include "types.h"
#include "formula.h"

namespace ctl {

// Forward declarations
class CTLAutomaton;
class SCCBlocks;
class SMTInterface;

/**
 * SymbolicTransition: Represents a raw transition in symbolic form
 * without expanding it into explicit Move objects.
 * 
 * Contains:
 * - guard: the propositional constraint that must be satisfied
 * - disjuncts: OR of conjunctions, each conjunction is a list of obligations
 * 
 * Each obligation is (dir, state) where:
 * - dir = -1 means "evaluate state's formula symbolically"
 * - dir >= 0 means "temporal child at direction dir must satisfy state"
 */
struct SymbolicTransition {
    std::string guard;
    std::vector<Conj> disjuncts;
};

/**
 * On-the-fly symbolic evaluator for CTL automaton emptiness checking.
 * 
 * Instead of expanding transitions via expandMoveRevisited, this evaluator
 * interprets transitions symbolically during fixpoint computation.
 */
class SymbolicEvaluator {
public:
    SymbolicEvaluator(
        const CTLAutomaton* automaton,
        const std::vector<CTLStatePtr>& states,
        const std::unordered_map<std::string_view, std::vector<CTLTransitionPtr>>& transitions,
        const std::unordered_map<std::string_view, BinaryOperator>& state_operators,
        const SCCBlocks* blocks,
        const SMTInterface* smt_interface
    );

    // Main evaluation entry point
    bool evaluateState(
        std::string_view state_name,
        int current_block,
        const std::unordered_set<std::string_view>& in_block_good,
        const std::vector<std::unordered_set<std::string_view>>& good_states,
        bool block_is_universal
    ) const;

private:
    const CTLAutomaton* automaton_;
    const std::vector<CTLStatePtr>& states_;
    const std::unordered_map<std::string_view, std::vector<CTLTransitionPtr>>& transitions_;
    const std::unordered_map<std::string_view, BinaryOperator>& state_operators_;
    const SCCBlocks* blocks_;
    const SMTInterface* smt_interface_;
    
    // Cache for state formulas
    mutable std::unordered_map<std::string_view, CTLFormulaPtr> state_formula_cache_;

    // Get formula for a state
    const CTLFormula* getStateFormula(
        std::string_view state_name
    ) const;

    // Evaluate a single disjunct (conjunction of obligations)
    bool evaluateConjunct(
        const Conj& conjunct,
        const std::unordered_set<std::string>& guard_atoms,
        int current_block,
        const std::unordered_set<std::string_view>& in_block_good,
        const std::vector<std::unordered_set<std::string_view>>& good_states,
        bool block_is_universal
    ) const;

    // Check if an obligation is satisfied
    bool isObligationSatisfied(
        const Atom& obligation,
        int current_block,
        const std::unordered_set<std::string_view>& in_block_good,
        const std::vector<std::unordered_set<std::string_view>>& good_states
    ) const;



    // Check if a set of atoms is satisfiable
    bool isSatisfiable(
        const std::unordered_set<std::string>& atoms
    ) const;
};

} // namespace ctl
