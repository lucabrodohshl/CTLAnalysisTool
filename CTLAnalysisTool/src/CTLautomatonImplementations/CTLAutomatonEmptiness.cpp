
#include "CTLautomaton.h"
#include "formula.h"
#include <queue>
#include <algorithm>
#include <iostream>

namespace ctl {

    
    bool CTLAutomaton::isEmpty() const {
        throw std::runtime_error("isEmpty() not implemented yet");
    }

    // ==================== Helper Functions for Formula Manipulation ====================
    
    Guard CTLAutomaton::makeTrue() const {
        Guard g;
        g.pretty_string = "true";
        g.sat_expr = (void*)smt_interface_->getTrue();
        return g;
    }
    
    Guard CTLAutomaton::makeFalse() const {
        Guard g;
        g.pretty_string = "false";
        g.sat_expr = (void*)smt_interface_->getFalse();
        return g;
    }
    
    Guard CTLAutomaton::makeOr(const Guard& left, const Guard& right) const {
        // Simplifications
        if (isEquivalentToTrue(left)) return left;
        if (isEquivalentToTrue(right)) return right;
        if (isEquivalentToFalse(left)) return right;
        if (isEquivalentToFalse(right)) return left;
        Guard g;
        g.pretty_string = "(" + left.pretty_string + " | " + right.pretty_string + ")";
        g.sat_expr = smt_interface_->makeOr(left.sat_expr, right.sat_expr);
        return g;
    }
    
    Guard CTLAutomaton::makeAnd(const Guard& left, const Guard& right) const {

        // Simplifications
        if (isEquivalentToFalse(left)) return left;
        if (isEquivalentToFalse(right)) return right;
        if (isEquivalentToTrue(left)) return right;
        if (isEquivalentToTrue(right)) return left;
        
        Guard g;
        g.pretty_string = "(" + left.pretty_string + " & " + right.pretty_string + ")";
        g.sat_expr = smt_interface_->makeAnd(left.sat_expr, right.sat_expr);
        return g;
    }
    
    bool CTLAutomaton::isEquivalentToTrue(const Guard& g) const {
        return std::hash<std::string>{}(g.pretty_string) == TRUE_HASH; 
    }
    
    bool CTLAutomaton::isEquivalentToFalse(const Guard& g) const {
         return std::hash<std::string>{}(g.pretty_string) == TRUE_HASH; 
    }

    
    bool CTLAutomaton::winningRegionsEqual(const WinningRegion& a, const WinningRegion& b) const {
        if (a.size() != b.size()) return false;
        
        for (const auto& [state, g_a] : a) {
            auto it = b.find(state);
            if (it == b.end()) return false;
            
            // Compare formulas (simplified comparison)
            const auto& g_b = it->second;
            if (!(g_a==g_b)) return false;
        }
        return true;
    }

    // ==================== Core Algorithm: Symbolic Predecessor ====================
    
    // Evaluate a single clause: conjunction of (direction, next_state) pairs
    Guard CTLAutomaton::evaluateClause(const Clause& clause, const WinningRegion& target_region) const {
        Guard result = makeTrue();
        std::cout << "    Evaluating clause with " << clause.literals.size() << " literals" << std::endl;
        // A clause is a conjunction: (dir1, q1) ∧ (dir2, q2) ∧ ...
        for (const auto& literal : clause.literals) {
            std::string_view next_state = literal.qnext;
            
            // Get the winning formula for the next state from the target region
            auto it = target_region.find(next_state);
            Guard next_formula = (it != target_region.end()) ? it->second : makeFalse();
            
            // Conjoin with the result
            result = makeAnd(result, next_formula);
        }

        
        return result;
    }
    
    // Evaluate a transition edge: guard ∧ (clause1 ∨ clause2 ∨ ...)
    Guard CTLAutomaton::evaluateRule(const SymbolicGameEdge& edge, const WinningRegion& target_region) const {
        // Parse the guard into a formula
        Guard guard;
        std::cout << "Evaluating rule with guard: " << edge.symbol.toString() << std::endl;
        if (edge.symbol == makeTrue()) {
            std::cout << "  Guard is TRUE" << std::endl;
            guard = makeTrue();
        } else if (edge.symbol == makeFalse()) {
            guard = makeFalse();
        } else {
            std::cout << "  Guard is neither TRUE nor FALSE: " << edge.symbol.pretty_string << std::endl;
            guard = edge.symbol;
        }
        
        // If guard is false, the entire transition is impossible
        if (isEquivalentToFalse(guard)) {
            std::cout << "  Guard is FALSE, transition impossible" << std::endl;
            return makeFalse();
        }
        
        // Evaluate the DNF: clause1 ∨ clause2 ∨ ...
        Guard dnf_result = makeFalse();
        for (const auto& clause : edge.clauses) {
            Guard clause_formula = evaluateClause(clause, target_region);
            std::cout << "  Evaluating clause, result: " << clause_formula.pretty_string << std::endl;
            dnf_result = makeOr(dnf_result, clause_formula);
        }
        
        // Combine: guard ∧ DNF
        return makeAnd(guard, dnf_result);
    }
    
