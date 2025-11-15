#pragma once

#include "types.h"
#include "refinement_graph.h"
#include <chrono>
#include <vector>
#include <memory>

namespace ctl {

struct AnalysisResult {
    size_t total_properties;
    size_t equivalence_classes;
    size_t total_refinements;
    size_t required_properties = 0;  // Properties with in-degree 0 (not refined by others)
    std::vector<RefinementGraph> class_graphs;
    std::vector<std::vector<std::shared_ptr<CTLProperty>>> equivalence_class_properties;
    std::chrono::milliseconds parsing_time;
    std::chrono::milliseconds equivalence_time;
    std::chrono::milliseconds refinement_time;
    std::chrono::milliseconds total_time;
    
    // Transitive optimization results
    size_t transitive_eliminated = 0;
    size_t false_properties = 0;

    size_t peak_memory_kb = 0;
    size_t refinement_memory_kb = 0;
    size_t total_analysis_memory_kb = 0;
};

} // namespace ctl
