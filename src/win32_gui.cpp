#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "deadline/ui/win32_gui.hpp"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "deadline/reports.hpp"
#include "deadline/exporters.hpp"
#include "deadline/reminders.hpp"
#include "deadline/statistics.hpp"

namespace deadline::ui {
namespace {

constexpr COLORREF backgroundColor = RGB(245, 247, 251);
constexpr COLORREF surfaceColor = RGB(255, 255, 255);
constexpr COLORREF textColor = RGB(27, 35, 55);
constexpr COLORREF mutedColor = RGB(101, 112, 137);
constexpr COLORREF accentColor = RGB(87, 81, 245);
constexpr COLORREF successColor = RGB(32, 169, 116);
constexpr COLORREF warningColor = RGB(241, 164, 42);
constexpr COLORREF dangerColor = RGB(224, 72, 91);

constexpr int idTaskList = 100;
constexpr int idProjectFilter = 101;
constexpr int idAdd = 102;
constexpr int idComplete = 103;
constexpr int idChecklist = 104;
constexpr int idReports = 105;
constexpr int idRefresh = 106;
constexpr int idExport = 109;
constexpr int idProgress = 107;
constexpr int idDetails = 108;
constexpr int idFormTitle = 201;
constexpr int idFormDeadline = 202;
constexpr int idFormProject = 203;
constexpr int idFormType = 204;
constexpr int idFormPriority = 205;
constexpr int idFormEstimate = 206;
constexpr int idFormExtra = 207;
constexpr int idFormDescription = 208;
constexpr int idFormSave = 209;
constexpr int idFormCancel = 210;

std::wstring wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const auto length = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length
    );
    return result;
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const auto length = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length,
        nullptr,
        nullptr
    );
    return result;
}

