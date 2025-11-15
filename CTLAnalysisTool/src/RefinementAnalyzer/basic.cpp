#include "Analyzers/Refinement.h"
#include "CTLautomaton.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <execution>
#include <filesystem>
#include <queue>
#include <stack>
#include <numeric>
#ifdef __unix__
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "utils.h"
namespace ctl {



// RefinementAnalyzer implementation
RefinementAnalyzer::RefinementAnalyzer(const std::vector<std::string>& property_strings) {
    properties_.reserve(property_strings.size());
    for (const auto& prop_str : property_strings) {
        //std::cout << "Loading property: " << prop_str << std::endl;
        try {
            
            properties_.push_back(CTLProperty::create(prop_str));
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse property '" << prop_str 
                      << "': " << e.what() << std::endl;
        }
    }
    //std::cout << "Loaded " << properties_.size() << " properties.\n";
}

RefinementAnalyzer::RefinementAnalyzer(const std::vector<std::shared_ptr<CTLProperty>>& properties) 
    {
        properties_ = properties;
    }


RefinementAnalyzer::RefinementAnalyzer(const std::string& filename) {
    auto property_strings = loadPropertiesFromFile(filename);
    __initialize_properties(property_strings);
    //std::cout << "Loaded " << properties_.size() << " properties from file: " << filename << "\n";
}

RefinementAnalyzer::~RefinementAnalyzer() {
    // Clear instance caches and Z3 resources
    clearInstanceCaches();
}

void RefinementAnalyzer::clearInstanceCaches() {
    // Clear instance caches from individual properties first
    // This will reset their automaton shared_ptrs, which in turn will clean up
    // SMT interfaces (including Z3 contexts and solvers) via unique_ptr destructors
    for (auto& property : properties_) {
        if (property) {
            property->clearInstanceCaches();
        }
    }
    
    // Clear equivalence class properties too
    for (auto& eq_class : equivalence_classes_) {
        for (auto& property : eq_class) {
            if (property) {
                property->clearInstanceCaches();
            }
        }
        eq_class.clear();
    }
    
    // Clear all instance-level caches
    properties_.clear();
    equivalence_classes_.clear();
    refinement_graphs_.clear();
    false_properties_strings_.clear();
    false_properties_index_.clear();
    result_per_property_.clear();
    
    // Clear CTL-SAT interface (releases Z3 resources if using Z3 backend)
    ctl_sat_interface_.reset();
    
    // Note: All Z3 resources (contexts, solvers) are managed via unique_ptr
    // and will be automatically freed when their owning objects are destroyed.
    // The above cleanup ensures proper destruction order and prevents memory leaks.
}

void RefinementAnalyzer::clearGlobalCaches() {
    // Clear static caches in CTLProperty class
    // Note: This requires adding a static method to CTLProperty
    CTLProperty::clearStaticCaches();
}











void RefinementAnalyzer::writeReport(const std::string& filename, const AnalysisResult& result) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    file << "CTL Refinement Analysis Report\n";
    file << "==============================\n\n";
    file << "Generated on: " << getCurrentTimestamp() << "\n\n";
    
    file << "Summary:\n";
    file << "--------\n";
    file << "Total properties: " << result.total_properties << "\n";
    file << "Equivalence classes: " << result.equivalence_classes << "\n";
    file << "Total refinements found: " << result.total_refinements << "\n";
    file << "Parsing time: " << formatDuration(result.parsing_time) << "\n";
    file << "Equivalence analysis time: " << formatDuration(result.equivalence_time) << "\n";
    file << "Refinement analysis time: " << formatDuration(result.refinement_time) << "\n";
    file << "Total analysis time: " << formatDuration(result.total_time) << "\n\n";
    file << "Refinement Memory Usage: " << result.refinement_memory_kb << " KB\n";
    file << "Total Analysis Memory Usage: " << result.total_analysis_memory_kb << " KB\n";
    file << "Peak Memory Usage: " << result.peak_memory_kb << " KB\n";

