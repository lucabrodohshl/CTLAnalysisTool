
#include "Analyzers/Refinement.h"
#include "CTLautomaton.h"
#include "utils.h"

#include <chrono>
#include <algorithm>
#include <future>
#include <atomic>
#include <mutex>
#include <numeric>
#include <iomanip>

namespace ctl {


AnalysisResult RefinementAnalyzer::analyze() {
    auto start_time = std::chrono::high_resolution_clock::now();
    auto mem_initial = memory_utils::getCurrentMemoryUsage();
    AnalysisResult result;
    result.total_properties = properties_.size();
    //result.initial_memory_mb = mem_initial.getResidentMB();
    // Parse time (already done in constructor, so this is 0)
    auto parse_end = std::chrono::high_resolution_clock::now();
    result.parsing_time = std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - start_time);
    
    
    result.false_properties = 0;
    if (use_parallel_analysis_) {
        std::cout << "Checking and removing unsatisfiable properties in parallel...\n";
        _checkAndRemoveUnsatisfiableProperties();
    } else {
        std::cout << "Checking and removing unsatisfiable properties serially...\n";
        _checkAndRemoveUnsatisfiableProperties();
    }

    
    result.false_properties = false_properties_strings_.size();
    // Build equivalence classes
    auto equiv_start = std::chrono::high_resolution_clock::now();
    buildEquivalenceClasses();
    auto equiv_end = std::chrono::high_resolution_clock::now();
    result.equivalence_time = std::chrono::duration_cast<std::chrono::milliseconds>(equiv_end - equiv_start);
    result.equivalence_classes = equivalence_classes_.size();
    result.equivalence_class_properties = equivalence_classes_;
    
    





    // Analyze refinements
    auto refine_start = std::chrono::high_resolution_clock::now();
    auto mem_refine_start = memory_utils::getCurrentMemoryUsage();
    if (use_parallel_analysis_) {
        if (use_transitive_optimization_) {
            std::cout << "Analyzing refinements in parallel with transitive optimization in parallel...\n";
            analyzeRefinementsParallelOptimized();
        } else {
            std::cout << "Analyzing refinements in parallel without transitive optimization...\n";
            analyzeRefinementClassParallel();
        }
    } else {
        std::cout << "Analyzing refinements serially...\n";
        analyzeRefinements();
    }
    auto mem_refine_end = memory_utils::getCurrentMemoryUsage();
    auto refine_end = std::chrono::high_resolution_clock::now();
    result.refinement_time = std::chrono::duration_cast<std::chrono::milliseconds>(refine_end - refine_start);
    result.refinement_memory_kb = mem_refine_end.getResident() - mem_refine_start.getResident();
    // Calculate total refinements
    result.total_refinements = 0;
    for (const auto& graph : refinement_graphs_) {
        result.total_refinements += graph.getEdgeCount();
    }
    
    // Calculate required properties (those with in-degree 0)
    auto required = getRequiredProperties();
    result.required_properties = required.size();
    
    // Apply transitive optimization if enabled (placeholder for now)
    if (use_transitive_optimization_) {
        result.transitive_eliminated = total_skipped_; // Placeholder
    }else{
        result.transitive_eliminated = -1;
    }
    
    result.class_graphs = refinement_graphs_;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto mem_final = memory_utils::getCurrentMemoryUsage();
    result.total_analysis_memory_kb = (mem_final.resident_memory_kb > mem_initial.resident_memory_kb)
                                       ? (mem_final.resident_memory_kb - mem_initial.resident_memory_kb)
                                       : 0;
    result.peak_memory_kb = memory_utils::getPeakMemoryUsage();

    return result;
}

