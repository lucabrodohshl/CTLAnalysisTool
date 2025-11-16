#pragma once
#include "types.h"
#include "analysis_result.h"
#include "Factories/externalSatFactory.h"
#include <vector>
#include <memory>
#include <thread>
#include <string>

namespace ctl{
    class Analyzer {
    public:
            Analyzer()=default; 
            virtual ~Analyzer() = default;
            virtual AnalysisResult analyze() = 0;
            void setParallelAnalysis(bool enabled) { use_parallel_analysis_ = enabled; }
            void setThreads(size_t threads) { threads_ = threads; }
            void setExternalSATInterface(AvailableCTLSATInterfaces interface_type, const std::string& sat_path) {
                external_sat_interface_set_ = true;
                external_sat_interface_ = ExternalSatFactory::createExternalSATInterface(interface_type, sat_path);
            }
            //return reference to pointer to external sat interface
            const ExternalCTLSATInterface* getExternalSATInterface() const {
                if (!external_sat_interface_set_) {
                    return nullptr;
                }
                return external_sat_interface_.get();
            }
            void setVerbose(bool verbose) { 
                verbose_ = verbose; 
                if (external_sat_interface_) {
                    external_sat_interface_->setVerbose(verbose);
                }
                for (auto& property : properties_) {
                    property->setVerbose(verbose);
                }
            }
            bool isVerbose() const { return verbose_; }



    protected:

        void __initialize_properties(const std::vector<std::string>& property_strings)
        {
            properties_.reserve(property_strings.size());
            for (const auto& prop_str : property_strings) {
                try {
                    properties_.push_back(CTLProperty::create(prop_str, verbose_));
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Failed to parse property '" << prop_str 
                              << "': " << e.what() << std::endl;
                }
            }

        }

        bool use_parallel_analysis_ = true;
        bool external_sat_interface_set_ = false;
        bool verbose_ = false;
        size_t threads_ = std::thread::hardware_concurrency();
        std::unique_ptr<ExternalCTLSATInterface> external_sat_interface_;
        std::vector<std::shared_ptr<CTLProperty>> properties_;
        std::vector<PropertyResult> result_per_property_;

    };

}; // namespace ctl
