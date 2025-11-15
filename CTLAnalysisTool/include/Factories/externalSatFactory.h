#pragma once

#include "ExternSATInterface.h"
#include "ExternalCTLSAT/ctl_sat.h"
#include <memory>
#include <stdexcept>
#include "types.h"
namespace ctl {
    class ExternalSatFactory {
    public:
        
        static std::unique_ptr<ExternalCTLSATInterface> createExternalSATInterface(AvailableCTLSATInterfaces interface_type, const std::string& sat_path) {
            switch (interface_type) {
                case AvailableCTLSATInterfaces::CTLSAT:
                    return std::make_unique<CTLSATInterface>(sat_path);
                case AvailableCTLSATInterfaces::MOMOCTL:
                    //return std::make_unique<MOMOCTLInterface>(sat_path);
                case AvailableCTLSATInterfaces::MLSOLVER:
                    //return std::make_unique<MLSolverInterface>(sat_path);
                case AvailableCTLSATInterfaces::NONE:
                default:
                    throw std::invalid_argument("Invalid or unsupported SAT interface type.");
            }
        }
    };
} // namespace ctl