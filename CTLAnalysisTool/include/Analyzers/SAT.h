#include "analyzerInterface.h"
#include "property.h"
#include "types.h"

namespace ctl{
    class SATAnalyzer : public Analyzer {
    public:
        explicit SATAnalyzer(const std::vector<std::shared_ptr<CTLProperty>>& properties);
        explicit SATAnalyzer(const std::vector<std::string>& property_strings);
        explicit SATAnalyzer(const std::string& filename);
        ~SATAnalyzer();
        AnalysisResult analyze() override;
        PropertyResult checkSAT(const CTLProperty& property) const;

        void writeInfoPerProperty(const std::string& filename) const;
        void writeEmptyProperties(const std::string& filename) const;
        void writeCsvResults(const std::string& csv_path, 
                        const std::string& input_name,
                        const AnalysisResult& result,
                        long long total_time_ms,
                        bool append = false) const;
    private:
        std::vector<std::string> false_properties_strings_;
        std::vector<size_t> false_properties_index_;
    };

}