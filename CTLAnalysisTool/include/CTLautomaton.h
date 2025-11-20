#ifndef CTLAUTOMATON_H
#define CTLAUTOMATON_H

#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <iostream>
#include <memory>
#include <cassert>
#include <algorithm>
#include <functional>
#include "visitors.h"
#include "formula.h"
#include "formula_utils.h"
#include "transition_expressions.h"
#include "types.h"
#include "SCCBlocks.h"
#include "SMTInterface.h"
#include "game_graph.h"

#include "transitions.h"

namespace ctl {





class CTLAutomaton {
public:
    CTLAutomaton() = default;
    explicit CTLAutomaton(const CTLFormula& formula, bool verbose = false);
    
    CTLAutomaton(const CTLAutomaton& other);
    CTLAutomaton& operator=(const CTLAutomaton& other);
    
    CTLAutomaton(CTLAutomaton&& other) noexcept;
    CTLAutomaton& operator=(CTLAutomaton&& other) noexcept;


    // Build automaton from formula
    void buildFromFormula(const CTLFormula& formula, bool symbolic = false);


    bool languageIncludes(const CTLAutomaton& other) const;
    bool languageIncludesOF(const CTLAutomaton& other) const;
    bool simulates(const CTLAutomaton& other) const;
    bool isSimulatedBy(const CTLAutomaton& other) const;

    void print() const;
    std::string toString() const;
    bool isEmpty() const;
    bool isState(std::string_view state_name) const {
        return v_states_.end() != std::find_if(v_states_.begin(), v_states_.end(),
                                       [state_name](const CTLStatePtr& s) {
                                           return s->name == state_name;
                                       });
    }


    void addEdge(const std::string_view from, const std::string_view to);
    void addTransition(const std::string_view from,
                             const std::string& guard,
                             const std::vector<Clause>& clauses,
                             bool is_dnf = true);

    std::string_view getStateOfFormula(const CTLFormula& f) const;
    bool isAccepting(const std::string_view state_name) const;
    
    std::string getFormulaString() const;
    CTLFormulaPtr getFormula() const;
    CTLFormulaPtr getNegatedFormula() const;

    std::unordered_map<std::string_view, std::vector<Move>> getExpandedTransitions() const {throw std::runtime_error("getExpandedTransitions() not implemented yet"); }

    std::string getRawFormula() const { return s_raw_formula_; }
    std::string getRawNegation() const { return "!(" + s_raw_formula_ + ")"; }
    std::string_view getInitialState() const { return initial_state_; }

    CTLAutomatonPtr clone() const {
        return std::make_unique<CTLAutomaton>(*this);
    }

    CTLAutomatonPtr getComplement() const {
        return std::make_unique<CTLAutomaton>(*getNegatedFormula());
    }

    
    // Public wrapper for satisfiability checking (for OTF product construction)
    bool isSatisfiable(const std::unordered_set<std::string>& g, bool without_parsing = false) const { return __isSatisfiable(g, without_parsing); }

    bool verbose() const { return verbose_; }
    void setVerbose(bool v) { verbose_ = v; }

    // Build a symbolic parity game from the automaton
    SymbolicParityGame buildGameGraph() const;

    bool checkCtlSatisfiability() const;
    
private:
    CTLFormulaPtr p_original_formula_;
    CTLFormulaPtr p_negated_formula_;
    std::string s_raw_formula_;
    std::vector<CTLStatePtr> v_states_;
    std::vector<CTLStatePtr> v_removed_states_;
    std::string_view initial_state_;
    std::unordered_map<std::string_view, std::unordered_set<std::string_view>> state_successors_;
    std::unordered_map<std::string_view, std::vector<CTLTransitionPtr>> m_transitions_;
    std::unordered_map<std::string_view, BinaryOperator> m_state_operator_;
    bool verbose_ = false;
    mutable std::unordered_map<std::string_view, std::vector<Move>> m_expanded_transitions_;
    mutable std::unique_ptr<SCCBlocks> blocks_;
    mutable std::unordered_set<std::string_view> s_accepting_states_;
    mutable std::unordered_map<size_t, std::string_view> formula_hash_to_state_cache_;
    mutable std::vector<std::unordered_set<int>> block_edges_;
    mutable std::vector<int> topological_order_;
    
    // SMT interface for satisfiability checking
    mutable std::unique_ptr<SMTInterface> smt_interface_;
    
    // handy shorthands for "no letter constraint" and directions
    std::string GTrue = "true";
    std::string GFalse = "false";
    int L = 0;
    int R = 1; // we only need identities; values are irrelevant to emptiness
    
    // Pre-computed hash values for common formulas for fast comparison
    static const size_t TRUE_HASH;
    static const size_t FALSE_HASH;

    
    
    



private:
      void __buildFromFormula( bool symbolic);
      void __decideBlockTypes() const;
      
