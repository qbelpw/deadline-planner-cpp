#pragma once

#include <functional>
#include <string>
#include <vector>

#include "deadline/repository.hpp"

namespace deadline {

class ExportDocument final {
public:
    ExportDocument(
        std::string suggestedFileName,
        std::string mediaType,
        std::string content
    );
    ExportDocument(const ExportDocument&) = default;
    ExportDocument(ExportDocument&&) noexcept = default;
    ExportDocument& operator=(const ExportDocument&) = default;
    ExportDocument& operator=(ExportDocument&&) noexcept = default;
    ~ExportDocument() = default;

    [[nodiscard]] const std::string& suggestedFileName() const noexcept;
    [[nodiscard]] const std::string& mediaType() const noexcept;
    [[nodiscard]] const std::string& content() const noexcept;

private:
    std::string suggestedFileName_;
    std::string mediaType_;
    std::string content_;
};

class StateExporter {
public:
    virtual ~StateExporter() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual ExportDocument exportState(
        const ApplicationState& state,
        const DateTime& generatedAt = DateTime::now()
    ) const = 0;
};

class CsvTaskExporter final : public StateExporter {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] ExportDocument exportState(
        const ApplicationState& state,
        const DateTime& generatedAt = DateTime::now()
    ) const override;
};

class MarkdownTaskExporter final : public StateExporter {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] ExportDocument exportState(
        const ApplicationState& state,
        const DateTime& generatedAt = DateTime::now()
    ) const override;
};

class ExportService final {
public:
    explicit ExportService(const ApplicationState& state) noexcept;

    [[nodiscard]] ExportDocument run(const StateExporter& exporter) const;
    [[nodiscard]] std::vector<ExportDocument> runAll(
        const std::vector<std::reference_wrapper<const StateExporter>>&
            exporters
    ) const;

private:
    const ApplicationState& state_;
};

}  // namespace deadline