    // Compute the one-step predecessor: all states that can reach the target in one step
    WinningRegion CTLAutomaton::symPre(const SymbolicParityGame& game, const WinningRegion& target_region) const {
        WinningRegion pre_region;
        
        // For each state in the game
        for (const auto& [state, node] : game.nodes) {
            // Get all outgoing edges from this state
            const auto& edges = game.getEdgesFrom(state);
            
            if (edges.empty()) {
                // No outgoing edges: cannot reach target
                pre_region[state] = makeFalse();
                continue;
            }
            
            // Build the predecessor formula based on the player ownership
            Guard state_formula = makeFalse();
            
            if (node.owner == Player::Player1_Eloise) {
                // Player 1's turn: needs at least ONE edge to reach target (OR)
                for (const auto& edge : edges) {
                    Guard edge_formula = evaluateRule(edge, target_region);
                    state_formula = makeOr(state_formula, edge_formula);
                }
            } else {
                // Player 2's turn: ALL edges must lead to target (AND)
                state_formula = makeTrue();
                for (const auto& edge : edges) {
                    Guard edge_formula = evaluateRule(edge, target_region);
                    state_formula = makeAnd(state_formula, edge_formula);
                }
            }
            
            pre_region[state] = state_formula;
        }
        
        return pre_region;
    }

    // ==================== Attractor Algorithm (Least Fixed-Point) ====================
    
    WinningRegion CTLAutomaton::symbolicAttractor(const SymbolicParityGame& game, 
                                                   const WinningRegion& target, 
                                                   Player sigma) const {
        WinningRegion attractor = target;
        
        if (verbose_) {
            std::cout << "Computing symbolic attractor for player " 
                      << (sigma == Player::Player1_Eloise ? "Eloise" : "Abelard") << std::endl;
        }
        
        int iteration = 0;
        while (true) {
            iteration++;
            WinningRegion prev_attractor = attractor;
            std::cout << "  Attractor iteration " << iteration << std::endl;

            // Compute one-step predecessors
            WinningRegion predecessors = symPre(game, attractor);
            
            // Update attractor based on player ownership
            for (const auto& [state, node] : game.nodes) {
                
                    attractor[state] = makeOr(attractor[state], predecessors[state]);
                
            }
            
            // Check for fixed point
            if (winningRegionsEqual(attractor, prev_attractor)) {
                if (verbose_) {
                    std::cout << "  Attractor converged after " << iteration << " iterations" << std::endl;
                }
                break;
            }
            
            if (iteration > 1000) {
                throw std::runtime_error("Attractor computation did not converge after 1000 iterations");
            }
        }
        
        return attractor;
    }
    
    // ==================== Trap Computation (Greatest Fixed-Point) ====================
    
    // Computes the largest set of states from which Player sigma can force the game
    // to stay within the 'domain' forever
    WinningRegion CTLAutomaton::symbolicTrap(const SymbolicParityGame& game,
                                             const WinningRegion& domain,
                                             Player sigma) const {
        WinningRegion trap = domain;
        
        if (verbose_) {
            std::cout << "Computing trap for player " 
                      << (sigma == Player::Player1_Eloise ? "Eloise" : "Abelard") << std::endl;
        }
        
        int iteration = 0;
        while (true) {
            iteration++;
            WinningRegion prev_trap = trap;
            
            // Compute predecessors that stay within the current trap
            WinningRegion predecessors = symPre(game, trap);
            
            // A state stays in the trap if it was there before AND
            // the player can force to stay in the trap
            for (const auto& [state, node] : game.nodes) {
                // The state must have been in the trap
                Guard in_trap = trap[state];
                
                // AND the player must be able to move within the trap
                Guard can_stay = predecessors[state];
                
                trap[state] = makeAnd(in_trap, can_stay);
            }
            
            // Check for fixed point
            if (winningRegionsEqual(trap, prev_trap)) {
                if (verbose_) {
                    std::cout << "  Trap converged after " << iteration << " iterations" << std::endl;
                }
                break;
            }
            
            if (iteration > 1000) {
                throw std::runtime_error("Trap computation did not converge after 1000 iterations");
            }
        }
        
        return trap;
    }

    // ==================== Main Solver: Nested Fixed-Point for Büchi Games ====================
    
