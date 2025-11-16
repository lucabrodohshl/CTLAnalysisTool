#include "ExternalCTLSAT/mlsolver_sat.h"
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <array>
#include <memory>
#include <regex>
#include <algorithm>

#include "sat_parsers/ctlsat_parser.h"

namespace ctl {

MLSolverInterface::MLSolverInterface(const std::string& mlsolver_sat_path) 
    {
    sat_path_ = mlsolver_sat_path;
     }

std::string MLSolverInterface::runMLSolver(const std::string& formula) const {
    if (verbose_) std::cout << "Running: "<< formula << "\n";
    // MLSolver outputs to stderr, so redirect stderr to stdout
    std::string command = sat_path_ + " --satisfiability ctl \"" + formula + "\" --pgsolver recursive 2>&1";
    if (verbose_) std::cout << "Command: " << command << "\n";
    std::array<char, 2048*4> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to run MLSolver command: " + command);
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

bool MLSolverInterface::isSatisfiable(const std::string& formula, bool with_clearing) const {
    if (with_clearing) MLSolverParser::clearComparisonMapping();
    try {
        std::cout << "checking for " << formula << "\n";
        if(verbose_) std::cout << "Satisfiability check for formula: " << formula << "\n";
        std::string mlsolver_formula = toMLSolverFormat(formula);
        std::cout << "Converted formula: " << mlsolver_formula << "\n";
        std::string output = runMLSolver(mlsolver_formula);

        //std::cout << "Output: " << output << "\n" << std::endl;
        // CTL-SAT returns "SAT" for satisfiable, "UNSAT" for unsatisfiable
        //std::cout << (output.find("Input formula is satisfable") != std::string::npos); //&& output.find("UNSAT") == std::string::npos;
        //std::cout << output;
        if (output.find("Formula is satisfiable!") != std::string::npos) {
            return true;
        } else if (output.find("Formula is unsatisfiable!") != std::string::npos) {
            return false;
        }
        throw std::runtime_error("Unexpected CTL-SAT output: " + output);
    } catch (const std::exception& e) {
        std::cerr << "Exception in isSatisfiable: " << e.what() << "\n";
        return false; // Assume unsatisfiable if there's an error
    }
}


// WE CHECK FOR RefinesPlus
bool MLSolverInterface::refines(const std::string& formula1, const std::string& formula2) const {
    // Clear comparison mapping to ensure consistent atom assignment across both formulas
    MLSolverParser::clearComparisonMapping();
    
    // Check if ¬(formula1 ∧ ¬formula2) is unsatisfiable
    // This is equivalent to checking if formula1 → formula2
    if (verbose_) std::cout << "Refinement check: " << formula1 << " -> " << formula2 << "\n";

    //if(!isSatisfiable(formula1)) {
    //    if (verbose_) std::cout << "Formula 1 is unsat\n\n\n\n\n";
    //    return false; //unsat formula refines nothing
    //}

    // Build implication test in CTLSAT format
    //std::string implication_test = "(" + toCTLSATSyntax(formula1) + "^ ~(" + toCTLSATSyntax(formula2) + ")))";
    //if (verbose_) std::cout << "Implication test formula: " << implication_test << "\n";

    // Run CTLSAT directly on the already-converted formula
    bool refines =!isSatisfiable(std::string(formula1) + " & !(" + std::string(formula2) + ")", false);
    if (verbose_) {
        if (refines) {
            std::cout << "Result: " << formula1 << " refines " << formula2 << "\n\n\n\n\n";
        } else {
            std::cout << "Result: " << formula1 << " does NOT refine " << formula2 << "\n\n\n\n\n";
        }
    }
    return refines;
    //try {
    //    std::string output = runMLSolver(implication_test);
    //    std::cout << "Implication test output: " << output << "\n";
    //    return output.find("Input formula is satisfable") == std::string::npos;  
    //} catch (const std::exception& e) {
    //    std::cerr << "Exception checking implication: " << e.what() << "\n";
    //    return false;
    //}
}

bool MLSolverInterface::implies(const std::string& formula1, const std::string& formula2) const {
    // Clear comparison mapping to ensure consistent atom assignment
    MLSolverParser::clearComparisonMapping();
    
    // Check if ¬(formula1 ∧ ¬formula2) is unsatisfiable
    // This is equivalent to checking if formula1 → formula2
    std::string implication_test = "~(" + toMLSolverFormat(formula1) + "^~(" + toMLSolverFormat(formula2) + "))";
    return !isSatisfiable(implication_test);
}

bool MLSolverInterface::equivalent(const std::string& formula1, const std::string& formula2) const {
    return implies(formula1, formula2) && implies(formula2, formula1);
}

std::string MLSolverInterface::toMLSolverFormat(const std::string& ctl_formula) {
    // Use MLSolverParser to convert formula
    try {
        return MLSolverParser::convertString(ctl_formula);
    } catch (const std::exception& e) {
        std::cerr << "Exception in toMLSolverFormat for formula: " << ctl_formula << "\n";
        std::cerr << "Error: " << e.what() << "\n";
        throw;
    }
}

} // namespace ctl