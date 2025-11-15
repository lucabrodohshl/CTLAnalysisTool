#include "Analyzers/SAT.h"
#include "utils.h"
#include "memory_tracker.h"
#include <chrono>
#include <thread>
namespace ctl {



    AnalysisResult SATAnalyzer::analyze() {
        auto start_time = std::chrono::high_resolution_clock::now();
        auto mem_initial = memory_utils::getCurrentMemoryUsage();
        AnalysisResult result;
        result.total_properties = properties_.size();
        result.false_properties = 0;
        auto parse_end = std::chrono::high_resolution_clock::now();
        result.parsing_time = std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - start_time);
        //create all the abtas before hand. If the abta is empty, we remove the property!
        for (size_t index = 0; index < properties_.size(); index++) {
            auto prop_result = checkSAT(*properties_[index]);
            prop_result.property1_index = index;
            if (!prop_result.passed) {
                false_properties_strings_.push_back(properties_[index]->toString());
                false_properties_index_.push_back(index);
                result.false_properties++;
            }
            
            result_per_property_.push_back(prop_result);
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto mem_final = memory_utils::getCurrentMemoryUsage();
        result.total_analysis_memory_kb = (mem_final.resident_memory_kb > mem_initial.resident_memory_kb)
                                        ? (mem_final.resident_memory_kb - mem_initial.resident_memory_kb)
                                        : 0;
        return result;

    }


    PropertyResult SATAnalyzer::checkSAT(const CTLProperty& property) const {
        //std::cout << "Checking satisfiability for property: " << property.toString() << std::endl;
        auto mem_before = memory_utils::getCurrentMemoryUsage();
        auto start_time = std::chrono::high_resolution_clock::now();
        bool is_sat;
        if (external_sat_interface_set_) {
            is_sat = property.isSatisfiable(*external_sat_interface_);
        } else {
             is_sat = property.isSatisfiable();
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto mem_after = memory_utils::getCurrentMemoryUsage();
        size_t mem_delta = (mem_after.resident_memory_kb > mem_before.resident_memory_kb) 
                          ? (mem_after.resident_memory_kb - mem_before.resident_memory_kb) 
                          : 0;
        return {is_sat, 
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time),
                0, 0, mem_delta};
    }
}