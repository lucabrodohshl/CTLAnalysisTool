#include "CTLautomaton.h"
#include "game_graph.h"
#include "formula.h"
#include <iostream>
#include <sstream>

namespace ctl {

// Helper function to determine if a formula is an eventuality
// Eventualities are formulas that must eventually be fulfilled
// For Büchi conditions: E[ψ₁ U ψ₂], A[ψ₁ U ψ₂], EF(ψ), AF(ψ)
static bool isEventuality(const CTLFormula* formula) {
    if (!formula || !formula->isTemporal()) {
        return false;
    }
    
    const auto* temp_formula = static_cast<const TemporalFormula*>(formula);
    TemporalOperator op = temp_formula->operator_;
    
    // Until operators are eventualities
    if (op == TemporalOperator::EU || op == TemporalOperator::AU) {
        return true;
    }
    
    // Eventually operators are eventualities
    if (op == TemporalOperator::EF || op == TemporalOperator::AF) {
        return true;
    }
    
    return false;
}

// Helper function to assign ownership based on the top-level operator
static Player assignOwnership(BinaryOperator top_op) {
    switch (top_op) {
        case BinaryOperator::OR:
            // Disjunction: Player 1 (Eloise) chooses which branch to take
            return Player::Player1_Eloise;
            
        case BinaryOperator::AND:
            // Conjunction: Player 2 (Abelard) chooses which branch to attack
            return Player::Player2_Abelard;
            
        case BinaryOperator::NONE:
        default:
            // Single constraint or forced move: assign to Player 1 by convention
            return Player::Player1_Eloise;
    }
}

// Helper function to assign ownership based on temporal operators
static Player assignOwnershipFromTemporal(const CTLFormula* formula) {
    if (!formula || !formula->isTemporal()) {
        return Player::Player1_Eloise;
    }
    
    const auto* temp_formula = static_cast<const TemporalFormula*>(formula);
    
    // Existential operators: Player 1 chooses the path
    if (temp_formula->givesExistentialTransition()) {
        return Player::Player1_Eloise;
    }
    
    // Universal operators: Player 2 challenges on all paths
    if (temp_formula->givesUniversalTransition()) {
        return Player::Player2_Abelard;
    }
    
    return Player::Player1_Eloise;
}

// Helper function to assign priority based on the formula type
// Priority 0 (even): safe to be stuck in
// Priority 1 (odd): bad to be stuck in (eventuality must be resolved)
static int assignPriority(const CTLFormula* formula) {
    if (!formula) {
        return 0;
    }
    
    // Eventualities that must be fulfilled get high (odd) priority
    // Being stuck in an "Until" or "Eventually" state forever is a loss for Player 1
    if (isEventuality(formula)) {
        return 1;  // ODD priority - must progress
    }
    
    // All other states are "safe" to be stuck in
    // This includes AG, EG, and states where eventuality is resolved
    return 0;  // EVEN priority - safe
}

// Main function to build the symbolic parity game
SymbolicParityGame CTLAutomaton::buildGameGraph() const {
    SymbolicParityGame game;
    
    // Store reference to the automaton for move computation
    game.automaton = this;
    game.initial_state = initial_state_;
    
    if (verbose_) {
        std::cout << "\n=== Building Symbolic Parity Game ===" << std::endl;
        std::cout << "Initial state: " << initial_state_ << std::endl;
        std::cout << "Total automaton states: " << v_states_.size() << std::endl;
    }
    
    // For each state in the automaton, create a game node
    for (const auto& state : v_states_) {
        std::string_view state_name = state->name;
        CTLFormulaPtr formula = state->formula;
        
        // Step 1: Determine the top-level operator for this state
        BinaryOperator top_op = BinaryOperator::NONE;
        auto op_it = m_state_operator_.find(state_name);
        if (op_it != m_state_operator_.end()) {
            top_op = op_it->second;
        }
        
        // Step 2: Assign ownership based on the operator
        Player owner;
        if (top_op != BinaryOperator::NONE) {
            owner = assignOwnership(top_op);
        } else {
            // No top-level binary operator, check if it's a temporal formula
            owner = assignOwnershipFromTemporal(formula.get());
        }
        
        // Step 3: Assign priority based on the formula type
        int priority = assignPriority(formula.get());
        
        // Create the game node
        SymbolicGameNode node(state_name, owner, priority, formula, top_op);
        game.nodes[state_name] = std::move(node);
        
        // Update statistics
        if (owner == Player::Player1_Eloise) {
            game.num_player1_nodes++;
        } else {
            game.num_player2_nodes++;
        }
        
        if (priority == 0) {
            game.num_priority0_nodes++;
        } else {
            game.num_priority1_nodes++;
        }
        
        if (verbose_ & false) {
            std::cout << "  State: " << state_name 
                      << " | Owner: " << (owner == Player::Player1_Eloise ? "P1(Eloise)" : "P2(Abelard)")
                      << " | Priority: " << priority
                      << " | Operator: ";
            switch (top_op) {
                case BinaryOperator::AND: std::cout << "AND"; break;
                case BinaryOperator::OR: std::cout << "OR"; break;
                case BinaryOperator::IMPLIES: std::cout << "IMPLIES"; break;
                case BinaryOperator::NONE: std::cout << "NONE"; break;
            }
            std::cout << std::endl;
        }
    }
    
    // Step 4: Build the edges from transitions
    if (verbose_) {
        std::cout << "\n=== Building Edges from Transitions ===" << std::endl;
    }
    
    for (const auto& state : v_states_) {
        std::string_view state_name = state->name;
        
        // Get transitions for this state
        auto trans_it = m_transitions_.find(state_name);
        if (trans_it != m_transitions_.end()) {
            const auto& transitions = trans_it->second;
            
            for (const auto& transition : transitions) {
                // Check if the guard is satisfiable using the cached sat_expr pointer
                // This avoids re-parsing the formula string every time
                if(!__isSatisfiable(transition->guard))
                    continue;
                // Each transition is: guard ∧ (∨ Clause)
                // where Clause is a conjunction of (direction, next_state) pairs
                SymbolicGameEdge edge(state_name, transition->guard, transition->clauses);
                
                // Add to outgoing edges
                game.out_edges[state_name].push_back(edge);
                
                // Add to incoming edges of all target states
                // Iterate through all clauses (disjuncts) in the DNF
                //for (const auto& clause : transition->clauses) {
                //    // Each clause is a conjunction of (direction, next_state) pairs
                //    for (const auto& literal : clause.literals) {
                //        std::string_view target_state = literal.qnext;
                //        // Ensure the entry exists and add this edge as an incoming edge
                //        game.in_edges[target_state].push_back(edge);
                //    }
                //}
                
                game.num_edges++;
                
                if (verbose_) {
                    std::cout << "  Edge from " << state_name 
                              << " with guard: " << transition->guard.toString()
                              << " | Clauses: " << transition->clauses.size() << std::endl;
                }
            }
        }
    }
    
    if (verbose_) {
        std::cout << "\n=== Game Statistics ===" << std::endl;
        std::cout << "Player 1 (Eloise) nodes: " << game.num_player1_nodes << std::endl;
        std::cout << "Player 2 (Abelard) nodes: " << game.num_player2_nodes << std::endl;
        std::cout << "Priority 0 (safe) nodes: " << game.num_priority0_nodes << std::endl;
        std::cout << "Priority 1 (eventuality) nodes: " << game.num_priority1_nodes << std::endl;
        std::cout << "Total edges: " << game.num_edges << std::endl;
        std::cout << "========================\n" << std::endl;
    }
    
    return game;
}

// Implementation of getMovesFrom for SymbolicParityGame
// Returns the transition rules for a state (in DNF: list of conjunctions)


// Convert the game to a string for debugging
std::string SymbolicParityGame::toString() const {
    std::ostringstream oss;
    
    oss << "=== Symbolic Parity Game ===" << std::endl;
    oss << "Initial State: " << initial_state << std::endl;
    oss << "Total Nodes: " << nodes.size() << std::endl;
    oss << "Player 1 Nodes: " << num_player1_nodes << std::endl;
    oss << "Player 2 Nodes: " << num_player2_nodes << std::endl;
    oss << "Priority 0 Nodes: " << num_priority0_nodes << std::endl;
    oss << "Priority 1 Nodes: " << num_priority1_nodes << std::endl;
    oss << "Total Edges: " << num_edges << std::endl;
    oss << std::endl;
    
    oss << "Nodes:" << std::endl;
    for (const auto& [state_name, node] : nodes) {
        oss << "  " << state_name << ":" << std::endl;
        oss << "    Owner: " << (node.owner == Player::Player1_Eloise ? "Player1 (Eloise)" : "Player2 (Abelard)") << std::endl;
        oss << "    Priority: " << node.priority << std::endl;
        oss << "    Operator: ";
        switch (node.top_operator) {
            case BinaryOperator::AND: oss << "AND"; break;
            case BinaryOperator::OR: oss << "OR"; break;
            case BinaryOperator::IMPLIES: oss << "IMPLIES"; break;
            case BinaryOperator::NONE: oss << "NONE"; break;
        }
        oss << std::endl;
        
        if (node.formula) {
            oss << "    Formula: " << node.formula->toString() << std::endl;
        }
        
        // Print outgoing edges for this node
        auto edge_it = out_edges.find(state_name);
        if (edge_it != out_edges.end() && !edge_it->second.empty()) {
            oss << "    Outgoing Edges: " << edge_it->second.size() << std::endl;
            for (size_t i = 0; i < edge_it->second.size(); ++i) {
                const auto& edge = edge_it->second[i];
                oss << "      Edge " << (i+1) << ":" << std::endl;
                oss << "        Guard: " << edge.symbol.toString() << std::endl;
                oss << "        Clauses: " << edge.clauses.size()  << std::endl;
                
                for (size_t j = 0; j < edge.clauses.size(); ++j) {
                    const auto& conj = edge.clauses[j];
                    oss << "          Clause " << (j+1) << ": [";
                    for (size_t k = 0; k < conj.literals.size(); ++k) {
                        const auto& literal = conj.literals[k];
                        if (k > 0) oss << " ∧ ";
                        oss << "(dir:" << literal.dir << ", " << literal.qnext << ")";
                    }
                    oss << "]" << std::endl;
                }
            }
        }
    }
    
    oss << "===========================" << std::endl;
    
    return oss.str();
}

void SymbolicParityGame::print() const {
    std::cout << toString();
}

} // namespace ctl