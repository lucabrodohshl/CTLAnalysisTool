#include "Analyzers/SAT.h"
#include "utils.h"

namespace ctl {
    // RefinementAnalyzer implementation
SATAnalyzer::SATAnalyzer(const std::vector<std::string>& property_strings) {
    __initialize_properties(property_strings);
    //std::cout << "Loaded " << properties_.size() << " properties.\n";
}

SATAnalyzer::SATAnalyzer(const std::vector<std::shared_ptr<CTLProperty>>& properties) 
{
        properties_ = properties;
}

SATAnalyzer::SATAnalyzer(const std::string& filename) {
    auto property_strings = loadPropertiesFromFile(filename);
    __initialize_properties(property_strings);
    //std::cout << "Loaded " << properties_.size() << " properties from file: " << filename << "\n";
}

SATAnalyzer::~SATAnalyzer() {
    // Clear instance caches and Z3 resources
    //clearInstanceCaches();
}



void SATAnalyzer::writeInfoPerProperty(const std::string& filename) const
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    file << "Index, Property, Time Taken (ms),Memory Used (KB)\n";
    for (const auto& result : result_per_property_) {

        //find property names
        std::string property_name = "";
        if(result.property1_index < properties_.size()) {
            property_name = properties_[result.property1_index]->toString();
        }


        file << result.property1_index << ","
             << "\"" << property_name << "\","
             << result.time_taken.count() << ","
             << result.memory_used_kb << "\n";
    }

    file.close();
}



void SATAnalyzer::writeEmptyProperties(const std::string& filename) const {
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


void SATAnalyzer:: writeCsvResults(const std::string& csv_path, 
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
        csv_file << "Input,Total_Properties,Unsatisfiable_Properties,"
                << "Total_Time_ms,"
                << "Total_Analysis_Memory_kB"
                << "\n";
    }
    
    int false_properties = false_properties_strings_.size();
    
    csv_file << input_name << ","
            << result.total_properties << ","
            << false_properties << ","
            << total_time_ms << ","
            << result.total_analysis_memory_kb
            << "\n";

    csv_file.close();
}

}