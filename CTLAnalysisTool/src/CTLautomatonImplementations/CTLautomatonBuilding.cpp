

#include "CTLautomaton.h"


#include <sstream>
#include <algorithm>
#include <queue>
#include <stack>

namespace ctl {

  void CTLAutomaton::buildFromFormula(const CTLFormula& formula, bool symbolic) {
    if (verbose_) std::cout << "Building automaton from formula: " << formula.toString() << "\n";
    p_original_formula_ = std::move(formula_utils::preprocessFormula(formula, false));
    if (verbose_) std::cout << "Converted formula: " << p_original_formula_->toString() << "\n";
    p_negated_formula_ = std::move(formula_utils::negateFormula(formula,     false));
    smt_interface_ = createDefaultSMTInterface();
    __buildFromFormula( false);
    blocks_ = std::make_unique<SCCBlocks>(__computeSCCs());

    
    //__decideBlockTypes();
  }







  void CTLAutomaton::__buildFromFormula(bool symbolic) {
      // Implementation of the actual building logic goes here
      s_accepting_states_.clear();
      state_successors_.clear();
      m_transitions_.clear();
      v_states_.clear();

    if(p_original_formula_->hash() == TRUE_HASH){
        auto state = std::make_shared<CTLState>();
        state->name = "q0";
        state->formula = p_original_formula_->clone();
        v_states_.push_back(state);
        initial_state_ = state->name;
        formula_hash_to_state_cache_[p_original_formula_->hash()] = state->name;
        addTransition(state->name, GTrue, { Clause{ { /* empty */ } } });
        return;
    }


    if (p_original_formula_->hash() == FALSE_HASH)
    {
        auto state = std::make_shared<CTLState>();
        state->name = "q0";
        state->formula = p_original_formula_->clone();
        v_states_.push_back(state);
        initial_state_ = state->name;
        formula_hash_to_state_cache_[p_original_formula_->hash()] = state->name;
        __addFalseTransition(state->name);
        return;
    }

    std::unordered_map<formula_utils::FormulaKey, const CTLFormula*, formula_utils::FormulaKeyHash> seen;
    std::vector<const CTLFormula*> subs;
    collectClosureDFS(*p_original_formula_, seen, subs);
    //std::cout << "Total subformulas collected: " << topo.size() << std::endl;
    // Create one state per subformula
    for (const CTLFormula* sf : subs) {
        auto state = std::make_shared<CTLState>();
        state->name = "q" + std::to_string(v_states_.size());
        state->formula = sf->clone();
        v_states_.push_back(state);
        formula_hash_to_state_cache_[sf->hash()] = state->name;
        if (sf->getType() == FormulaType::BINARY) {
            //m_state_operator_[state->name] = static_cast<const BinaryFormula*>(sf)->operator_;
            m_state_operator_[state->name] = static_cast<const BinaryFormula*>(sf)->operator_;
        }

        if (sf->getType() == FormulaType::TEMPORAL) {
            auto t = static_cast<const TemporalFormula*>(sf);
            if (t->operator_ == TemporalOperator::AU || t->operator_ == TemporalOperator::EU) 
                continue; // this is a waiting state, it is not accepting
             s_accepting_states_.insert(state->name);
        }else{
            s_accepting_states_.insert(state->name);
        }
    }
    // 3) Handle state transitions
    __handleStatesAndTransitions(symbolic);


    initial_state_ = getStateOfFormula(*p_original_formula_);
    
    
  }



void CTLAutomaton::__clean() {
    throw std::runtime_error("CTLAutomaton::__clean not implemented yet.");
}

bool CTLAutomaton::__isSatisfiable(const std::string& g, bool without_parsing) const {
    if (!smt_interface_) {
        throw std::runtime_error("SMT interface not initialized");
    }

    bool r= smt_interface_->isSatisfiable(g, without_parsing);
    return r;
}


bool CTLAutomaton::__isSatisfiable(const std::unordered_set<std::string>& g, bool without_parsing) const {
    if (!smt_interface_) {
        throw std::runtime_error("SMT interface not initialized");
    }
    return smt_interface_->isSatisfiable(g, without_parsing);
}

Guard CTLAutomaton::createGuardFromString(const std::string& guard) const{
    if (!smt_interface_) {
        throw std::runtime_error("SMT interface not initialized");
    }
    Guard g;
    g.pretty_string = guard;
    g.sat_expr = nullptr;  // Not using the pointer due to Z3 lifetime issues
    
    // Check satisfiability - if UNSAT, replace with "false"
    if (!__isSatisfiable(guard, false)) {
        g.pretty_string = GFalse;
        return g;
    }
    
    return g;
}


