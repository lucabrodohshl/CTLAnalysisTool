#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <iostream>
#include <memory>

namespace ctl {

struct Literal { int dir; std::string_view qnext; };    // (dir, q')
struct Clause {  std::vector<Literal> literals; };       // ∧ of literals
using FromToPair = std::pair<std::string_view, std::string_view>;




#ifdef USE_Z3
using sat_expr= z3::expr; ;
#endif
#ifdef USE_CVC5
using sat_expr = cvc5::Term;
#endif



struct Guard{
    std::string pretty_string;
    void* sat_expr;
    Guard() {
        pretty_string= "";
        sat_expr= NULL;
    }
    Guard(const std::string& ps) : pretty_string(ps) {sat_expr= NULL; }
    std::string toString() const {
        return pretty_string;
    }
    std::string toSatString() const {
        return "";
    }
};

// overload == operator for Guard
inline bool operator==(const Guard& a, const Guard& b) {
    return std::hash<std::string>{}(a.pretty_string) == std::hash<std::string>{}(b.pretty_string);
}


//override string conversion for Guard
inline std::ostream& operator<<(std::ostream& os, const Guard& g) {
    os << g.toString();
    return os;
}
//override + operator for string and Guard
inline std::string operator+(const std::string& s, const Guard& g) {
    return s + g.toString();
}



struct CTLTransition { 
        Guard guard; 
        std::string_view from;
        std::vector<Clause> clauses;
        bool is_dnf; // guard ∧ (∨ Clause) or // guard ∧ (∧ Disj) if CNF
        CTLTransition() = default;
}; 



using CTLTransitionPtr = std::shared_ptr<CTLTransition>;

}