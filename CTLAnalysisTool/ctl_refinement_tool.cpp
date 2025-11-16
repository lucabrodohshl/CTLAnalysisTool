#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include "Analyzers/Refinement.h"
#include "parser.h"
#include "synthetic_benchmark.h"
#include "ExternalCTLSAT/ctl_sat.h"
#include "utils.h"
#include "types.h"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_file_or_folder>\n";
    std::cout << "       " << program_name << " --benchmark [benchmark_options]\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help           Show this help message\n";
    std::cout << "  -o, --output <dir>   Output directory (default: output)\n";
    std::cout << "  -s, --syntactic      Use syntactic refinement only\n";
    std::cout << "  -p, --parallel       Enable parallel analysis (default)\n";
    std::cout << "  -j, --threads <n>    Number of threads to use\n";
    std::cout << "  -v, --verbose        Verbose output\n";
    std::cout << "  --no-parallel        Disable parallel analysis\n";
    std::cout << "  --no-transitive      Disable transitive closure optimization\n";
    std::cout << "  --semantic           Use semantic refinement (ABTA-based)\n";
    std::cout << "  --use-full-language-inclusion  Use full language inclusion for refinement checking\n";
    std::cout << "  --use-simulation      Use simulation for refinement checking\n";
    std::cout << "  --use-extern-sat <interface>  Specify which external SAT interface to use (CTLSAT, MOMOCTL, MLSOLVER)\n";
    std::cout << "  --sat-path <path>  Specify the path to the external SAT solver\n";
    std::cout << "\n";
    std::cout << "Input can be either a .txt file or a folder containing .txt files.\n";
    std::cout << "If a folder is provided, all .txt files will be processed.\n";
    std::cout << "\n";


    std::cout << "Input file should contain one CTL formula per line.\n";
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string output_dir = "output";
    bool use_syntactic = false;
    bool use_parallel = false;  
    bool use_transitive = true;  
    bool use_language_inclusion = true;
    size_t num_threads = std::thread::hardware_concurrency();
    ctl::AvailableCTLSATInterfaces sat_interface = ctl::AvailableCTLSATInterfaces::CTLSAT;
    bool verbose = false;
    bool use_extern_sat = false;
    
    // Benchmark mode variables
    bool benchmark_mode = false;
    ctl::BenchmarkConfig benchmark_config;
    std::string input_csv;
    std::string output_csv = "benchmark_results.csv";
    std::string sat_path = "./extern/ctl-sat";
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                output_dir = argv[++i];
            } else {
                std::cerr << "Error: -o option requires an argument\n";
                return 1;
            }
        } else if (arg == "-s" || arg == "--syntactic") {
            std::cerr << "Synthactic refinement not supported yet";
            return 1;
            use_syntactic = true;
        } else if (arg == "-p" || arg == "--parallel") {
            use_parallel = true;
        } else if (arg == "--no-parallel") {
            use_parallel = false;
        } else if (arg == "--no-transitive") {
            use_transitive = false;
        } else if (arg == "--use-full-language-inclusion") {
            use_language_inclusion = true;
        } else if (arg == "--use-simulation") {
            use_language_inclusion = false;
        } else if (arg == "--use-extern-sat") {
            use_extern_sat = true;
            if (i + 1 < argc) {
                std::string interface_str = argv[++i];
                if (interface_str == "CTLSAT") {
                    sat_interface = ctl::AvailableCTLSATInterfaces::CTLSAT;
                } else if (interface_str == "MOMOCTL") {
                    sat_interface = ctl::AvailableCTLSATInterfaces::MOMOCTL;
                } else if (interface_str == "MLSOLVER") {
                    sat_interface = ctl::AvailableCTLSATInterfaces::MLSOLVER;
                } else {
                    std::cerr << "Error: Unknown SAT interface: " << interface_str << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --sat-interface option requires an argument\n";
                return 1;
            }
        } else if (arg == "--sat-path") {
            // if use_extern_sat is set, this is required 
            if (i + 1 < argc) {
                sat_path = argv[++i];
                use_extern_sat = true;
            } else {
                std::cerr << "Error: --ctl-sat-path option requires an argument\n";
                return 1;
            }
        } else if (arg == "-j" || arg == "--threads") {
            if (i + 1 < argc) {
                num_threads = std::stoul(argv[++i]);
            } else {
                std::cerr << "Error: -j option requires an argument\n";
                return 1;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg[0] != '-') {
            input_file = arg;
        } else {
            std::cerr << "Error: Unknown option " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    if (use_extern_sat && (sat_interface == ctl::AvailableCTLSATInterfaces::NONE || !ctl::satInterfaceExist(sat_path))) {
        std::cerr << "Error: No External SAT interface type specified or sat path does not exist.\n";
        return 1;
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: No input file or folder specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    // Check if input exists
    if (!ctl::pathExists(input_file)) {
        std::cerr << "Error: Input path does not exist: " << input_file << "\n";
        return 1;
    }
    
    // Determine if input is a file or folder
    bool is_folder = ctl::isDirectory(input_file);
    std::vector<std::string> input_files;
    
    if (is_folder) {
        input_files = ctl::getTextFilesInDirectory(input_file);
        if (input_files.empty()) {
            std::cerr << "Error: No .txt files found in folder: " << input_file << "\n";
            return 1;
        }
        if (verbose) {
            std::cout << "Found " << input_files.size() << " .txt files in folder\n";
        }
    } else {
        // Single file
        input_files.push_back(input_file);
    }
    
    // Handle output directory
    if (ctl::pathExists(output_dir)) {
        if (ctl::isDirectory(output_dir)) {
            std::cout << "Warning: Output directory already exists: " << output_dir << "\n";
            std::cout << "         Existing files may be overwritten.\n";
        } else {
            std::cerr << "Error: Output path exists but is not a directory: " << output_dir << "\n";
            return 1;
        }
    } else {
        if (!ctl::createDirectory(output_dir)) {
            std::cerr << "Error: Failed to create output directory: " << output_dir << "\n";
            return 1;
        }
        if (verbose) {
            std::cout << "Created output directory: " << output_dir << "\n";
        }
    }
    
    // CSV results file
    std::string csv_results = output_dir + "/analysis_results.csv";
    bool first_file = true;
    
    try {
        if (verbose) {
            std::cout << "RefinementBasedCTLReduction Tool\n";
            std::cout << "================================\n";
            std::cout << "Output directory: " << output_dir << "\n";
            std::cout << "Refinement method: " << (use_syntactic ? "Syntactic" : "Semantic") << "\n";
            std::cout << "Parallel analysis: " << (use_parallel ? "Enabled" : "Disabled") << "\n";
            std::cout << "Transitive optimization: " << (use_transitive ? "Enabled" : "Disabled") << "\n";
            std::cout << "Full language inclusion: " << (use_language_inclusion ? "Enabled" : "Disabled") << "\n";
            std::cout << "Using method: " << (use_extern_sat ? "External SAT" : "Automaton Based") << "\n";
            if (use_extern_sat){
                std::cout << "  Interface: " << ctl::AvailableCTLSATInterfacesToString(sat_interface) << std::endl;
                std::cout << "  Interface Path: " << sat_path << std::endl;
            }
            if (use_parallel) {
                std::cout << "Number of threads: " << num_threads << "\n";
            }
            std::cout << "\n";
        }
        
        // Process each input file
        for (const auto& current_input : input_files) {
            // Extract filename for reporting
            std::string input_name = current_input;
            size_t last_slash = input_name.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                input_name = input_name.substr(last_slash + 1);
            }
            
            if (verbose) {
                std::cout << "\n========================================\n";
                std::cout << "Processing: " << input_name << "\n";
                std::cout << "========================================\n";
            }
            
            // Load and analyze properties
            auto start_time = std::chrono::high_resolution_clock::now();
            
            if (verbose) {
                std::cout << "Loading properties from file...\n";
            }
            
            auto analyzer = ctl::RefinementAnalyzer(current_input);
            
            // Configure analyzer
            analyzer.setParallelAnalysis(use_parallel);
            analyzer.setSyntacticRefinement(use_syntactic);
            analyzer.setThreads(num_threads);
            analyzer.setUseTransitiveOptimization(use_transitive);
            analyzer.setFullLanguageInclusion(use_language_inclusion);

            //analyzer.setUseCTLSAT(use_extern_sat);
            //analyzer.createCTLSATInterface(sat_path);
            if (use_extern_sat) {
                analyzer.setExternalSATInterface(sat_interface, sat_path);
            }
            analyzer.setVerbose(verbose);
            if (verbose) {
                //std::cout << "Loaded " << analyzer->getProperties().size() << " properties\n";
                std::cout << "Starting analysis...\n";
            }
            
            // Perform analysis
            auto result = analyzer.analyze();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            // Output results
            if (verbose) {
                std::cout << "Analysis completed in " << total_duration.count() << " ms\n";
                std::cout << "\nResults:\n";
                std::cout << "- Total properties: " << result.total_properties << "\n";
                std::cout << "- Equivalence classes: " << result.equivalence_classes << "\n";
                std::cout << "- Total refinements: " << result.total_refinements << "\n";
                std::cout << "- Required properties: " << result.required_properties << "\n";
                std::cout << "- Properties removed: " << (result.total_properties - result.required_properties) << "\n";
                std::cout << "- Parsing time: " << result.parsing_time.count() << " ms\n";
                std::cout << "- Equivalence time: " << result.equivalence_time.count() << " ms\n";
                std::cout << "- Refinement time: " << result.refinement_time.count() << " ms\n";
            }
            
            // Create subdirectory for this file if processing multiple files
            std::string file_output_dir = output_dir;
            if (input_files.size() > 1) {
                // Remove .txt extension from input_name for folder name
                std::string folder_name = input_name;
                if (folder_name.length() > 4 && folder_name.substr(folder_name.length() - 4) == ".txt") {
                    folder_name = folder_name.substr(0, folder_name.length() - 4);
                }
                
                // Create a single subfolder called FileSpecific
                file_output_dir = output_dir + "/FileSpecific";
                if (!ctl::pathExists(file_output_dir)) {
                    if (!ctl::createDirectory(file_output_dir)) {
                        std::cerr << "Warning: Failed to create subdirectory: " << file_output_dir << "\n";
                        file_output_dir = output_dir;
                    }
                }

                file_output_dir = file_output_dir + "/" + folder_name;
                if (!ctl::pathExists(file_output_dir)) {
                    if (!ctl::createDirectory(file_output_dir)) {
                        std::cerr << "Warning: Failed to create subdirectory: " << file_output_dir << "\n";
                        file_output_dir = output_dir;
                    }
                }
                 
                
            }
            
            // Write output files
            std::string report_file = file_output_dir + "/refinement_analysis.txt";
            analyzer.writeReport(report_file, result);
            
            analyzer.writeGraphs(file_output_dir);
            
            std::string required_props_file = file_output_dir + "/required_properties.txt";
            analyzer.writeRequiredProperties(required_props_file);

            std::string false_props_file = file_output_dir + "/false_properties.txt";
            analyzer.writeEmptyProperties(false_props_file);

            std::string info_per_property_file = file_output_dir + "/info_per_property.csv";
            analyzer.writeInfoPerProperty(info_per_property_file);

            // Write CSV results
            analyzer.writeCsvResults(
                csv_results, 
                input_name, 
                result, 
                total_duration.count(), 
                !first_file);
            first_file = false;
            
            if (verbose) {
                std::cout << "\nOutput files written to: " << file_output_dir << "\n";
                std::cout << "- Analysis report: " << report_file << "\n";
                std::cout << "- Required properties: " << required_props_file << "\n";
                std::cout << "- Refinement graphs: " << file_output_dir << "/refinement_class_*.png\n";
            }
            
            if (!verbose && input_files.size() > 1) {
                std::cout << "Processed " << input_name << ": " 
                          << result.equivalence_classes << " classes, "
                          << result.total_refinements << " refinements, "
                          << (result.total_properties - result.required_properties) << " removed\n";
            }
        }
      
        std::cout << "\n========================================\n";
        std::cout << "Analysis completed successfully!\n";
        std::cout << "Processed " << input_files.size() << " file(s)\n";
        std::cout << "CSV results written to: " << csv_results << "\n";
        std::cout << "========================================\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}