  std::string CTLAutomaton::__handleProp (const std::string& proposition, bool symbolic){
        // If symbolic=true we can decide satisfiability now; else keep the prop (or "true") per your needs
        //if (!symbolic) return proposition.empty() ? "true" : proposition;
        //return proposition.empty() ? "" : proposition;
        //bool sat = __isSatisfiable(proposition);
        //if (!sat) return "false";
        //return proposition.empty() ? "" : proposition;
        if (std::hash<std::string>{}(proposition) == TRUE_HASH) return "true";
        if (std::hash<std::string>{}(proposition) == FALSE_HASH) return "false";
        bool sat = __isSatisfiable(proposition);
        if (!sat) return "false";
        return proposition.empty() ? "" : proposition;
        //else return (symbolic ? "true" : proposition);
        //return sat ? "true" : "false";
    };




  void CTLAutomaton::addEdge(const std::string_view from, const std::string_view to) {
        state_successors_[from].insert(to);
        
    };

    // Record graph edges and store exactly ONE transition (guard + clauses).
  void CTLAutomaton::addTransition(const std::string_view from,
                             const std::string& guard,
                             const std::vector<Clause>& clauses,
                             bool is_dnf) {
      // Record edges for SCCs
 
      auto& succs = state_successors_[from];
      for (const auto& c : clauses) {
          for (const auto& a : c.literals) {
              succs.insert(a.qnext); // ok if duplicates; SCC code can tolerate or you can dedup
          }
      }
      // Single transition object
      CTLTransitionPtr t = std::make_shared<CTLTransition>();
        t->guard     = createGuardFromString(guard);
        t->clauses = clauses;
        t->is_dnf = is_dnf;
        t->from      = from;
        m_transitions_[from].push_back(std::move(t));
//
      //
      //t->guard     = createGuardFromString(guard);
      //t->clauses = clauses;
      //t->from      = from;
      //m_transitions_[from].push_back(std::move(t));
  };


  void CTLAutomaton::__handleORTransition(const std::shared_ptr<BinaryFormula>& bin, const CTLStatePtr& state)
  {
        auto leftType  = bin->left->getType();
        auto rightType = bin->right->getType();
        // false/true simplifications
        if (leftType == FormulaType::BOOLEAN_LITERAL) {
            auto b = std::dynamic_pointer_cast<BooleanLiteral>(bin->left);
            if (b->value) {
                // true ∨ x = true
                __addTrueTransition(state->name);
                return;
            } 
        }
        if (rightType == FormulaType::BOOLEAN_LITERAL) {
            auto b = std::dynamic_pointer_cast<BooleanLiteral>(bin->right);
            if (b->value) {
                __addTrueTransition(state->name);
                return;
            }
        }
        // If either side is 'false', drop it (q ∨ false = q)
        std::vector<Clause> clauses;
        if (!(leftType == FormulaType::BOOLEAN_LITERAL &&
            !std::dynamic_pointer_cast<BooleanLiteral>(bin->left)->value)) {
            std::string_view s_left = getStateOfFormula(*bin->left);
            clauses.push_back(Clause{ { { -1, s_left } } });
        }
        if (!(rightType == FormulaType::BOOLEAN_LITERAL &&
            !std::dynamic_pointer_cast<BooleanLiteral>(bin->right)->value)) {
            std::string_view s_right = getStateOfFormula(*bin->right);
            clauses.push_back(Clause{ { { -1, s_right } } });
        }
        if (clauses.empty()) {
            // both sides false → δ = false
            addTransition(state->name, GFalse, { Clause{ {} } });
        } else {
            __addTrueTransition(state->name, clauses);
        }
  }


