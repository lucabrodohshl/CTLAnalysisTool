#pragma once

#include <string>
#include <vector>
#include <memory>


#include "ExternSATInterface.h"

namespace ctl {

class CTLSATInterface: public ExternalCTLSATInterface {
    
public:
    explicit CTLSATInterface(const std::string& sat_path = "./extern/ctl-sat");
    
    // Check if formula is satisfiable
    bool isSatisfiable(const std::string& formula, bool with_clearing=false) const override;

    // Check if formula1 refines formula2 (formula1 -> formula2)
    bool refines(const std::string& formula1, const std::string& formula2) const override;
    bool implies(const std::string& formula1, const std::string& formula2) const override;
    // Check if formula1 is equivalent to formula2
    bool equivalent(const std::string& formula1, const std::string& formula2) const override;
    
    // Convert CTL formula to CTL-SAT syntax
    static std::string toCTLSATSyntax(const std::string& ctl_formula);
    
    // Run CTL-SAT with given formula and return output
    std::string runCTLSAT(const std::string& formula) const;

    
};

} // namespace ctl