void RefinementAnalyzer::buildEquivalenceClasses() {
    if (properties_.empty()) {
        return;
    }
    
    UnionFind uf;
    
    // Initialize all properties in the union-find structure
    for (size_t i = 0; i < properties_.size(); ++i) {
        uf.find(i); // This ensures the element is tracked
    }
    
    // Build a map from atomic propositions to property indices
    // Properties that share at least one atomic proposition should be in the same class
    std::unordered_map<std::string, std::vector<size_t>> atom_to_properties;
    
    for (size_t i = 0; i < properties_.size(); ++i) {
        //if property in false_properties_index skip it
        bool skip = false;
        for (const auto& index : false_properties_index_) {
            if (i == index) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        const auto& atoms = properties_[i]->getAtomicPropositions();
        for (const auto& atom : atoms) {
            // Skip numeric literals - they're not meaningful for grouping
            if (atom == "true" || atom == "false" || atom == "1" || atom == "0") {
                continue;
            }
            // Skip if it's a pure number
            try {
                std::stod(atom);
                continue; // It's a number, skip it
            } catch (...) {
                // Not a number, it's a valid atom
            }
            atom_to_properties[atom].push_back(i);
        }
    }
    
    // Unite all properties that share an atomic proposition
    for (const auto& [atom, prop_indices] : atom_to_properties) {
        // Unite all properties that share this atom
        for (size_t i = 1; i < prop_indices.size(); ++i) {
            uf.unite(prop_indices[0], prop_indices[i]);
        }
    }
    
    // Build equivalence classes
    auto classes = uf.getEquivalenceClasses();
    equivalence_classes_.clear();
    equivalence_classes_.reserve(classes.size());
    
    for (const auto& class_indices : classes) {
        std::vector<std::shared_ptr<CTLProperty>> class_properties;
        class_properties.reserve(class_indices.size());
        
        for (size_t idx : class_indices) {
            class_properties.push_back(properties_[idx]);
        }
        
        equivalence_classes_.push_back(std::move(class_properties));
    }
}

void RefinementAnalyzer::analyzeRefinements() {
    refinement_graphs_.clear();
    refinement_graphs_.reserve(equivalence_classes_.size());
    
    for (size_t i = 0; i < equivalence_classes_.size(); ++i) {
        std::cout << "Analyzing refinement class " << (i + 1) << "/" << equivalence_classes_.size() << "...\n";
        _analyzeRefinementClassSerial(i, use_transitive_optimization_);

    }
}

void RefinementAnalyzer::_analyzeRefinementClassSerial(size_t class_index, bool use_transitive) {
    const auto& class_properties = equivalence_classes_[class_index];
    
    RefinementGraph graph;
    
    // Add all properties as nodes
    for (const auto& prop : class_properties) {
        graph.addNode(prop);
    }
    
    size_t n = class_properties.size();
    size_t skipped_pairs = 0;
    
    // Check all pairs for refinement
    size_t total_operations = n * (n - 1);
    size_t completed_operations = 0;
    int last_printed_percent = -1;
    
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            
            // OPTIMIZATION: Skip if already reachable (transitive closure)
            if (use_transitive && graph.hasEdge(i, j)) {
                skipped_pairs++;
                completed_operations++;
                continue;
            }
            
            PropertyResult result = checkRefinement(*class_properties[i], *class_properties[j]);
            result.property1_index = i;
            result.property2_index = j;
            if (result.passed) {
                graph.addEdge(i, j);
                
                // TRANSITIVE CLOSURE: If i->j, then i can reach everything j can reach
                if (use_transitive) {
                    for (size_t k = 0; k < n; ++k) {
                        if (graph.hasEdge(j, k)) {
                            graph.addEdge(i, k);
                        }
                    }
                }
            }
            result_per_property_.push_back(result);
            completed_operations++;
            
            // Update progress bar every 5%
            int current_percent = (completed_operations * 100) / total_operations;
            if (current_percent >= last_printed_percent + 5) {
                last_printed_percent = current_percent;
                printProgressBar(current_percent, class_index + 1, equivalence_classes_.size());
            }
        }
    }
    
    // Print optimization statistics
    if (use_transitive && skipped_pairs > 0) {
        size_t total_pairs = n * (n - 1);
        double skip_ratio = (total_pairs > 0) ? (100.0 * skipped_pairs / total_pairs) : 0.0;
        std::cout << "    [Transitive Closure] Skipped " << skipped_pairs << "/" << total_pairs 
                  << " pairs (" << std::fixed << std::setprecision(1) << skip_ratio << "%)" << std::endl;
        
    }
    total_skipped_ += skipped_pairs;
    
    refinement_graphs_.push_back(std::move(graph));
}

