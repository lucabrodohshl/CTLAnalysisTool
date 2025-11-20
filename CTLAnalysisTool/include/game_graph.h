#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

#include "types.h"
#include "transitions.h"

namespace ctl {
    // Enum for players in the parity game
    // Player 1 (Eloise) tries to satisfy the formula
    // Player 2 (Abelard) tries to refute the formula
    enum class Player { 
        Player1_Eloise,  // Makes choices in OR nodes (disjunction)
        Player2_Abelard  // Makes challenges in AND nodes (conjunction)
    };

    

    // Symbolic game node representing an automaton state
    struct SymbolicGameNode {
        std::string_view state_name;      // The automaton state name
        Player owner;                      // Which player controls this node
        int priority;                      // Parity priority (0 = even/good, 1 = odd/bad)
        CTLFormulaPtr formula;            // The formula this state represents
        BinaryOperator top_operator;      // The top-level operator (AND, OR, NONE)
        
        SymbolicGameNode() 
            : state_name(""), owner(Player::Player1_Eloise), 
              priority(0), formula(nullptr), top_operator(BinaryOperator::NONE) {}
        
        SymbolicGameNode(std::string_view name, Player p, int prio, 
                        CTLFormulaPtr f, BinaryOperator op)
            : state_name(name), owner(p), priority(prio), 
              formula(f), top_operator(op) {}
    };


    // Symbolic game edge representing a transition
    struct SymbolicGameEdge {
        std::string_view source;          // Source state
        Guard symbol;                      // Guard/constraint on this transition
        std::vector<Clause> clauses;      // DNF: disjunction of conjunctions (φ₁ ∨ φ₂ ∨ ...)
        
        SymbolicGameEdge() = default;
        SymbolicGameEdge(std::string_view src, const Guard& symbol, const std::vector<Clause>& clauses)
            : source(src), symbol(symbol), clauses(clauses) {}
        
        // Get all target states reachable from this edge
        std::unordered_set<std::string_view> getTargetStates() const {
            std::unordered_set<std::string_view> targets;
            for (const auto& conj : clauses) {
                for (const auto& atom : conj.literals) {
                    targets.insert(atom.qnext);
                }
            }
            return targets;
        }
    };

    // The symbolic parity game structure
    // This represents an implicit game graph where:
    // - Nodes are automaton states
    // - Edges are defined by transition rules (in DNF)
    // - Ownership determines who makes moves
    // - Priorities define the winning condition
    struct SymbolicParityGame {
        // Map from state name to game node information
        std::unordered_map<std::string_view, SymbolicGameNode> nodes;
        
        // Map from state name to its outgoing edges (transitions in DNF)
        std::unordered_map<std::string_view, std::vector<SymbolicGameEdge>> out_edges;

        std::unordered_map<std::string_view, std::vector<SymbolicGameEdge>> in_edges;
        
        // Initial state of the game
        std::string_view initial_state;
        
        // The automaton that defines the game rules
        // This is not owned by the game, just a reference
        const class CTLAutomaton* automaton;
        
        // Statistics
        size_t num_player1_nodes = 0;
        size_t num_player2_nodes = 0;
        size_t num_priority0_nodes = 0;
        size_t num_priority1_nodes = 0;
        size_t num_edges = 0;
        
        SymbolicParityGame() : automaton(nullptr) {}
        
        // Get the outgoing edges from a state
        const std::vector<SymbolicGameEdge>& getEdgesFrom(std::string_view state) const {
            static const std::vector<SymbolicGameEdge> empty;
            auto it = out_edges.find(state);
            if (it != out_edges.end()) {
                return it->second;
            }
            return empty;
        }
        
        // Get the incoming edges to a state
        const std::vector<SymbolicGameEdge>& getEdgesTo(std::string_view state) const {
            static const std::vector<SymbolicGameEdge> empty;
            auto it = in_edges.find(state);
            if (it != in_edges.end()) {
                return it->second;
            }
            return empty;
        }
        
        // Print the game structure for debugging
        std::string toString() const;
        void print() const;


    };

    // Legacy structures for explicit game graphs (if needed)
    struct GameNode {
        std::string id;
        Player player;
        std::string_view origin_state;
    };

    struct GameEdge {
        std::string_view source_id;
        std::string_view target_id;
        Guard* guard;
    };

    struct GameGraph {
        std::unordered_map<std::string, GameNode> nodes;
        std::vector<GameEdge> edges;
    };


    // WinningRegion maps each state to a formula representing all valuations from which that player can win
    using WinningRegion = std::unordered_map<std::string_view, Guard>;

} // namespace ctl