      std::string __handleProp (const std::string& proposition, bool symbolic);
      void __handleStatesAndTransitions(bool symbolic);
      std::vector<std::vector<std::string_view>> __computeSCCs() const;
      bool __isSatisfiable (const std::string& g, bool without_parsing = false) const;
      bool __isSatisfiable(const std::unordered_set<std::string>& g, bool without_parsing = false) const ;
      bool __isSatisfiable(const Guard& g) const ;
      void __handleORTransition(const std::shared_ptr<BinaryFormula>& bin, const CTLStatePtr& state);
      void __handleAXTransition(const std::shared_ptr<TemporalFormula>& bin, const CTLStatePtr& state);
      inline void __addFalseTransition(std::string_view s) { addTransition(s, GFalse, { Clause{ {} } }); };
      inline void __addTrueTransition(std::string_view s) { addTransition(s, GTrue, { Clause{ {} } }); };
      inline void __addTrueTransition(std::string_view s, const std::vector<Clause>& clauses) {
          addTransition(s, GTrue, clauses);
      };
      Guard createGuardFromString(const std::string& guard) const;
      void __clean();
  
        // Symbolic parity game solver functions
        WinningRegion symPre(const SymbolicParityGame& game, const WinningRegion& target_region) const;
        Guard evaluateRule(const SymbolicGameEdge& edge, const WinningRegion& target_region) const;
        Guard evaluateClause(const Clause& clause, const WinningRegion& target_region) const;
        WinningRegion symbolicAttractor(const SymbolicParityGame& game, const WinningRegion& target, Player sigma) const;
        WinningRegion symbolicTrap(const SymbolicParityGame& game, const WinningRegion& domain, Player sigma) const;
        std::pair<WinningRegion, WinningRegion> symbolicSolveRecursive(const SymbolicParityGame& game) const;
        
        
        // Helper functions for formula manipulation
        Guard makeOr(const Guard& left, const Guard& right) const;
        Guard makeAnd(const Guard& left, const Guard& right) const;
        Guard makeTrue() const;
        Guard makeFalse() const;
        bool isEquivalentToTrue(const Guard& g) const;
        bool isEquivalentToFalse(const Guard& g) const;
        bool winningRegionsEqual(const WinningRegion& a, const WinningRegion& b) const;

        bool __isSatisfiableUnion(const std::unordered_set<std::string>& base, const std::unordered_set<std::string>& add) const;
        void __appendGuard(std::unordered_set<std::string>& base, const std::unordered_set<std::string>& add) const;
        
        // On-the-fly language inclusion helper functions
        bool __onTheFlyProductEmptiness(const CTLAutomaton& automaton_a, const CTLAutomaton& automaton_b) const;
        bool __isProductStateReachable(const std::pair<std::string_view, std::string_view>& product_state,
                                     const CTLAutomaton& automaton_a, const CTLAutomaton& automaton_b) const;
        bool __areMovesCompatible(const Move& move_a, const Move& move_b) const;
        
        // Product state hash function
        struct ProductStateHash {
            size_t operator()(const std::pair<std::string_view, std::string_view>& p) const {
                return std::hash<std::string_view>{}(p.first) ^ 
                       (std::hash<std::string_view>{}(p.second) << 1);
            }
        };
        
        void __computeReachableProductStates(
            const std::pair<std::string_view, std::string_view>& initial_state,
            const CTLAutomaton& automaton_a, const CTLAutomaton& automaton_b,
            std::unordered_set<std::pair<std::string_view, std::string_view>, ProductStateHash>& reachable_states,
            std::unordered_map<std::pair<std::string_view, std::string_view>,
                              std::unordered_set<std::pair<std::string_view, std::string_view>, ProductStateHash>,
                              ProductStateHash>& product_successors) const;
        
        std::vector<std::unordered_set<std::pair<std::string_view, std::string_view>, ProductStateHash>> 
        __computeProductSCCs(
            const std::unordered_set<std::pair<std::string_view, std::string_view>, ProductStateHash>& reachable_states,
            const std::unordered_map<std::pair<std::string_view, std::string_view>,
                                    std::unordered_set<std::pair<std::string_view, std::string_view>, ProductStateHash>,
                                    ProductStateHash>& product_successors) const;
        
        bool __hasAcceptingCycle(
            const std::unordered_set<std::pair<std::string_view, std::string_view>, ProductStateHash>& scc,
            const CTLAutomaton& automaton_a, const CTLAutomaton& automaton_b,
            const std::unordered_map<std::pair<std::string_view, std::string_view>,
                                    std::unordered_set<std::pair<std::string_view, std::string_view>, ProductStateHash>,
                                    ProductStateHash>& product_successors) const;
        
        

        

        std::vector<int>& __getTopologicalOrder() const;
        std::vector<std::unordered_set<int>>& __getDAG() const;
        

};






}
#endif // CTLAUTOMATON_Hunordered_set