  void CTLAutomaton::__handleAXTransition(const std::shared_ptr<TemporalFormula>& bin, const CTLStatePtr& state)
  {
        if (bin->operand->getType() == FormulaType::BOOLEAN_LITERAL) {
            auto b = std::dynamic_pointer_cast<BooleanLiteral>(bin->operand);
            if (!b->value) {
                // AX false = false
                addTransition(state->name, GFalse, { Clause{ {} } });
                return;
            } else {
                // AX true = true
                __addTrueTransition(state->name);
                return;
            }
        }
        std::string_view sub = getStateOfFormula(*bin->operand);
        addTransition(state->name, GTrue, { Clause{ { { L, sub }, { R, sub } } } });
  }

  void CTLAutomaton::__handleStatesAndTransitions(bool symbolic)
  {

    std::queue<CTLStatePtr> worklist;
    std::unordered_set<std::string_view> processed_states;
    // Initial population of the worklist
    for (const auto& state : v_states_) {
        worklist.push(state);
    }
    while (!worklist.empty()) {
        auto state = worklist.front();
        worklist.pop();
        if (processed_states.find(state->name) != processed_states.end()) {
            continue; // already processed
        }
        processed_states.insert(state->name);
        FormulaType t = state->formula->getType();
        
        
        switch (t) {
            case FormulaType::BOOLEAN_LITERAL: {
                auto b = std::dynamic_pointer_cast<BooleanLiteral>(state->formula);
                if (b->value) {
                    // δ(q) := true   (clauses may be empty; treat as one empty conj under guard=true)
                    addTransition(state->name, GTrue, { Clause{ { /* empty */ } } });
                } else {
                    // false: no satisfiable transition, or guard=false
                    addTransition(state->name, GFalse, { Clause{ {} } }); // harmless; existsSatisfyingTransition will skip
                }
                break;
            }
            case FormulaType::ATOMIC: 
            {
                auto a = std::dynamic_pointer_cast<AtomicFormula>(state->formula);
                addTransition(state->name, a->proposition, { Clause{ {} } });
                break;
            }
            case FormulaType::NEGATION: 
              {
                  auto n = std::dynamic_pointer_cast<NegationFormula>(state->formula);
                  if (auto atom = std::dynamic_pointer_cast<AtomicFormula>(n->operand)) {
                      addTransition(state->name, "!(" + atom->proposition + ")", { Clause{ {} } });
                      break;
                  }
                  // Handle other negations if needed
                  throw std::runtime_error("Unhandled negation formula in builder: " + state->name + " : " + state->formula->toString());
                  break;
              }

            case FormulaType::BINARY: 
              {
                auto bin =  std::dynamic_pointer_cast<BinaryFormula>(state->formula);
                switch (bin->operator_) {
                    case BinaryOperator::AND: {
                        auto leftType  = bin->left->getType();
                        auto rightType = bin->right->getType();

                        // true/false simplifications
                        if (leftType == FormulaType::BOOLEAN_LITERAL) {
                            auto b = std::dynamic_pointer_cast<BooleanLiteral>(bin->left);
                            if (!b->value) {
                                // left = false → whole conj = false
                               __addFalseTransition(state->name); 
                                break;
                            } // if true → skip left, use right only
                        }

                        if (rightType == FormulaType::BOOLEAN_LITERAL) {
                            auto b = std::dynamic_pointer_cast<BooleanLiteral>(bin->right);
                            if (!b->value) {
                               __addFalseTransition(state->name);
                                break;
                            }
                        }

                        // If either side is 'true', drop it (q ∧ true = q)
                        std::string_view s_left, s_right;
                        if (!(leftType == FormulaType::BOOLEAN_LITERAL &&
                            std::dynamic_pointer_cast<BooleanLiteral>(bin->left)->value)) {
                            s_left = getStateOfFormula(*bin->left);
                        }
                        if (!(rightType == FormulaType::BOOLEAN_LITERAL &&
                            std::dynamic_pointer_cast<BooleanLiteral>(bin->right)->value)) {
                            s_right = getStateOfFormula(*bin->right);
                        }

                        // If both sides reduced away (true ∧ true)
                        if (s_left.empty() && s_right.empty()) {
                            __addTrueTransition(state->name); // δ = true
                        } else if (s_left.empty()) {
                            addTransition(state->name, GTrue, { Clause{ { { -1, s_right } } } });
                        } else if (s_right.empty()) {
                            addTransition(state->name, GTrue, { Clause{ { { -1, s_left } } } });

                        } else {
                            // normal case: φ ∧ ψ
                            addTransition(state->name, GTrue,
                                { Clause{ { { -1, s_left }, { -1, s_right } } } });

                        }
                        break;
                    }


                    case BinaryOperator::OR: {
                        __handleORTransition(bin, state);
                        break;
                    }

                    default:
                        throw std::runtime_error("Unsupported binary operator in builder");
                }
    
              }
              break;
              // Temporal core, 
              // For now, AU and EU and AR and ER do not use AX, EX inside the self loops, but directly reference the state itself.
            case FormulaType::TEMPORAL: 
              {
                using Op = TemporalOperator;
                auto t = std::dynamic_pointer_cast<TemporalFormula>(state->formula);
                switch (t->operator_) {
                    // AX φ  :  ∧_{c∈{L,R}} next(c, φ)
                    case Op::AX: {
                        __handleAXTransition(t, state);
                        break;
                    }
                    // EX φ  :  ∨_{c∈{L,R}} next(c, φ)
                    case Op::EX: {
                        if (t->operand->getType() == FormulaType::BOOLEAN_LITERAL) {
                            auto b = std::dynamic_pointer_cast<BooleanLiteral>(t->operand);
                            if (!b->value) {
                                // AX false = false
                               __addFalseTransition(state->name);
                                break;
                            } else {
                                // AX true = true
                                __addTrueTransition(state->name);
                                break;
                            }
                        }
                        std::string_view sub = getStateOfFormula(*t->operand);
                        addTransition(state->name, GTrue, { Clause{ { { L, sub } } }, Clause{ { { R, sub } } } });
                        break;
                    }
                    // A(φ U ψ) : ψ  ∨  (φ ∧ AX self)
                    case Op::AU: {
                        std::vector<Clause> clauses;

                        const auto& phi = *t->operand;
                        const auto& psi = *t->second_operand;

                        bool phi_true  = (phi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        std::dynamic_pointer_cast<BooleanLiteral>(t->operand)->value);
                        bool phi_false = (phi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        !std::dynamic_pointer_cast<BooleanLiteral>(t->operand)->value);
                        bool psi_true  = (psi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        std::dynamic_pointer_cast<BooleanLiteral>(t->second_operand)->value);
                        bool psi_false = (psi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        !std::dynamic_pointer_cast<BooleanLiteral>(t->second_operand)->value);

                        // Simplify trivial cases
                        if (psi_true) {
                            // A(φ U true) = true
                            __addTrueTransition(state->name);
                            break;
                        }
                        if (psi_false && phi_false) {
                            // A(false U false) = false
                           __addFalseTransition(state->name);
                            break;
                        }
                        if (phi_false && !psi_false) {
                            // A(false U ψ) = ψ
                            std::string_view s_psi = getStateOfFormula(psi);
                            clauses.push_back(Clause{ { { -1, s_psi } } });
                            __addTrueTransition(state->name, clauses);
                            break;
                        }
                        if (psi_false && !phi_false) {
                            // A(φ U false) = false
                           __addFalseTransition(state->name);
                            break;
                        }

                        // (1) Immediate satisfaction: ψ
                        if (!psi_false) {
                            std::string_view s_psi = getStateOfFormula(psi);
                            clauses.push_back(Clause{ { { -1, s_psi } } });
                        }

                        // (2) Continue case: φ ∧ AX self  (universal next)
                        if (!phi_false) {
                            Clause cont;
                            if (!phi_true) {
                                std::string_view s_phi = getStateOfFormula(phi);
                                cont.literals.push_back({ -1, s_phi });
                            }
                            // universal next: both branches must satisfy A(φ U ψ)
                            cont.literals.push_back({ L, state->name });
                            cont.literals.push_back({ R, state->name });
                            clauses.push_back(std::move(cont));
                        }

                        if (clauses.empty())
                           __addFalseTransition(state->name);
                        else
                            __addTrueTransition(state->name, clauses);
                        break;
                    }

                    // E(φ U ψ) : ψ  ∨  (φ ∧ EX self)  = ψ  ∨  (φ ∧ next(L,self)) ∨ (φ ∧ next(R,self))
                    case Op::EU: {
                        std::vector<Clause> clauses;

                        const auto& phi = *t->operand;
                        const auto& psi = *t->second_operand;

                        bool phi_true  = (phi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        std::dynamic_pointer_cast<BooleanLiteral>(t->operand)->value);
                        bool phi_false = (phi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        !std::dynamic_pointer_cast<BooleanLiteral>(t->operand)->value);
                        bool psi_true  = (psi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        std::dynamic_pointer_cast<BooleanLiteral>(t->second_operand)->value);
                        bool psi_false = (psi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        !std::dynamic_pointer_cast<BooleanLiteral>(t->second_operand)->value);
                        

                        //std::cout << "Handling E(φ U ψ) for state " << state->name << " with φ=" << phi.toString() << " and ψ=" << psi.toString() << "\n";
                        // Simplify special cases
                        if (psi_true) {  // E(φ U true) = true
                            __addTrueTransition(state->name);
                            break;
                        }
                        if (psi_false && phi_false) {  // E(false U false) = false
                           __addFalseTransition(state->name);
                            break;
                        }

                        if (phi_false && !psi_false) {
                            // E(false U ψ) = ψ
                            std::string_view s_psi = getStateOfFormula(psi);
                            clauses.push_back(Clause{ { { -1, s_psi } } });
                            __addTrueTransition(state->name, clauses);
                            break;
                        }

                        if(psi_false){
                            // E(φ U false) = false
                           __addFalseTransition(state->name);
                            break;
                        }

                        // (1) Immediate satisfaction: ψ
                        if (!psi_false) {
                            std::string_view s_psi = getStateOfFormula(psi);
                            clauses.push_back(Clause{ { { -1, s_psi } } });
                        }

                        // (2) Continuation: φ ∧ EX self
                        if (!phi_false) {
                            if (phi_true) {
                                // true ∧ EX self = EX self
                                clauses.push_back(Clause{ { { L, state->name } } });
                                clauses.push_back(Clause{ { { R, state->name } } });
                            } else {
                                std::string_view s_phi = getStateOfFormula(phi);
                                clauses.push_back(Clause{ { { -1, s_phi }, { L, state->name } } });
                                clauses.push_back(Clause{ { { -1, s_phi }, { R, state->name } } });
                            }
                        }

                        if (clauses.empty())
                           __addFalseTransition(state->name);
                        else
                            __addTrueTransition(state->name, clauses);
                        break;
                    }

                    case Op::AR: {
                    // A(φ R ψ)  ≡  ψ ∧ (φ ∨ AX self)

                    const auto& phi = t->operand;
                    const auto& psi = t->second_operand;
                    
                    // Handle Trivial Cases
                    bool psi_false = (psi->getType() == FormulaType::BOOLEAN_LITERAL &&
                                    !std::dynamic_pointer_cast<BooleanLiteral>(psi)->value);
                    if (psi_false) {
                        // A(φ R false) = false
                       __addFalseTransition(state->name);
                        break;
                    }

                    // Create the Helper State for the Disjunctive Part
                    // The helper state represents the sub-formula `(φ ∨ AX self)`.
                    
                    // NOTE: 't' is the shared_ptr to the current TemporalFormula A(φ R ψ).


                    auto helper_ax_formula = std::make_shared<TemporalFormula>(TemporalOperator::AX, t->clone());

                    auto helper_or_formula = std::make_shared<BinaryFormula>(
                        phi->clone(),
                        BinaryOperator::OR,
                        helper_ax_formula
                    );

                    std::string s_helper_ax_name = state->name + "_ax";
                    auto new_state_ax = std::make_shared<CTLState>(CTLState{ s_helper_ax_name, helper_ax_formula->clone() });
                    v_states_.push_back(new_state_ax);
                    formula_hash_to_state_cache_[helper_ax_formula->hash()] = new_state_ax->name;


                    std::string s_helper_or_name = state->name + "_or";
                    auto new_state_or = std::make_shared<CTLState>(CTLState{ s_helper_or_name, helper_or_formula->clone() });
                    v_states_.push_back(new_state_or);
                    formula_hash_to_state_cache_[helper_or_formula->hash()] = new_state_or->name;



                    worklist.push(new_state_ax);
                    worklist.push(new_state_or);

                    //  Build the Transition for the Current AR State
                    // This transition is a CNF, which we model as a DNF with a single conjunct.
                    // This is Player 2's move.
                    
                    Clause single_conjunct;

                    // 2a. Add the first obligation: ψ must hold.
                    std::string_view s_psi = getStateOfFormula(*psi);
                    single_conjunct.literals.push_back({ -1, s_psi });

                    // 2b. Add the second obligation: a move to the helper state.
                    single_conjunct.literals.push_back({ -1, new_state_or->name });

                    // 2c. Add the final transition for the AR state.
                    // It's a DNF with ONE disjunct (our single conjunct).
                    addTransition(state->name, GTrue, { std::move(single_conjunct) });

                    break;
}
                    // E(φ Ũ ψ) : ψ ∧ (φ ∨ EX self) = (ψ ∧ φ) ∨ (ψ ∧ next(L,self)) ∨ (ψ ∧ next(R,self))
                    case Op::ER: {
                        // E(φ Ũ ψ) = ψ ∧ (φ ∨ EX self)
                        std::vector<Clause> clauses;
                        
                        const auto& phi = *t->operand;
                        const auto& psi = *t->second_operand;

                        bool phi_true  = (phi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        std::dynamic_pointer_cast<BooleanLiteral>(t->operand)->value);
                        bool phi_false = (phi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        !std::dynamic_pointer_cast<BooleanLiteral>(t->operand)->value);
                        bool psi_true  = (psi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        std::dynamic_pointer_cast<BooleanLiteral>(t->second_operand)->value);
                        bool psi_false = (psi.getType() == FormulaType::BOOLEAN_LITERAL &&
                                        !std::dynamic_pointer_cast<BooleanLiteral>(t->second_operand)->value);

                        if (psi_false) {
                           __addFalseTransition(state->name);
                            break;
                        }
                        if (psi_true && phi_true) {
                            __addTrueTransition(state->name);
                            break;
                        }

                        // (ψ ∧ φ)
                        if (!psi_false && !phi_false) {
                            Clause conj1;
                            if (!psi_true) {
                                
                                std::string_view s_psi = getStateOfFormula(psi);
                                conj1.literals.push_back({ -1, s_psi });
                            }
                            if (!phi_true) {
                                
                                std::string_view s_phi = getStateOfFormula(phi);
                                conj1.literals.push_back({ -1, s_phi });
                            }
                            clauses.push_back(std::move(conj1));
                        }

                        // (ψ ∧ EX self)
                        if (!psi_false) {
                            if (psi_true) {
                                clauses.push_back(Clause{ { { L, state->name } } });
                                clauses.push_back(Clause{ { { R, state->name } } });
                            } else {
                                std::string_view s_psi = getStateOfFormula(psi);
                                clauses.push_back(Clause{ { { -1, s_psi }, { L, state->name } } });
                                clauses.push_back(Clause{ { { -1, s_psi }, { R, state->name } } });
                            }
                        }

                        if (clauses.empty())
                           __addFalseTransition(state->name);
                        else
                            __addTrueTransition(state->name, clauses);

                        break;
                    }

                    default:
                        throw std::runtime_error("Non-core temporal op in builder: " + state->name + " : " + state->formula->toString());
                }
            
                break;
              }

            default:
                throw std::runtime_error("Unhandled node in builder: " + state->name + " of type " + formula_utils::formulaTypeToString(t));
                break; // continue to the error below
        }

    }
  }