    // Write details for each equivalence class
    for (size_t i = 0; i < result.equivalence_class_properties.size(); ++i) {
        const auto& class_props = result.equivalence_class_properties[i];
        const auto& graph = result.class_graphs[i];
        
        file << "Equivalence Class " << (i + 1) << ":\n";
        file << "-------------------\n";
        file << "Properties in this class: " << class_props.size() << "\n";
        file << "Refinement edges: " << graph.getEdgeCount() << "\n";
        file << "Graph density: " << std::fixed << std::setprecision(3) 
             << graph.getDensity() << "\n\n";
        
        file << "Properties:\n";
        for (size_t j = 0; j < class_props.size(); ++j) {
            file << "  " << (j + 1) << ". " << class_props[j]->toString() << "\n";
        }
        file << "\n";
        
        if (!graph.getEdges().empty()) {
            file << "Refinements (⇒ means 'refines'):\n";
            for (const auto& edge : graph.getEdges()) {
                file << "  " << class_props[edge.from]->toString() 
                     << "  ⇒  " << class_props[edge.to]->toString() << "\n";
            }
        } else {
            file << "No non-trivial refinements found in this class.\n";
        }
        
        file << "\n\n";
    }
    
    file.close();
}

void RefinementAnalyzer::writeGraphs(const std::string& output_directory, const std::string& base_name) const {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_directory);
    
    for (size_t i = 0; i < refinement_graphs_.size(); ++i) {
        std::string filename = output_directory + "/" + base_name + "_" + std::to_string(i + 1) + ".dot";
        std::string title = "Refinement Graph - Class " + std::to_string(i + 1);
        refinement_graphs_[i].toDot(filename, title);
    }
}

std::vector<std::shared_ptr<CTLProperty>> RefinementAnalyzer::getRequiredProperties() const {
    std::vector<std::shared_ptr<CTLProperty>> required;
    
    for (const auto& graph : refinement_graphs_) {
        // Step 1: Find all strongly connected components (cycles)
        auto sccs = graph.findStronglyConnectedComponents();
        const auto& nodes = graph.getNodes();
        
        if (sccs.empty() || nodes.empty()) {
            continue;
        }
        
        // Step 2: For each SCC, pick one representative (the first node in the SCC)
        std::unordered_map<size_t, size_t> node_to_scc_id;
        std::vector<size_t> scc_representatives;
        
        for (size_t scc_id = 0; scc_id < sccs.size(); ++scc_id) {
            const auto& scc = sccs[scc_id];
            if (!scc.empty()) {
                scc_representatives.push_back(scc[0]);  // Pick first node as representative
                for (size_t node_id : scc) {
                    node_to_scc_id[node_id] = scc_id;
                }
            }
        }
        
        // Step 3: Build condensation graph (SCC graph)
        // For each SCC, find which other SCCs it refines
        std::vector<std::unordered_set<size_t>> condensation_graph(sccs.size());
        const auto& adjacency = graph.getAdjacencyList();
        
        for (size_t node_id = 0; node_id < nodes.size(); ++node_id) {
            size_t from_scc = node_to_scc_id[node_id];
            
            auto adj_it = adjacency.find(node_id);
            if (adj_it != adjacency.end()) {
                for (size_t target_node : adj_it->second) {
                    size_t to_scc = node_to_scc_id[target_node];
                    if (from_scc != to_scc) {  // Only edges between different SCCs
                        condensation_graph[from_scc].insert(to_scc);
                    }
                }
            }
        }
        
        // Step 4: Find SCCs with in-degree 0 in condensation graph
        std::vector<size_t> scc_in_degrees(sccs.size(), 0);
        for (size_t from_scc = 0; from_scc < sccs.size(); ++from_scc) {
            for (size_t to_scc : condensation_graph[from_scc]) {
                scc_in_degrees[to_scc]++;
            }
        }
        
        // Step 5: Add representatives of SCCs with in-degree 0
        for (size_t scc_id = 0; scc_id < sccs.size(); ++scc_id) {
            if (scc_in_degrees[scc_id] == 0 && !sccs[scc_id].empty()) {
                size_t repr_node_id = scc_representatives[scc_id];
                required.push_back(nodes[repr_node_id]);
            }
        }
    }
    
    return required;
}