PropertyResult RefinementAnalyzer::checkRefinement(const CTLProperty& prop1, const CTLProperty& prop2) const {
    auto mem_before = memory_utils::getCurrentMemoryUsage();
    auto start_time = std::chrono::high_resolution_clock::now();
    bool res;
    if (!external_sat_interface_set_) {
        // Use existing refinement methods
        res = prop1.refines(prop2, use_syntactic_refinement_, use_full_language_inclusion_);
    } else {
        // Use CTL-SAT for refinement checking
        res = prop1.refines(prop2, *external_sat_interface_);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto mem_after = memory_utils::getCurrentMemoryUsage();
    size_t mem_delta = (mem_after.resident_memory_kb > mem_before.resident_memory_kb) 
                      ? (mem_after.resident_memory_kb - mem_before.resident_memory_kb) 
                      : 0;
    return {res, 
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time),
            0, 0, mem_delta};

}

void RefinementAnalyzer::analyzeRefinementClassParallel() {
    auto futures = createAnalysisTasks();
    
    refinement_graphs_.clear();
    refinement_graphs_.resize(equivalence_classes_.size());
    
    // Collect results
    for (size_t i = 0; i < futures.size(); ++i) {
        refinement_graphs_[i] = futures[i].get();
    }
}

void RefinementAnalyzer::analyzeRefinementsParallelOptimized() {
    refinement_graphs_.clear();
    refinement_graphs_.reserve(equivalence_classes_.size());

    // Run sequentially over classes (inner level already parallel)
    int class_count = 0;
    for (const auto& eq_class : equivalence_classes_) {
        refinement_graphs_.push_back(_analyzeClassTaskOptimized(eq_class));
        std::cout << "Equivalence class analyzed. ("<< ++class_count << "/" << equivalence_classes_.size() << ")\n";
    }
}


//void RefinementAnalyzer::analyzeRefinementsParallelOptimized() {
//    refinement_graphs_.clear();
//    refinement_graphs_.reserve(equivalence_classes_.size());
//    
//    // Process each class with the optimized task
//    std::vector<std::future<RefinementGraph>> futures;
//    futures.reserve(equivalence_classes_.size());
//    
//    for (size_t i = 0; i < equivalence_classes_.size(); ++i) {
//        futures.push_back(
//            std::async(std::launch::async,
//                      &RefinementAnalyzer::_analyzeClassTaskOptimized, this,
//                      std::cref(equivalence_classes_[i])));
//    }
//    
//    // Collect results
//    for (auto& future : futures) {
//        refinement_graphs_.push_back(future.get());
//    }
//}

std::vector<std::future<RefinementGraph>> RefinementAnalyzer::createAnalysisTasks() {
    std::vector<std::future<RefinementGraph>> futures;
    futures.reserve(equivalence_classes_.size());
    
    // Create a thread pool
    size_t num_threads = threads_;
    
    for (size_t i = 0; i < equivalence_classes_.size(); ++i) {
        futures.push_back(
            std::async(std::launch::async, 
                      &RefinementAnalyzer::_analyzeClassTask, this, 
                      std::cref(equivalence_classes_[i])));
    }
    
    return futures;
}

RefinementGraph RefinementAnalyzer::_analyzeClassTask(const std::vector<std::shared_ptr<CTLProperty>>& class_properties) {
    RefinementGraph graph;
    
    // Add all properties as nodes
    for (const auto& prop : class_properties) {
        graph.addNode(prop);
    }
    
    // Check all pairs for refinement
    for (size_t i = 0; i < class_properties.size(); ++i) {
        for (size_t j = 0; j < class_properties.size(); ++j) {
            if (i != j) {
                PropertyResult result = checkRefinement(*class_properties[i], *class_properties[j]);
                result.property1_index = i;
                result.property2_index = j;
                if (result.passed) {
                    graph.addEdge(i, j);
                }
                result_per_property_.push_back(result);
            }
        }
    }


    
    return graph;
}

