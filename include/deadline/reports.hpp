#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "deadline/repository.hpp"

namespace deadline {

using ReportRow = std::vector<std::string>;

template<typename Row>
class ReportResult final {
public:
    ReportResult() = default;

    ReportResult(std::vector<std::string> columns, std::vector<Row> rows)
        : columns_(std::move(columns)),
          rows_(std::move(rows)) {}

    ReportResult(const ReportResult&) = default;
    ReportResult(ReportResult&&) noexcept = default;
    ReportResult& operator=(const ReportResult&) = default;
    ReportResult& operator=(ReportResult&&) noexcept = default;
    ~ReportResult() = default;

    [[nodiscard]] const std::vector<std::string>& columns() const noexcept {
        return columns_;
    }

    [[nodiscard]] const std::vector<Row>& rows() const noexcept {
        return rows_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return rows_.size();
    }

    [[nodiscard]] const Row& operator[](const std::size_t index) const {
        return rows_.at(index);
    }

private:
    std::vector<std::string> columns_;
    std::vector<Row> rows_;
};

class Report {
public:
    virtual ~Report() = default;
    [[nodiscard]] virtual std::string title() const = 0;
    [[nodiscard]] virtual ReportResult<ReportRow> generate(
        const ApplicationState& state,
        const DateTime& now
    ) const = 0;
};

class OverdueReport final : public Report {
public:
    [[nodiscard]] std::string title() const override;
    [[nodiscard]] ReportResult<ReportRow> generate(
        const ApplicationState& state,
        const DateTime& now
    ) const override;
};

class WorkloadReport final : public Report {
public:
    [[nodiscard]] std::string title() const override;
    [[nodiscard]] ReportResult<ReportRow> generate(
        const ApplicationState& state,
        const DateTime& now
    ) const override;
};

class CompletionReport final : public Report {
public:
    [[nodiscard]] std::string title() const override;
    [[nodiscard]] ReportResult<ReportRow> generate(
        const ApplicationState& state,
        const DateTime& now
    ) const override;
};

class ReportService final {
public:
    explicit ReportService(const ApplicationState& state) noexcept;

    [[nodiscard]] ReportResult<ReportRow> generate(
        const Report& report,
        const DateTime& now = DateTime::now()
    ) const;

private:
    const ApplicationState& state_;
};

}  // namespace deadline
