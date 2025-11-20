#include "../include/CTLautomaton.h"
#include "../include/formula.h"
#include "../include/property.h"
#include <iostream>

using namespace ctl;


std::shared_ptr<CTLProperty> makeProperty(const std::string& formula_str) {
    auto p = std::make_shared<CTLProperty>(formula_str, false);
    p->setVerbose(true);    
    //p->simplifyABTA();
    //if (p->isEmpty()) {
    //    std::cerr << "Warning: '" << formula_str << "' yields empty ABTA.\n";
    //}
    return p;
}


int main() {
    try {
        // Test 1: Simple AG formula
        std::cout << "=== Test 1: AG(x >= 4) & EF(false)===" << std::endl;
        auto p = makeProperty("AG(x >= 4) & EF(false)");
        p->automaton().print();
        p->automaton().buildGameGraph().print();
        bool result1 = p->automaton().checkCtlSatisfiability();
        std::cout << "\nResult: " << (result1 ? "SATISFIABLE" : "UNSATISFIABLE") << "\n\n";

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