void RefinementAnalyzer::updateGraphWithOptimization(RefinementGraph& graph, 
                                          const std::unordered_set<std::string>& eliminated_properties) {
    // Remove eliminated properties from the graph
    
    // Create mapping from formula to index
    std::unordered_map<std::string, size_t> formula_to_index;
    for (size_t i = 0; i < graph.nodes_.size(); ++i) {
        formula_to_index[graph.nodes_[i]->getFormula().toString()] = i;
    }
    
    // Identify indices to remove
    std::vector<size_t> indices_to_remove;
    for (const auto& formula : eliminated_properties) {
        auto it = formula_to_index.find(formula);
        if (it != formula_to_index.end()) {
            indices_to_remove.push_back(it->second);
        }
    }
    
    // Sort in descending order to safely remove
    std::sort(indices_to_remove.rbegin(), indices_to_remove.rend());
    
    // Remove nodes and update adjacency lists
    for (size_t idx : indices_to_remove) {
        // Remove from nodes
        graph.nodes_.erase(graph.nodes_.begin() + idx);
        
        // Remove from adjacency lists
        graph.adjacency_.erase(graph.adjacency_.begin() + idx);
        
        // Update remaining adjacency lists (decrement indices > removed index)
        for (auto& adj_list : graph.adjacency_) {
            adj_list.erase(
                std::remove_if(adj_list.begin(), adj_list.end(),
                              [idx](size_t i) { return i == idx; }),
                adj_list.end()
            );
            
            // Decrement indices greater than removed index
            for (auto& i : adj_list) {
                if (i > idx) {
                    i--;
                }
            }
        }
    }
}

TransitiveOptimizationStats RefinementAnalyzer::getTransitiveOptimizationStats() const {
    return transitive_stats_;
}


std::unordered_map<std::string, size_t> RefinementAnalyzer::getStatistics() const {
    std::unordered_map<std::string, size_t> stats;
    
    stats["total_properties"] = properties_.size();
    stats["equivalence_classes"] = equivalence_classes_.size();
    stats["total_refinements"] = 0;
    
    for (const auto& graph : refinement_graphs_) {
        stats["total_refinements"] += graph.getEdgeCount();
    }
    
    auto required = getRequiredProperties();
    stats["required_properties"] = required.size();
    
    return stats;
}

std::string RefinementAnalyzer::formatDuration(std::chrono::milliseconds duration) const {
    auto ms = duration.count();
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    } else if (ms < 60000) {
        return std::to_string(ms / 1000.0) + "s";
    } else {
        auto minutes = ms / 60000;
        auto seconds = (ms % 60000) / 1000.0;
        return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
}

void RefinementAnalyzer::writeEmptyProperties(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    file << "# False Properties (removed during analysis)\n";
    file << "# Total: " << false_properties_strings_.size() << " properties\n\n";

    for (const auto& prop : false_properties_strings_) {
        file << prop << "\n";
    }

    file.close();

    std::string filename_no_ext = filename.substr(0, filename.find_last_of('.'));
    std::ofstream file_csv(filename_no_ext + ".csv");
    if (!file_csv.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename_no_ext + ".csv");
    }
    file_csv << "Property\n";
    for (auto prop : false_properties_strings_) {
            file_csv << "\"" << prop << "\"\n";
    }
    file_csv.close();
}

void RefinementAnalyzer::writeRequiredProperties(const std::string& filename) const {
    auto required = getRequiredProperties();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    file << "# Required Properties (not refined by others)\n";
    file << "# Total: " << required.size() << " out of " << properties_.size() << " properties\n\n";
    
    

    for (const auto& prop : required) {
        file << prop->toString() << "\n";
    }

    file << "# Required Properties (by Index)\n";
    file << "# Format: Index: Property. We index from 0, so when loading remember to add 1\n\n";

    for (size_t i = 0; i < properties_.size(); ++i) {
        if (std::find(required.begin(), required.end(), properties_[i]) != required.end()) {
            file << i << ": " << properties_[i]->toString() << "\n";
        }
    }

    file.close();
    //remove .txt from filename
    std::string filename_no_ext = filename.substr(0, filename.find_last_of('.'));
    std::ofstream file_csv(filename_no_ext + ".csv");
    if (!file_csv.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename_no_ext + ".csv");
    }
    file_csv << "Index,Property\n";
    for (size_t i = 0; i < properties_.size(); ++i) {
        if (std::find(required.begin(), required.end(), properties_[i]) != required.end()) {
            file_csv << i << ",\"" << properties_[i]->toString() << "\"\n";
        }
    }
    file_csv.close();
}

