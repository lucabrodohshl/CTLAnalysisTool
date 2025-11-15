#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include "Analyzers/SAT.h"
#include "parser.h"
#include "utils.h"
#include "types.h"


void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_file_or_folder>\n";
    std::cout << "       " << program_name << " --benchmark [benchmark_options]\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help           Show this help message\n";
    std::cout << "  -o, --output <dir>   Output directory (default: output)\n";
    std::cout << "  -p, --parallel       Enable parallel analysis (default)\n";
    std::cout << "  -j, --threads <n>    Number of threads to use\n";
    std::cout << "  -v, --verbose        Verbose output\n";
    std::cout << "  --no-parallel        Disable parallel analysis\n";
    std::cout << "  --use-full-language-inclusion  Use full language inclusion for refinement checking\n";
    std::cout << "  --use-extern-sat   Use external SAT interface for satisfiability checking\n";
    std::cout << "  --sat-interface <interface>  Specify which external SAT interface to use (CTLSAT, MOMOCTL, MLSOLVER)\n";
    std::cout << "\n";
    std::cout << "Input can be either a .txt file or a folder containing .txt files.\n";
    std::cout << "If a folder is provided, all .txt files will be processed.\n";
    std::cout << "\n";


    std::cout << "Input file should contain one CTL formula per line.\n";
}



bool satInterfaceExist(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (!ctl::pathExists(path)) {
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string output_dir = "output";
    bool use_parallel = false;  
    size_t num_threads = std::thread::hardware_concurrency();
    bool verbose = false;
    ctl::AvailableCTLSATInterfaces sat_interface = ctl::AvailableCTLSATInterfaces::NONE;
    bool use_extern_sat = false;
    std::string sat_path = "";
    // Parse command line arguments
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
        } else if (arg == "-p" || arg == "--parallel") {
            use_parallel = true;
        } else if (arg == "--no-parallel") {
            use_parallel = false;
        } else if (arg == "-j" || arg == "--threads") {
            if (i + 1 < argc) {
                num_threads = std::stoul(argv[++i]);
            } else {
                std::cerr << "Error: -j option requires an argument\n";
                return 1;
            }
        } else if (arg == "--use-extern-sat") {
            use_extern_sat = true;
        }else if (arg == "--sat-interface") {
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
        }else if (arg == "--sat-at") {
            if (i + 1 < argc) {
                sat_path = argv[++i];
            } else {
                std::cerr << "Error: --sat-path option requires an argument\n";
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

    if (use_extern_sat && (sat_interface == ctl::AvailableCTLSATInterfaces::NONE || !satInterfaceExist(sat_path))) {
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
            std::cout << "SAT Analysis\n";
            std::cout << "================================\n";
            std::cout << "Output directory: " << output_dir << "\n";
            std::cout << "Parallel analysis: " << (use_parallel ? "Enabled" : "Disabled") << "\n";
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
            
            auto analyzer = ctl::SATAnalyzer(current_input);
            if (verbose) {
                //std::cout << "Loaded " << analyzer->getProperties().size() << " properties\n";
                std::cout << "Starting analysis...\n";
            }
            // Configure analyzer
            analyzer.setParallelAnalysis(use_parallel);
            analyzer.setThreads(num_threads);

            if (use_extern_sat) {
                analyzer.setExternalSATInterface(sat_interface, sat_path);
            }
            // Perform analysis
            auto result = analyzer.analyze();
            auto end_time = std::chrono::high_resolution_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            // Output results
            if (verbose) {
                std::cout << "Analysis completed in " << total_duration.count() << " ms\n";
                std::cout << "- Unsatisfiable properties: " << result.false_properties << "\n";
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
            std::string info_per_property_file = file_output_dir + "/info_per_property.csv";
            analyzer.writeInfoPerProperty(info_per_property_file);
            std::string false_props_file = file_output_dir + "/false_properties.txt";
            analyzer.writeEmptyProperties(false_props_file);   
            
            
            analyzer.writeCsvResults(
                csv_results, 
                input_name, 
                result, 
                total_duration.count(), 
                !first_file);
            first_file = false;
         }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

}


