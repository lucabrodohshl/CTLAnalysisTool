

#pragma once
namespace ctl {
    class ExternalCTLSATInterface {
        public:
            void setVerbose(bool verbose) { verbose_ = verbose; }

            // Check if formula is satisfiable
            virtual bool isSatisfiable(const std::string& formula, bool with_clearing=false) const = 0;

            // Check if formula1 refines formula2 (formula1 -> formula2)
            virtual bool refines(const std::string& formula1, const std::string& formula2) const = 0;
            virtual bool implies(const std::string& formula1, const std::string& formula2) const = 0;
            // Check if formula1 is equivalent to formula2
            virtual bool equivalent(const std::string& formula1, const std::string& formula2) const = 0;
           
        protected:
            bool verbose_ = false;
            std::string sat_path_;
    };
}
