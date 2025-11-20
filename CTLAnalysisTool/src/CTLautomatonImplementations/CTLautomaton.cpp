#include "CTLautomaton.h"
#include <queue>
namespace ctl {

// Pre-computed hash values for fast formula comparison
const size_t CTLAutomaton::TRUE_HASH = BooleanLiteral(true).hash();
const size_t CTLAutomaton::FALSE_HASH = BooleanLiteral(false).hash();

// Constructor from CTL formula
CTLAutomaton::CTLAutomaton(const CTLFormula& formula, bool verbose) : verbose_(verbose) {
    buildFromFormula(formula);

};

CTLAutomaton::CTLAutomaton(const CTLAutomaton& other) {
    // Copy constructor implementation (deep copy if necessary)
    p_original_formula_ = other.p_original_formula_->clone();
    p_negated_formula_ = other.p_negated_formula_->clone();
    verbose_ = other.verbose_;
}



CTLAutomaton& CTLAutomaton::operator=(const CTLAutomaton& other)  {
    // Copy assignment implementation (deep copy if necessary)
    if (this != &other) {
        p_original_formula_ = other.p_original_formula_->clone();
    }
    return *this;
}

CTLAutomaton::CTLAutomaton(CTLAutomaton&& other) noexcept {
    p_original_formula_ = std::move(other.p_original_formula_);
    // Move constructor implementation
}
CTLAutomaton& CTLAutomaton::operator=(CTLAutomaton&& other) noexcept {
    // Move assignment implementation
    if (this != &other) {
        p_original_formula_ = std::move(other.p_original_formula_);
    }
    return *this;
}



void CTLAutomaton::print() const{
    std::cout << toString() << std::endl;
}
std::string CTLAutomaton::toString() const {
    std::string out;
    out += "CTLAutomaton\n";
    out += "States:\n";
    for (const auto& s : v_states_) {
        out += "  " + s->name + ": " + s->formula->toString();
        if (s->name == initial_state_)
            out += "  (initial)";
        out += "\n";
    }

    out += "\nTransitions (δ):\n";
    if (m_transitions_.empty()) {
        out += "  (no transitions)\n";
        return out;
    }

    for (const auto& [from, tlist] : m_transitions_) {
        for (const auto& t : tlist) {
            out += "  " + std::string(t->from) + "  --[guard: " + t->guard + "]--> \n";
            int idx = 0;
            for (const auto& conj : t->clauses) {
                out += " ( ";
                if (conj.literals.empty()) {
                    out += "(ε) and";
                    continue;
                }
                for (const auto& a : conj.literals) {
                    out += "(" + std::to_string(a.dir) + "," + std::string(a.qnext) + ") and ";
                }
                //remove last and
                if(out.size() >= 4 && out.substr(out.size() - 5) == " and ") out = out.substr(0, out.size() - 5);
                out += ") or \n";
            }
            out = out.substr(0, out.size() - 3);
            out += "\n";
        }

        
    }

    out += "Accepting States:\n";
    out += "{";
    for (const auto& s : s_accepting_states_) {
        out += " " + std::string(s) + ", ";
    }
    if (!s_accepting_states_.empty())
        out = out.substr(0, out.size() - 2); // remove last comma and space
    out += "}\n";
    return out;
}



std::string_view CTLAutomaton::getStateOfFormula(const CTLFormula& f) const {
    // Use hash-based lookup for O(1) formula comparison
    size_t target_hash = f.hash();
    
    // Check cache first - this should be the common case after build
    auto cache_it = formula_hash_to_state_cache_.find(target_hash);
    if (cache_it != formula_hash_to_state_cache_.end()) {
        return cache_it->second;
    }
    
    // If not in cache, do full search (this should be rare after build)
    for (const auto& state : v_states_) {
        // First check hash for quick elimination
        if (state->formula->hash() == target_hash) {
            // Hash matches, now do expensive equals check
            if (state->formula->equals(f)) {
                // Cache the result for future lookups
                formula_hash_to_state_cache_[target_hash] = state->name;
                return state->name;
            }
        }
    }

    // Final fallback: string comparison (should almost never happen)
    static thread_local std::string target_str;
    target_str = f.toString();
    
    for (const auto& state : v_states_) {
        if (state->formula->toString() == target_str) {
            // Cache this result too
            formula_hash_to_state_cache_[target_hash] = state->name;
            return state->name;
        }
    }

    // Error case - build debug info
    std::string a = "Available states:\n";
    a.reserve(v_states_.size() * 100);
    for (const auto& state : v_states_) {
       a += "  " + state->name + ": " + state->formula->toString() + "\n";
    }
    throw std::runtime_error("State for formula not found: " + target_str + "\n" + a);
}


bool CTLAutomaton::isAccepting(const std::string_view state_name) const {
    return s_accepting_states_.count(state_name) > 0;
}

std::string CTLAutomaton::getFormulaString() const {
    return p_original_formula_ ? p_original_formula_->toString() : "";
}
CTLFormulaPtr CTLAutomaton::getFormula() const {
    return p_original_formula_ ? p_original_formula_->clone() : nullptr;
}

CTLFormulaPtr CTLAutomaton::getNegatedFormula() const {
    return p_negated_formula_ ? p_negated_formula_->clone() : nullptr;
}








    std::vector<int>& CTLAutomaton::__getTopologicalOrder() const {

        if (!topological_order_.empty()) {
            return topological_order_;
        }

        auto& block_edges = __getDAG();

        // 3) Compute topological order of the DAG
        std::vector<int> topo;
        std::vector<int> indeg(blocks_->size(), 0);
        for (int i = 0; i < (int)blocks_->size(); ++i){
            for (int j : block_edges[i]) indeg[j]++;
        }
        std::queue<int> q;
        for (int i = 0; i < (int)blocks_->size(); ++i)
            if (indeg[i] == 0) q.push(i);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            topo.push_back(u);

            for (int v : block_edges[u])
                if (--indeg[v] == 0) q.push(v);
        }

        topological_order_ = std::move(topo);
        return  topological_order_;
    }





    bool CTLAutomaton::__isSatisfiable(const Guard& g) const {
        // Use the SMT interface to check satisfiability of the guard
        if (!smt_interface_) {
            throw std::runtime_error("SMT interface not initialized");
        }
        return smt_interface_->isSatisfiable(g.sat_expr);
    }


}