std::wstring windowText(const HWND window) {
    const auto length = GetWindowTextLengthW(window);
    std::wstring value(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(window, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(length));
    return value;
}

std::wstring typeLabel(const TaskType type) {
    switch (type) {
        case TaskType::deadline:
            return L"Дедлайн";
        case TaskType::recurring:
            return L"Повторяющаяся";
        case TaskType::checklist:
            return L"Чек-лист";
    }
    return L"Задача";
}

std::wstring urgencyLabel(const DeadlineScale& scale) {
    return wide(scale.label);
}

class MainWindow final {
public:
    MainWindow(TaskService& tasks, ProjectService& projects) noexcept
        : tasks_(tasks), projects_(projects),
          backgroundBrush_(CreateSolidBrush(backgroundColor)),
          surfaceBrush_(CreateSolidBrush(surfaceColor)) {}

    ~MainWindow() {
        DeleteObject(font_);
        DeleteObject(titleFont_);
        DeleteObject(backgroundBrush_);
        DeleteObject(surfaceBrush_);
    }

    int run(const int showCommand) {
        INITCOMMONCONTROLSEX controls{
            sizeof(INITCOMMONCONTROLSEX),
            ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS
        };
        InitCommonControlsEx(&controls);
        registerClass();
        window_ = CreateWindowExW(
            0,
            className,
            L"Deadline Planner — проект по ООП на C++",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1240,
            780,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            this
        );
        require(window_ != nullptr, "Не удалось создать главное окно");
        ShowWindow(window_, showCommand);
        UpdateWindow(window_);

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

private:
    static constexpr const wchar_t* className = L"DeadlinePlannerMainWindow";

    static LRESULT CALLBACK windowProcedure(
        HWND window,
        UINT message,
        WPARAM word,
        LPARAM number
    ) {
        MainWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            const auto* data = reinterpret_cast<CREATESTRUCTW*>(number);
            self = static_cast<MainWindow*>(data->lpCreateParams);
            SetWindowLongPtrW(
                window,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(self)
            );
            self->window_ = window;
        } else {
            self = reinterpret_cast<MainWindow*>(
                GetWindowLongPtrW(window, GWLP_USERDATA)
            );
        }
        return self == nullptr
            ? DefWindowProcW(window, message, word, number)
            : self->handle(message, word, number);
    }

    void registerClass() {
        WNDCLASSEXW definition{};
        definition.cbSize = sizeof(definition);
        definition.lpfnWndProc = windowProcedure;
        definition.hInstance = GetModuleHandleW(nullptr);
        definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        definition.hbrBackground = backgroundBrush_;
        definition.lpszClassName = className;
        definition.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&definition);
    }

    LRESULT handle(const UINT message, const WPARAM word, const LPARAM number) {
        switch (message) {
            case WM_CREATE:
                createControls();
                refreshAll();
                return 0;
            case WM_SIZE:
                layout(LOWORD(number), HIWORD(number));
                return 0;
            case WM_COMMAND:
                command(LOWORD(word), HIWORD(word));
                return 0;
            case WM_NOTIFY:
                notify(reinterpret_cast<NMHDR*>(number));
                return 0;
            case WM_CTLCOLORSTATIC:
                return controlColor(
                    reinterpret_cast<HDC>(word),
                    reinterpret_cast<HWND>(number)
                );
            case WM_CTLCOLOREDIT:
                SetTextColor(reinterpret_cast<HDC>(word), textColor);
                SetBkColor(reinterpret_cast<HDC>(word), surfaceColor);
                return reinterpret_cast<LRESULT>(surfaceBrush_);
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
                paint();
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(window_, message, word, number);
        }
    }

    HWND addControl(
        const wchar_t* classValue,
        const wchar_t* text,
        const DWORD style,
        const int identifier
    ) {
        const auto control = CreateWindowExW(
            classValue == WC_LISTVIEWW ? WS_EX_CLIENTEDGE : 0,
            classValue,
            text,
            WS_CHILD | WS_VISIBLE | style,
            0,
            0,
            100,
            30,
            window_,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(identifier)
            ),
            GetModuleHandleW(nullptr),
            nullptr
        );
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(font_),
            TRUE
        );
        return control;
    }

    HWND addLabel(const wchar_t* text, const int identifier = 0) {
        return addControl(
            L"STATIC",
            text,
            SS_LEFT | SS_NOPREFIX,
            identifier
        );
    }

    HWND addButton(const wchar_t* text, const int identifier) {
        return addControl(
            L"BUTTON",
            text,
            BS_PUSHBUTTON | BS_FLAT | WS_TABSTOP,
            identifier
        );
    }

    HWND addEdit(const int identifier, const bool multiline = false) {
        auto style = ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER;
        if (multiline) {
            style = ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL
                | WS_TABSTOP | WS_BORDER;
        }
        return addControl(L"EDIT", L"", style, identifier);
    }

    HWND addCombo(const int identifier) {
        return addControl(
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
            identifier
        );
    }

    void createControls() {
        font_ = CreateFontW(
            -17,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH,
            L"Segoe UI"
        );
        titleFont_ = CreateFontW(
            -31,
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH,
            L"Segoe UI"
        );
        heading_ = addLabel(L"Мои дедлайны");
        SendMessageW(
            heading_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(titleFont_),
            TRUE
        );
        subtitle_ = addLabel(
            L"Планируйте задачи и следите за оставшимся временем"
        );
        summaryActive_ = addLabel(L"Активные\n0");
        summarySoon_ = addLabel(L"Скоро\n0");
        summaryOverdue_ = addLabel(L"Просрочены\n0");
        summaryDone_ = addLabel(L"Выполнены\n0");

        projectFilter_ = addCombo(idProjectFilter);
        taskList_ = addControl(
            WC_LISTVIEWW,
            L"",
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            idTaskList
        );
        ListView_SetExtendedListViewStyle(
            taskList_,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES
        );
        addColumn(0, L"Задача", 270);
        addColumn(1, L"Проект", 145);
        addColumn(2, L"Срок", 145);
        addColumn(3, L"Тип", 115);
        addColumn(4, L"Осталось", 150);

        addButton_ = addButton(L"＋ Новая задача", idAdd);
        completeButton_ = addButton(L"✓ Выполнить", idComplete);
        checklistButton_ = addButton(L"Пункт чек-листа", idChecklist);
        reportsButton_ = addButton(L"Отчёты", idReports);
        refreshButton_ = addButton(L"Обновить", idRefresh);
        exportButton_ = addButton(L"Экспорт", idExport);
        detailsTitle_ = addLabel(L"Выберите задачу");
        details_ = addLabel(
            L"Здесь появятся подробности и шкала срока.",
            idDetails
        );
        progress_ = addControl(
            PROGRESS_CLASSW,
            L"",
            PBS_SMOOTH,
            idProgress
        );
        SendMessageW(progress_, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));

        formTitleLabel_ = addLabel(L"Новая задача");
        formNameLabel_ = addLabel(L"Название");
        formTitle_ = addEdit(idFormTitle);
        formDeadlineLabel_ = addLabel(L"Срок: ГГГГ-ММ-ДД ЧЧ:ММ");
        formDeadline_ = addEdit(idFormDeadline);
        formProjectLabel_ = addLabel(L"Проект");
        formProject_ = addCombo(idFormProject);
        formTypeLabel_ = addLabel(L"Тип задачи");
        formType_ = addCombo(idFormType);
        addComboItem(formType_, L"Дедлайн");
        addComboItem(formType_, L"Повторяющаяся");
        addComboItem(formType_, L"Чек-лист");
        SendMessageW(formType_, CB_SETCURSEL, 0, 0);
        formPriorityLabel_ = addLabel(L"Приоритет 1..3");
        formPriority_ = addEdit(idFormPriority);
        SetWindowTextW(formPriority_, L"2");
        formEstimateLabel_ = addLabel(L"Оценка, минут");
        formEstimate_ = addEdit(idFormEstimate);
        SetWindowTextW(formEstimate_, L"60");
        formExtraLabel_ = addLabel(
            L"Интервал дней или пункты через точку с запятой"
        );
        formExtra_ = addEdit(idFormExtra);
        formDescriptionLabel_ = addLabel(L"Описание");
        formDescription_ = addEdit(idFormDescription, true);
        formSave_ = addButton(L"Сохранить", idFormSave);
        formCancel_ = addButton(L"Отмена", idFormCancel);
        setFormVisible(false);
    }

    void addColumn(const int index, const wchar_t* title, const int width) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<wchar_t*>(title);
        column.cx = width;
        column.iSubItem = index;
        ListView_InsertColumn(taskList_, index, &column);
    }

    static void addComboItem(const HWND combo, const wchar_t* text) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }

    void layout(const int width, const int height) const {
        constexpr int margin = 28;
        constexpr int gap = 12;
        const int contentWidth = std::max(width - margin * 2, 400);
        MoveWindow(heading_, margin, 22, 400, 42, TRUE);
        MoveWindow(subtitle_, margin, 64, 600, 24, TRUE);

        const int cardWidth = (contentWidth - gap * 3) / 4;
        const HWND cards[]{
            summaryActive_, summarySoon_, summaryOverdue_, summaryDone_
        };
        for (int index = 0; index < 4; ++index) {
            MoveWindow(
                cards[index],
                margin + index * (cardWidth + gap),
                104,
                cardWidth,
                64,
                TRUE
            );
        }

        MoveWindow(projectFilter_, margin, 188, 210, 260, TRUE);
        MoveWindow(addButton_, margin + 222, 186, 160, 34, TRUE);
        MoveWindow(completeButton_, margin + 394, 186, 130, 34, TRUE);
        MoveWindow(checklistButton_, margin + 536, 186, 150, 34, TRUE);
        MoveWindow(reportsButton_, margin + 698, 186, 100, 34, TRUE);
        MoveWindow(refreshButton_, margin + 810, 186, 100, 34, TRUE);
        MoveWindow(exportButton_, margin + 922, 186, 100, 34, TRUE);

        const int listWidth = formVisible_
            ? std::max(contentWidth - 382, 420)
            : std::max((contentWidth * 68) / 100, 520);
        const int panelX = margin + listWidth + gap;
        const int panelWidth = contentWidth - listWidth - gap;
        const int listHeight = std::max(height - 258, 280);
        MoveWindow(taskList_, margin, 232, listWidth, listHeight, TRUE);

        if (!formVisible_) {
            MoveWindow(
                detailsTitle_,
                panelX + 18,
                248,
                panelWidth - 36,
                30,
                TRUE
            );
            MoveWindow(progress_, panelX + 18, 292, panelWidth - 36, 18, TRUE);
            MoveWindow(details_, panelX + 18, 328, panelWidth - 36,
                       listHeight - 96, TRUE);
        } else {
            layoutForm(panelX + 18, 244, panelWidth - 36);
        }
    }

    void layoutForm(const int x, int y, const int width) const {
        auto row = [&y, x, width](
            const HWND label,
            const HWND field,
            const int fieldHeight
        ) {
            MoveWindow(label, x, y, width, 20, TRUE);
            y += 22;
            MoveWindow(field, x, y, width, fieldHeight, TRUE);
            y += fieldHeight + 8;
        };
        MoveWindow(formTitleLabel_, x, y, width, 28, TRUE);
        y += 34;
        row(formNameLabel_, formTitle_, 30);
        row(formDeadlineLabel_, formDeadline_, 30);
        row(formProjectLabel_, formProject_, 180);
        y -= 150;
        row(formTypeLabel_, formType_, 180);
        y -= 150;
        const int half = (width - 10) / 2;
        MoveWindow(formPriorityLabel_, x, y, half, 20, TRUE);
        MoveWindow(formEstimateLabel_, x + half + 10, y, half, 20, TRUE);
        y += 22;
        MoveWindow(formPriority_, x, y, half, 30, TRUE);
        MoveWindow(formEstimate_, x + half + 10, y, half, 30, TRUE);
        y += 38;
        row(formExtraLabel_, formExtra_, 30);
        row(formDescriptionLabel_, formDescription_, 64);
        MoveWindow(formSave_, x, y, half, 34, TRUE);
        MoveWindow(formCancel_, x + half + 10, y, half, 34, TRUE);
    }

    void setFormVisible(const bool visible) {
        formVisible_ = visible;
        const int command = visible ? SW_SHOW : SW_HIDE;
        const HWND formControls[]{
            formTitleLabel_, formNameLabel_, formTitle_, formDeadlineLabel_,
            formDeadline_, formProjectLabel_, formProject_, formTypeLabel_,
            formType_, formPriorityLabel_, formPriority_, formEstimateLabel_,
            formEstimate_, formExtraLabel_, formExtra_, formDescriptionLabel_,
            formDescription_, formSave_, formCancel_
        };
        for (const auto control : formControls) {
            if (control != nullptr) {
                ShowWindow(control, command);
            }
        }
        const int detailsCommand = visible ? SW_HIDE : SW_SHOW;
        if (detailsTitle_ != nullptr) {
            ShowWindow(detailsTitle_, detailsCommand);
            ShowWindow(details_, detailsCommand);
            ShowWindow(progress_, detailsCommand);
        }
        RECT area{};
        if (window_ != nullptr) {
            GetClientRect(window_, &area);
            layout(area.right, area.bottom);
            InvalidateRect(window_, nullptr, TRUE);
        }
    }

    void refreshAll() {
        refreshProjects();
        refreshTasks();
        refreshSummary();
        updateDetails();
    }

    void refreshProjects() {
        const auto selected = static_cast<int>(
            SendMessageW(projectFilter_, CB_GETCURSEL, 0, 0)
        );
        SendMessageW(projectFilter_, CB_RESETCONTENT, 0, 0);
        SendMessageW(formProject_, CB_RESETCONTENT, 0, 0);
        projectIds_.clear();
        formProjectIds_.clear();
        addComboItem(projectFilter_, L"Все проекты");
        projectIds_.push_back({});
        for (const auto& [identifier, project] : tasks_.state().projects) {
            if (project.archived()) {
                continue;
            }
            const auto title = wide(project.title());
            addComboItem(projectFilter_, title.c_str());
            addComboItem(formProject_, title.c_str());
            projectIds_.push_back(identifier);
            formProjectIds_.push_back(identifier);
        }
        SendMessageW(
            projectFilter_,
            CB_SETCURSEL,
            selected >= 0 && selected < static_cast<int>(projectIds_.size())
                ? selected
                : 0,
            0
        );
        if (!formProjectIds_.empty()) {
            SendMessageW(formProject_, CB_SETCURSEL, 0, 0);
        }
    }

    void refreshTasks() {
        const auto selectedFilter = static_cast<int>(
            SendMessageW(projectFilter_, CB_GETCURSEL, 0, 0)
        );
        TaskFilter filter;
        if (selectedFilter > 0
            && selectedFilter < static_cast<int>(projectIds_.size())) {
            filter.projectId = projectIds_[
                static_cast<std::size_t>(selectedFilter)
            ];
        }
        visibleTaskIds_.clear();
        ListView_DeleteAllItems(taskList_);
        const auto found = tasks_.find(filter);
        const auto now = DateTime::now();
        int row = 0;
        for (const auto reference : found) {
            const auto& task = reference.get();
            visibleTaskIds_.push_back(task.id());
            const auto title = wide(task.title());
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = row;
            item.pszText = const_cast<wchar_t*>(title.c_str());
            ListView_InsertItem(taskList_, &item);
            setCell(row, 1, projectTitle(task.projectId()));
            setCell(row, 2, wide(task.deadline().format()));
            setCell(row, 3, typeLabel(task.type()));
            setCell(row, 4, urgencyLabel(task.scale(now)));
            ++row;
        }
    }

    void setCell(const int row, const int column, const std::wstring& value) {
        ListView_SetItemText(
            taskList_,
            row,
            column,
            const_cast<wchar_t*>(value.c_str())
        );
    }

    std::wstring projectTitle(const std::string& identifier) const {
        const auto found = tasks_.state().projects.find(identifier);
        return found == tasks_.state().projects.end()
            ? L"—"
            : wide(found->second.title());
    }

    void refreshSummary() const {
        const StatisticsService service(tasks_.state());
        const auto values = service.calculate();
        SetWindowTextW(
            summaryActive_,
            (L"АКТИВНЫЕ\r\n" + std::to_wstring(values.active)).c_str()
        );
        SetWindowTextW(
            summarySoon_,
            (L"СКОРО\r\n" + std::to_wstring(values.dueSoon)).c_str()
        );
        SetWindowTextW(
            summaryOverdue_,
            (L"ПРОСРОЧЕНЫ\r\n" + std::to_wstring(values.overdue)).c_str()
        );
        SetWindowTextW(
            summaryDone_,
            (L"ВЫПОЛНЕНЫ\r\n" + std::to_wstring(values.completed)).c_str()
        );
    }

    int selectedRow() const {
        return ListView_GetNextItem(taskList_, -1, LVNI_SELECTED);
    }

    const Task* selectedTask() const {
        const auto row = selectedRow();
        if (row < 0 || row >= static_cast<int>(visibleTaskIds_.size())) {
            return nullptr;
        }
        return &tasks_.find(visibleTaskIds_[static_cast<std::size_t>(row)]);
    }

    void updateDetails() const {
        const auto* task = selectedTask();
        if (task == nullptr) {
            SetWindowTextW(detailsTitle_, L"Выберите задачу");
            SetWindowTextW(
                details_,
                L"Нажмите на строку слева, чтобы увидеть подробности."
            );
            SendMessageW(progress_, PBM_SETPOS, 0, 0);
            return;
        }
        const auto scale = task->scale(DateTime::now());
        SetWindowTextW(detailsTitle_, wide(task->title()).c_str());
        std::wostringstream details;
        details << L"Проект: " << projectTitle(task->projectId()) << L"\r\n\r\n"
                << L"Тип: " << typeLabel(task->type()) << L"\r\n"
                << L"Срок: " << wide(task->deadline().format()) << L"\r\n"
                << L"Приоритет: " << task->priority() << L"\r\n"
                << L"Оценка: " << task->estimate().minutes() << L" мин\r\n\r\n"
                << urgencyLabel(scale) << L"\r\n\r\n"
                << wide(task->description());
        if (task->type() == TaskType::recurring) {
            const auto& recurring = static_cast<const RecurringTask&>(*task);
            details << L"\r\nИнтервал: " << recurring.intervalDays()
                    << L" дн.\r\nВыполнений: "
                    << recurring.completionCount();
        }
        if (task->type() == TaskType::checklist) {
            const auto& checklist = static_cast<const ChecklistTask&>(*task);
            details << L"\r\n\r\nПункты:\r\n";
            for (const auto& item : checklist.items()) {
                details << (item.done() ? L"✓ " : L"○ ")
                        << wide(item.text()) << L"\r\n";
            }
        }
        SetWindowTextW(details_, details.str().c_str());
        const auto value = static_cast<int>(std::round(
            std::clamp(scale.elapsedShare, 0.0, 1.0) * 1000.0
        ));
        SendMessageW(progress_, PBM_SETPOS, value, 0);
        const auto color = scale.urgency == DeadlineScale::Urgency::overdue
            ? dangerColor
            : scale.urgency == DeadlineScale::Urgency::urgent
                ? warningColor
                : successColor;
        SendMessageW(progress_, PBM_SETBARCOLOR, 0, color);
    }

    void command(const int identifier, const int notification) {
        try {
            if (identifier == idAdd) {
                openForm();
            } else if (identifier == idComplete) {
                completeSelected();
            } else if (identifier == idChecklist) {
                toggleNextChecklistItem();
            } else if (identifier == idReports) {
                showReports();
            } else if (identifier == idRefresh) {
                refreshAll();
            } else if (identifier == idExport) {
                exportTasks();
            } else if (identifier == idFormSave) {
                saveForm();
            } else if (identifier == idFormCancel) {
                setFormVisible(false);
            } else if (identifier == idProjectFilter
                       && notification == CBN_SELCHANGE) {
                refreshTasks();
                updateDetails();
            }
        } catch (const std::exception& error) {
            MessageBoxW(
                window_,
                wide(error.what()).c_str(),
                L"Не удалось выполнить действие",
                MB_OK | MB_ICONWARNING
            );
        }
    }

    void notify(const NMHDR* header) const {
        if (header->idFrom == idTaskList
            && header->code == LVN_ITEMCHANGED) {
            updateDetails();
        }
    }

    void openForm() {
        refreshProjects();
        SetWindowTextW(formTitle_, L"");
        SetWindowTextW(
            formDeadline_,
            wide(DateTime::now().addDays(7).format()).c_str()
        );
        SetWindowTextW(formExtra_, L"");
        SetWindowTextW(formDescription_, L"");
        SendMessageW(formType_, CB_SETCURSEL, 0, 0);
        setFormVisible(true);
        SetFocus(formTitle_);
    }

    void saveForm() {
        TaskDraft draft;
        draft.title = utf8(windowText(formTitle_));
        draft.deadline = DateTime::parse(utf8(windowText(formDeadline_)));
        draft.description = utf8(windowText(formDescription_));
        draft.priority = parseInt(formPriority_, "Приоритет");
        draft.estimate = TimeInterval(parseInt(formEstimate_, "Оценка"));
        require(draft.priority >= 1 && draft.priority <= 3,
                "Приоритет должен быть от 1 до 3");
        const auto projectIndex = static_cast<int>(
            SendMessageW(formProject_, CB_GETCURSEL, 0, 0)
        );
        require(projectIndex >= 0, "Выберите проект");
        draft.projectId = formProjectIds_[
            static_cast<std::size_t>(projectIndex)
        ];
        const auto typeIndex = static_cast<int>(
            SendMessageW(formType_, CB_GETCURSEL, 0, 0)
        );
        require(typeIndex >= 0 && typeIndex <= 2, "Выберите тип задачи");
        draft.type = static_cast<TaskType>(typeIndex);
        const auto extra = utf8(windowText(formExtra_));
        if (draft.type == TaskType::recurring) {
            std::size_t position = 0;
            draft.intervalDays = std::stoi(extra, &position);
            require(position == extra.size(), "Интервал должен быть числом");
        } else if (draft.type == TaskType::checklist) {
            std::stringstream stream(extra);
            std::string item;
            while (std::getline(stream, item, ';')) {
                if (!trim(item).empty()) {
                    draft.checklistItems.push_back(trim(item));
                }
            }
        }
        tasks_.addTask(draft);
        setFormVisible(false);
        refreshAll();
    }

    static int parseInt(const HWND control, const std::string& field) {
        const auto text = utf8(windowText(control));
        std::size_t position = 0;
        const auto value = std::stoi(text, &position);
        require(position == text.size(), field + " должна быть числом");
        return value;
    }

    void completeSelected() {
        const auto* task = selectedTask();
        require(task != nullptr, "Сначала выберите задачу");
        tasks_.complete(task->id());
        refreshAll();
    }

    void toggleNextChecklistItem() {
        const auto* task = selectedTask();
        require(task != nullptr, "Сначала выберите задачу");
        require(task->type() == TaskType::checklist,
                "Выбранная задача не является чек-листом");
        const auto& checklist = static_cast<const ChecklistTask&>(*task);
        const auto found = std::find_if(
            checklist.items().begin(),
            checklist.items().end(),
            [](const ChecklistItem& item) { return !item.done(); }
        );
        require(found != checklist.items().end(), "Все пункты уже выполнены");
        tasks_.setChecklistItem(task->id(), found->id(), true);
        refreshAll();
    }

    void showReports() const {
        ReportService reports(tasks_.state());
        const OverdueReport overdue;
        const WorkloadReport workload;
        const CompletionReport completion;
        const Report* all[]{&overdue, &workload, &completion};
        std::wostringstream text;
        for (const auto* report : all) {
            const auto result = reports.generate(*report);
            text << wide(report->title()) << L"\r\n";
            if (result.rows().empty()) {
                text << L"  Нет данных\r\n\r\n";
                continue;
            }
            for (const auto& row : result.rows()) {
                text << L"  • ";
                for (std::size_t index = 0; index < row.size(); ++index) {
                    if (index != 0) {
                        text << L" | ";
                    }
                    text << wide(row[index]);
                }
                text << L"\r\n";
            }
            text << L"\r\n";
        }
        const ReminderEngine reminders;
        text << L"Умные напоминания\r\n";
        const auto messages = reminders.evaluate(tasks_.state());
        if (messages.empty()) {
            text << L"  Нет актуальных напоминаний\r\n";
        }
        for (const auto& message : messages) {
            text << L"  • [" << wide(toString(message.level())) << L"] "
                 << wide(message.message()) << L"\r\n";
        }
        MessageBoxW(window_, text.str().c_str(), L"Отчёты", MB_OK);
    }

    void exportTasks() const {
        const CsvTaskExporter csv;
        const MarkdownTaskExporter markdown;
        const ExportService service(tasks_.state());
        const std::vector<std::reference_wrapper<const StateExporter>>
            exporters{csv, markdown};
        const std::filesystem::path directory = "exports";
        std::filesystem::create_directories(directory);
        std::wostringstream created;
        for (const auto& document : service.runAll(exporters)) {
            const auto path = directory / document.suggestedFileName();
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            require(output.good(), "Не удалось открыть файл экспорта");
            const unsigned char bom[]{0xEF, 0xBB, 0xBF};
            output.write(
                reinterpret_cast<const char*>(bom),
                static_cast<std::streamsize>(sizeof(bom))
            );
            output << document.content();
            require(output.good(), "Не удалось записать файл экспорта");
            created << L"• " << path.wstring() << L"\r\n";
        }
        MessageBoxW(
            window_,
            created.str().c_str(),
            L"Экспорт завершён",
            MB_OK | MB_ICONINFORMATION
        );
    }

    LRESULT controlColor(const HDC context, const HWND control) const {
        SetBkMode(context, TRANSPARENT);
        SetTextColor(context, control == subtitle_ ? mutedColor : textColor);
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    }

    void paint() const {
        PAINTSTRUCT state{};
        const auto context = BeginPaint(window_, &state);
        RECT area{};
        GetClientRect(window_, &area);
        FillRect(context, &area, backgroundBrush_);

        RECT listPanel{16, 96, area.right - 16, 176};
        FillRect(context, &listPanel, surfaceBrush_);
        RECT contentPanel{16, 224, area.right - 16, area.bottom - 16};
        FillRect(context, &contentPanel, surfaceBrush_);
        EndPaint(window_, &state);
    }

    TaskService& tasks_;
    ProjectService& projects_;
    HWND window_ = nullptr;
    HFONT font_ = nullptr;
    HFONT titleFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH surfaceBrush_ = nullptr;
    bool formVisible_ = false;
    std::vector<std::string> visibleTaskIds_;
    std::vector<std::string> projectIds_;
    std::vector<std::string> formProjectIds_;

    HWND heading_ = nullptr;
    HWND subtitle_ = nullptr;
    HWND summaryActive_ = nullptr;
    HWND summarySoon_ = nullptr;
    HWND summaryOverdue_ = nullptr;
    HWND summaryDone_ = nullptr;
    HWND projectFilter_ = nullptr;
    HWND taskList_ = nullptr;
    HWND addButton_ = nullptr;
    HWND completeButton_ = nullptr;
    HWND checklistButton_ = nullptr;
    HWND reportsButton_ = nullptr;
    HWND refreshButton_ = nullptr;
    HWND exportButton_ = nullptr;
    HWND detailsTitle_ = nullptr;
    HWND details_ = nullptr;
    HWND progress_ = nullptr;
    HWND formTitleLabel_ = nullptr;
    HWND formNameLabel_ = nullptr;
    HWND formTitle_ = nullptr;
    HWND formDeadlineLabel_ = nullptr;
    HWND formDeadline_ = nullptr;
    HWND formProjectLabel_ = nullptr;
    HWND formProject_ = nullptr;
    HWND formTypeLabel_ = nullptr;
    HWND formType_ = nullptr;
    HWND formPriorityLabel_ = nullptr;
    HWND formPriority_ = nullptr;
    HWND formEstimateLabel_ = nullptr;
    HWND formEstimate_ = nullptr;
    HWND formExtraLabel_ = nullptr;
    HWND formExtra_ = nullptr;
    HWND formDescriptionLabel_ = nullptr;
    HWND formDescription_ = nullptr;
    HWND formSave_ = nullptr;
    HWND formCancel_ = nullptr;
};

}  // namespace

int runWin32Gui(
    TaskService& tasks,
    ProjectService& projects,
    const int showCommand
) {
    MainWindow window(tasks, projects);
    return window.run(showCommand);
}

}  // namespace deadline::ui

#endif