    void CTLAutomaton::__decideBlockTypes() const {
        // loop through each block, get the state and decide its type
        for (size_t i = 0; i < blocks_->blocks.size(); ++i) {
            // Check for μ/ν
            bool acceptance_for_this_block = false;
            std::vector<SCCAcceptanceType> acceptance_in_block; // refers to SCCAcceptanceType enum, which is nu or mu
            std::vector<SCCBlockType> block_types_in_block;
            //std::vector<SCCAcceptanceType> acceptance_in_block;

            
            for (size_t j = 0; j < blocks_->blocks[i].size(); ++j) {
                const auto& state_name = blocks_->blocks[i][j];
                auto it = std::find_if(v_states_.begin(), v_states_.end(),
                                       [&state_name](const CTLStatePtr& s) {
                                           return s->name == state_name;
                                       });
                if (it != v_states_.end()) {
                    acceptance_in_block.push_back(formula_utils::getBlockAcceptanceTypeFromFormula((*it)->formula));
                    block_types_in_block.push_back(formula_utils::getSCCBlockTypeFromFormula((*it)->formula));
                }
                
            }


            // Decide block type
            SCCAcceptanceType final_acceptance_type = SCCAcceptanceType::UNDEFINED;
            SCCBlockType final_block_type = SCCBlockType::UNDEFINED;
            // if types are the same, assign that type
            if (!acceptance_in_block.empty()) {
                final_acceptance_type = acceptance_in_block[0];
                bool all_same = std::all_of(acceptance_in_block.begin(), acceptance_in_block.end(),
                                            [final_acceptance_type](SCCAcceptanceType t) { return t == final_acceptance_type; });
                if (!all_same) std::cerr << "Warning: Mixed block types in block " << i << "\n";
                final_block_type = block_types_in_block[0];
                all_same = std::all_of(block_types_in_block.begin(), block_types_in_block.end(),
                                            [final_block_type](SCCBlockType t) { return t == final_block_type; });
                if (!all_same) std::cerr << "Warning: Mixed existential/universal types in block " << i << "\n";
            }else throw std::runtime_error("Error: Empty block found when deciding block types.");
            // Check for acceptance
            
            blocks_->setInfoAt(i, final_acceptance_type, final_block_type);
            if(blocks_->isGreatestFixedPoint(i))
                s_accepting_states_.insert(blocks_->blocks[i].begin(), blocks_->blocks[i].end());
        }
    }