std::string RefinementAnalyzer::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void RefinementAnalyzer::printProgressBar(int percent, size_t current_class, size_t total_classes) const {
    // Get terminal width (default to 80 if can't determine)
    int terminal_width = 80;
    #ifdef __unix__
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        terminal_width = w.ws_col;
    }
    #endif
    
    // Format prefix with class info
    std::ostringstream prefix_stream;
    prefix_stream << "    [Class " << current_class << "/" << total_classes << "] ";
    std::string prefix = prefix_stream.str();
    
    // Format suffix with percentage
    std::ostringstream suffix_stream;
    suffix_stream << " " << std::setw(3) << percent << "%";
    std::string suffix = suffix_stream.str();
    
    // Calculate available space for the progress bar
    int available_width = terminal_width - prefix.length() - suffix.length() - 2; // -2 for brackets
    if (available_width < 10) available_width = 10; // Minimum bar width
    
    // Calculate filled and empty portions
    int filled = (percent * available_width) / 100;
    int empty = available_width - filled;
    
    // Build the progress bar
    std::ostringstream bar_stream;
    bar_stream << prefix << "[";
    for (int i = 0; i < filled; ++i) bar_stream << "=";
    for (int i = 0; i < empty; ++i) bar_stream << " ";
    bar_stream << "]" << suffix;
    
    // Print with carriage return to overwrite previous line
    std::cout << "\r" << bar_stream.str() << std::flush;
    
    // Add newline when complete
    if (percent >= 100) {
        std::cout << std::endl;
    }
}

std::vector<size_t> RefinementGraph::getInDegrees() const {
    std::vector<size_t> in_degrees(nodes_.size(), 0);
    for (const auto& edge : edges_) {
        in_degrees[edge.to]++;
    }
    return in_degrees;
}

std::vector<size_t> RefinementGraph::getOutDegrees() const {
    std::vector<size_t> out_degrees(nodes_.size(), 0);
    for (const auto& edge : edges_) {
        out_degrees[edge.from]++;
    }
    return out_degrees;
}

// Utility functions
namespace analyzer_utils {


void exportToCSV(const AnalysisResult& result, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    file << "class_id,property_index,property,refines_property_index,refines_property\n";
    
    for (size_t class_id = 0; class_id < result.class_graphs.size(); ++class_id) {
        const auto& graph = result.class_graphs[class_id];
        const auto& properties = result.equivalence_class_properties[class_id];
        
        for (const auto& edge : graph.getEdges()) {
            file << class_id << ","
                 << edge.from << ","
                 << "\"" << properties[edge.from]->toString() << "\","
                 << edge.to << ","
                 << "\"" << properties[edge.to]->toString() << "\"\n";
        }
    }
    
    file.close();
}

std::unordered_map<std::string, double> computeGraphStatistics(const RefinementGraph& graph) {
    std::unordered_map<std::string, double> stats;
    
    stats["nodes"] = static_cast<double>(graph.getNodeCount());
    stats["edges"] = static_cast<double>(graph.getEdgeCount());
    stats["density"] = graph.getDensity();
    
    auto in_degrees = graph.getInDegrees();
    auto out_degrees = graph.getOutDegrees();
    
    if (!in_degrees.empty()) {
        double avg_in = std::accumulate(in_degrees.begin(), in_degrees.end(), 0.0) / in_degrees.size();
        double avg_out = std::accumulate(out_degrees.begin(), out_degrees.end(), 0.0) / out_degrees.size();
        
        stats["avg_in_degree"] = avg_in;
        stats["avg_out_degree"] = avg_out;
        stats["max_in_degree"] = static_cast<double>(*std::max_element(in_degrees.begin(), in_degrees.end()));
        stats["max_out_degree"] = static_cast<double>(*std::max_element(out_degrees.begin(), out_degrees.end()));
    }
    
    return stats;
}




} // namespace analyzer_utils

void RefinementAnalyzer::setUseTransitiveOptimization(bool use_transitive) {
    use_transitive_optimization_ = use_transitive;
}

void RefinementAnalyzer:: writeInfoPerProperty(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    file << "Index 1, Index 2, Property 1, Property 2, Time Taken (ms),Memory Used (KB)\n";
    for (const auto& result : result_per_property_) {

        //find property names
        std::string property1_name = "";
        std::string property2_name = "";
        if(result.property1_index < properties_.size()) {
            property1_name = properties_[result.property1_index]->toString();
        }
        if(result.property2_index < properties_.size()) {
            property2_name = properties_[result.property2_index]->toString();
        }


        file << result.property1_index << "," << result.property2_index << ","
             << "\"" << property1_name << "\","
             << "\"" << property2_name << "\","
             << result.time_taken.count() << ","
             << result.memory_used_kb << "\n";
    }

    file.close();
}

} // namespace ctl




//nohup ./check_refinements ./assets/benchmark/Dataset/Properties/2018 --use-full-language-inclusion --verbose -o output_2018_p --parallel -j 6  > output.log 2>&1 &
//CloudDeployment-PT-6b.txt