    std::pair<WinningRegion, WinningRegion> CTLAutomaton::symbolicSolveRecursive(const SymbolicParityGame& game) const {
        WinningRegion W_0, W_1;
        
        // BASE CASE: If there are no states, return empty winning regions
        if (game.nodes.empty()) {
            if (verbose_) {
                std::cout << "Base case: empty game" << std::endl;
            }
            return {W_0, W_1};
        }
        
        if (verbose_) {
            std::cout << "\n=== Solving Büchi game with " << game.nodes.size() << " states ===" << std::endl;
        }
        
        // W will be the winning region for Player 0 (Eloise).
        // Initialize it optimistically to all 'true'.
        WinningRegion W;
        for (const auto& [state, node] : game.nodes) {
            W[state] = makeTrue();
        }
        
        // OUTER LOOP: Greatest Fixed-Point for the overall winning region W.
        // This is the main loop that converges to the largest set from which Eloise can win.
        int iteration = 0;
        while (true) {
            iteration++;
            if (verbose_) {
                std::cout << "\n--- Outer GFP Iteration " << iteration << " ---" << std::endl;
            }
            
            WinningRegion W_prev = W;
            
            // Step 1: Define the target set for this iteration:
            //         It's the set of accepting states (priority 0) that are
            //         still considered part of the winning region W.
            // BUG FIX 1: Target is priority 0 (even = accepting), not priority 1!
            WinningRegion target;
            for (const auto& [state, node] : game.nodes) {
                if (node.priority == 0) {  // Priority 0 = even = accepting/good
                    target[state] = W[state];
                } else {
                    target[state] = makeFalse();
                }
            }
            std::cout << "Target states for this iteration:" << std::endl;
            for (const auto& [state, formula] : target) {
                if (!isEquivalentToFalse(formula)) {
                    std::cout << "  State: " << state << " Formula: " << formula.pretty_string << std::endl;
                }
            }
            
            // Step 2: Compute the attractor to this target.
            //         The attractor function itself is a least fixed-point computation (inner LFP).
            //         This computes the set of states from which Eloise can FORCE a visit
            //         to an accepting state within the current winning region W.
            WinningRegion attractor = symbolicAttractor(game, target, Player::Player1_Eloise);
            
            // Step 3: The new winning region is this attractor.
            //         BUG FIX 2: W = attractor, not W = trap(attractor).
            //         Any state not in the attractor is a state from which Eloise cannot
            //         guarantee reaching an accepting state, so it's a losing state.
            W = attractor;
            
            // Check for convergence of the outer GFP loop.
            if (winningRegionsEqual(W, W_prev)) {
                if (verbose_) {
                    std::cout << "Outer GFP loop converged after " << iteration << " iterations." << std::endl;
                }
                break;
            }
            
            if (iteration > 100) {
                throw std::runtime_error("Outer fixed-point did not converge after 100 iterations");
            }
        }
        
        // The final W is the winning region for Player 0 (Eloise)
        W_0 = W;
        
        // Player 1 (Abelard) wins where Player 0 does not.
        // Symbolically: W_1[state] = NOT W_0[state]
        for (const auto& [state, node] : game.nodes) {
            // For now, we compute the complement symbolically
            // If W_0[state] is true, then W_1[state] is false, and vice versa
            if (isEquivalentToTrue(W_0[state])) {
                W_1[state] = makeFalse();
            } else if (isEquivalentToFalse(W_0[state])) {
                W_1[state] = makeTrue();
            } else {
                // For complex formulas, we would need makeNot()
                // For now, just set to false (conservative: we only care about W_0 for satisfiability)
                W_1[state] = makeFalse();
            }
        }
        
        return {W_0, W_1};
    }
    
    // ==================== Top-Level Function: CTL Satisfiability Check ====================
    
    bool CTLAutomaton::checkCtlSatisfiability() const {
        if (verbose_) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "Starting CTL Satisfiability Check" << std::endl;
            std::cout << "========================================\n" << std::endl;
        }
        
        // Step 1: Build the symbolic parity game from the automaton
        SymbolicParityGame game = buildGameGraph();
        
        if (verbose_) {
            std::cout << "Built symbolic parity game with " << game.nodes.size() << " nodes" << std::endl;
        }
        
        // Step 2: Solve the game recursively
        auto [W_0, W_1] = symbolicSolveRecursive(game);
        
        // Step 3: Get the winning formula for Player 0 (Eloise) at the initial state
        std::string_view initial_state = game.initial_state;
        Guard winning_formula = W_0[initial_state];
        
        if (verbose_) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "Winning formula for initial state: " << std::endl;

            std::cout << winning_formula.pretty_string << std::endl;
            std::cout << "========================================\n" << std::endl;
        }
        
        // Step 4: THE SMT SOLVER CALL
        // Check if there exists any valuation that makes this formula true
        if (isEquivalentToFalse(winning_formula)) {
            return false;  // Trivially unsatisfiable
        }
        
        if (isEquivalentToTrue(winning_formula)) {
            return true;   // Trivially satisfiable
        }
        
        bool is_sat = __isSatisfiable(winning_formula);
        
        if (verbose_) {
            std::cout << "SMT solver result: " << (is_sat ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
        }
        
        return is_sat;
    }




    std::vector<std::unordered_set<int>>& CTLAutomaton::__getDAG() const
    {
        if (!block_edges_.empty()) {
            return block_edges_;
        }

        std::vector<std::unordered_set<int>> block_edges(blocks_->size());
        for (int i = 0; i < (int)blocks_->size(); ++i) {
            for (const auto& st : blocks_->blocks[i]) {
                auto it = state_successors_.find(st);
                if (it == state_successors_.end()) {
                    block_edges[i] = {};
                    continue;
                }
                //block_edges[i] = {};
                for (const auto& succ : it->second) {
                    int bj = blocks_->getBlockId(succ);
                    if (bj != i) block_edges[i].insert(bj);
                }
            }
        }
        block_edges_ = std::move(block_edges);
        return block_edges_;

    }
}