  std::vector<std::vector<std::string_view>> CTLAutomaton::__computeSCCs() const {

      std::vector<std::string_view> V; V.reserve(state_successors_.size());
      std::unordered_map<std::string_view, int> idx; idx.reserve(state_successors_.size());

      V.reserve(state_successors_.size());
      for (const auto& kv : state_successors_) {
          idx.emplace(kv.first, static_cast<int>(V.size()));
          V.push_back(kv.first);
      }
      if (!idx.count(initial_state_)) {
          // In case the initial state has no registered successors yet
          idx.emplace(initial_state_, static_cast<int>(V.size()));
          V.push_back(initial_state_);
      }

      const int n = static_cast<int>(V.size());
      std::vector<int> index(n, -1), low(n, 0);
      std::vector<char> onstack(n, 0);
      std::stack<int> st;
      int timer = 0;
      std::vector<std::vector<std::string_view>> sccs;

      std::function<void(int)> dfs = [&](int v){
          index[v] = low[v] = timer++;
          st.push(v); onstack[v] = 1;

          const std::string_view sv = V[v];
          auto it = state_successors_.find(sv);
          if (it != state_successors_.end()) {
              for (const auto& succ : it->second) {
                  auto jt = idx.find(succ);
                  if (jt == idx.end()) continue; // unknown state (shouldn't happen)
                  int w = jt->second;
                  if (index[w] == -1) {
                      dfs(w);
                      low[v] = std::min(low[v], low[w]);
                  } else if (onstack[w]) {
                      low[v] = std::min(low[v], index[w]);
                  }
              }
          }

          if (low[v] == index[v]) {
              // root of SCC
              std::vector<std::string_view> comp;
              while (true) {
                  int w = st.top(); st.pop(); onstack[w] = 0;
                  comp.push_back(std::string_view(V[w]));
                  if (w == v) break;
              }

              sccs.push_back(comp);
          }
      };

      for (int v = 0; v < n; ++v) {
          if (index[v] == -1) dfs(v);
      }
      return sccs;
  }




    

}