// Optimized version with property-level parallelization and transitive closure
RefinementGraph RefinementAnalyzer::_analyzeClassTaskOptimized(const std::vector<std::shared_ptr<CTLProperty>>& class_properties) {
    RefinementGraph graph;
    
    // Add all properties as nodes
    for (const auto& prop : class_properties) {
        graph.addNode(prop);
    }
    
    size_t n = class_properties.size();
    if (n <= 1) {
        return graph;
    }

    // Simple reachability array - using std::atomic<bool> for thread safety
    std::unique_ptr<std::atomic<bool>[]> reachability(new std::atomic<bool>[n * n]);
    for (size_t i = 0; i < n * n; ++i) {
        reachability[i].store(false, std::memory_order_relaxed);
    }
    
    // Lambda to access reachability[i][j]
    auto reach = [&](size_t i, size_t j) -> std::atomic<bool>& {
        if (i >= n || j >= n) {
            throw std::out_of_range("reachability index out of range");
        }
        return reachability[i * n + j];
    };
    
    std::atomic<size_t> skipped_pairs(0);
    std::mutex update_mutex;  // Protect transitive closure updates
    std::mutex read_mutex;
    // Process all pairs in parallel - check refinements and update reachability
    std::vector<std::future<std::vector<PropertyResult>>> futures;
    
    std::cout << "    [Refinement Class] Analyzing " << n << " properties with " << threads_ << " threads...\n";
    auto num_threads = threads_;
    for (size_t t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t, num_threads]() -> std::vector<PropertyResult> {
            std::vector<PropertyResult> results_per_thread;
            // Each thread handles rows: t, t+threads_, t+2*threads_, ...
            for (size_t i = t; i < n; i += num_threads) {
                for (size_t j = 0; j < n; ++j) {
                    if (i == j) continue;
                    
    
                        if (reach(i, j).load(std::memory_order_acquire)) {
                            skipped_pairs.fetch_add(true, std::memory_order_relaxed);
                            continue;
                        }
                    
                    
                    // Actually test refinement
                    PropertyResult result = checkRefinement(*class_properties[i], *class_properties[j]);
                    result.property1_index = i;
                    result.property2_index = j;
                    results_per_thread.push_back(result);
                    if (result.passed) {
                        // Mark direct reachability
                        reach(i, j).store(1, std::memory_order_release);
                        
                        // TRANSITIVE CLOSURE: If i->j, then i can reach everything j can reach
                        
                            for (size_t k = 0; k < n; ++k) {
                                if (reach(j, k).load(std::memory_order_relaxed)) {
                                    reach(i, k).store(true, std::memory_order_relaxed);
                                }
                            }
                        
                    }
                }
            }
            return results_per_thread;
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        auto results = future.get();
        result_per_property_.insert(result_per_property_.end(), results.begin(), results.end());
    }
    
    // Print optimization statistics
    if (use_transitive_optimization_) {
        size_t total_pairs = n * (n - 1);
        size_t skipped = skipped_pairs.load();
        if (skipped > 0) {
            double skip_ratio = (total_pairs > 0) ? (100.0 * skipped / total_pairs) : 0.0;
            std::cout << "    [Transitive Closure] Skipped " << skipped << "/" << total_pairs 
                      << " pairs (" << std::fixed << std::setprecision(1) << skip_ratio << "%)" << std::endl;
        }
        total_skipped_ += skipped;
    }
    
    // Now build the graph from the reachability matrix (single-threaded, no race conditions)
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (reach(i, j).load(std::memory_order_acquire)) {
                graph.addEdge(i, j);
            }
        }
    }
    
    return graph;
}


    void RefinementAnalyzer::_checkAndRemoveUnsatisfiableProperties() {

        for (auto it = properties_.begin(); it != properties_.end(); ) {
            bool is_false = false;

            if (!external_sat_interface_) {
                // Simplify and check if ABTA is empty
                (*it)->simplify();
                is_false = (*it)->isEmpty();
            } else {
                // Use CTL-SAT to check for unsatisfiability
                is_false = !(*it)->isSatisfiable(*external_sat_interface_);//!ctl_sat_interface_->isSatisfiable((*it)->toString(), true);
            }

            if (is_false) {
                std::cerr << "Property " << (*it)->toString() << " is unsatisfiable and will be removed from analysis.\n";
                false_properties_strings_.push_back((*it)->toString());
                false_properties_index_.push_back(std::distance(properties_.begin(), it));
                it = properties_.erase(it); // Remove property if unsatisfiable
            } else {
                ++it; // Move to next property
            }
        }

        //if (!use_ctl_sat_){
        //    //create all the abtas before hand. If the abta is empty, we remove the property!
        //    for (auto it = properties_.begin(); it != properties_.end(); ) {
        //        it->get()->simplify();
        //        if(it->get()->isEmpty()) {
        //            std::cerr << "Property " << it->get()->toString() << " has an empty ABTA and will be removed from analysis.\n";
        //            false_properties_strings_.push_back(it->get()->toString());
        //            false_properties_index_.push_back(std::distance(properties_.begin(), it));
        //            it = properties_.erase(it); // Remove property if ABTA creation fails
        //        } else {
        //            ++it; // Move to next property
        //        }
        //    }
        //}else{
//
        //    for (auto it = properties_.begin(); it != properties_.end(); ) {
        //        // Use CTL-SAT to check for unsatisfiability
        //        std::cout << "Checking property for unsatisfiability using CTL-SAT: " << it->get()->toString() << std::endl;
        //        bool is_unsat = ctl_sat_interface_->isSatisfiable(it->get()->toString(),true) == false;
        //        if (is_unsat) {
        //            std::cerr << "Property " << it->get()->toString() << " is unsatisfiable according to CTL-SAT and will be removed from analysis.\n";
        //            false_properties_strings_.push_back(it->get()->toString());
        //            false_properties_index_.push_back(std::distance(properties_.begin(), it));
        //            it = properties_.erase(it); // Remove property if unsatisfiable
        //        } else {
        //            ++it; // Move to next property
        //        }
        //    }
        //}
    }


    void RefinementAnalyzer::_checkAndRemoveUnsatisfiablePropertiesParallel() {
        if (properties_.empty()) return;
        
        size_t n = properties_.size();
        std::vector<std::future<std::vector<size_t>>> futures;
        
        // Launch parallel tasks
        for (size_t t = 0; t < threads_; ++t) {
            futures.push_back(std::async(std::launch::async, [this, t, n]() -> std::vector<size_t> {
                std::vector<size_t> false_indices;
                
                // Each thread handles indices: t, t+threads_, t+2*threads_, ...
                for (size_t i = t; i < n; i += threads_) {
                    bool is_false = false;
                    
                    if (!external_sat_interface_set_) {
                        // Simplify and check if ABTA is empty
                        properties_[i]->simplify();
                        is_false = properties_[i]->isEmpty();
                    } else {
                        // Use CTL-SAT to check for unsatisfiability
                        properties_[i]->isEmpty(*external_sat_interface_);
                    }
                    
                    if (is_false) {
                        false_indices.push_back(i);
                    }
                }
                
                return false_indices;
            }));
        }
        
        // Collect all false property indices from all threads
        std::vector<size_t> all_false_indices;
        for (auto& future : futures) {
            auto thread_false_indices = future.get();
            all_false_indices.insert(all_false_indices.end(), 
                                    thread_false_indices.begin(), 
                                    thread_false_indices.end());
        }
        
        // Sort indices in descending order to safely remove from properties_ vector
        std::sort(all_false_indices.rbegin(), all_false_indices.rend());
        
        // Store false properties and remove them
        for (size_t idx : all_false_indices) {
            false_properties_strings_.push_back(properties_[idx]->toString());
            false_properties_index_.push_back(idx);
        }   
    }



    // Helper function to write CSV results
    void RefinementAnalyzer::writeCsvResults(const std::string& csv_path, 
                        const std::string& input_name,
                        const AnalysisResult& result,
                        long long total_time_ms,
                        bool append) const {
        std::ofstream csv_file;
        if (append) {
            csv_file.open(csv_path, std::ios::app);
        } else {
            csv_file.open(csv_path);
            // Write header
            csv_file << "Input,Total_Properties,Equivalence_Classes,Total_Refinements,"
                    << "Required_Properties,Properties_Removed,TransitiveEliminations,"
                    << "Parsing_Time_ms,Equivalence_Time_ms,"
                    << "Refinement_Time_ms,Total_Time_ms,"
                    << "Total_Analysis_Memory_kB, Refinement_Memory_kB"
                    << "\n";
        }
        
        int properties_removed = result.total_properties - result.required_properties;
        
        csv_file << input_name << ","
                << result.total_properties << ","
                << result.equivalence_classes << ","
                << result.total_refinements << ","
                << result.required_properties << ","
                << properties_removed << ","
                << result.transitive_eliminated << ","
                << result.parsing_time.count() << ","
                << result.equivalence_time.count() << ","
                << result.refinement_time.count() << ","
                << total_time_ms << ","
                //<< result.peak_memory_kb << ","
                << result.total_analysis_memory_kb << ","
                << result.refinement_memory_kb
                << "\n";

        csv_file.close();
    }




}