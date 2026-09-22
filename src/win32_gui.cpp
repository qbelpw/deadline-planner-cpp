#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "deadline/ui/win32_gui.hpp"

// clang-format off
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "deadline/exporters.hpp"
#include "deadline/reminders.hpp"
#include "deadline/reports.hpp"
#include "deadline/statistics.hpp"

namespace deadline::ui {
namespace {

constexpr COLORREF backgroundColor = RGB(28, 29, 32);
constexpr COLORREF surfaceColor = RGB(32, 34, 38);
constexpr COLORREF textColor = RGB(232, 232, 234);
constexpr COLORREF mutedColor = RGB(157, 160, 167);
constexpr COLORREF accentColor = RGB(56, 104, 167);
constexpr COLORREF warningColor = RGB(239, 194, 116);
constexpr COLORREF dangerColor = RGB(244, 143, 133);
constexpr COLORREF borderColor = RGB(51, 53, 58);
constexpr COLORREF selectedColor = RGB(41, 55, 73);

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
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(),
                        static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const auto length = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                            static_cast<int>(value.size()),
                                            nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(),
                        static_cast<int>(value.size()), result.data(), length,
                        nullptr, nullptr);
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
        : tasks_(tasks),
          projects_(projects),
          backgroundBrush_(CreateSolidBrush(backgroundColor)),
          surfaceBrush_(CreateSolidBrush(surfaceColor)) {}

    ~MainWindow() {
        DeleteObject(font_);
        DeleteObject(titleFont_);
        DeleteObject(sectionFont_);
        ImageList_Destroy(rowHeightImages_);
        DeleteObject(backgroundBrush_);
        DeleteObject(surfaceBrush_);
    }

    int run(const int showCommand) {
        INITCOMMONCONTROLSEX controls{
            sizeof(INITCOMMONCONTROLSEX),
            ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
        InitCommonControlsEx(&controls);
        registerClass();
        window_ = CreateWindowExW(
            0, className, L"Deadline Planner",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
            1280, 840, nullptr, nullptr, GetModuleHandleW(nullptr), this);
        require(window_ != nullptr, "Не удалось создать главное окно");
        const BOOL darkCaption = TRUE;
        DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE,
                              &darkCaption, sizeof(darkCaption));
        ShowWindow(window_, showCommand);
        UpdateWindow(window_);

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(window_, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        return static_cast<int>(message.wParam);
    }

   private:
    static constexpr const wchar_t* className = L"DeadlinePlannerMainWindow";

    static LRESULT CALLBACK windowProcedure(HWND window, UINT message,
                                            WPARAM word, LPARAM number) {
        MainWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            const auto* data = reinterpret_cast<CREATESTRUCTW*>(number);
            self = static_cast<MainWindow*>(data->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(self));
            self->window_ = window;
        } else {
            self = reinterpret_cast<MainWindow*>(
                GetWindowLongPtrW(window, GWLP_USERDATA));
        }
        return self == nullptr ? DefWindowProcW(window, message, word, number)
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
            case WM_GETMINMAXINFO: {
                RECT minimum{0, 0, 1060, 700};
                AdjustWindowRect(&minimum, WS_OVERLAPPEDWINDOW, FALSE);
                auto* limits = reinterpret_cast<MINMAXINFO*>(number);
                limits->ptMinTrackSize = {minimum.right - minimum.left,
                                          minimum.bottom - minimum.top};
                return 0;
            }
            case WM_DRAWITEM:
                if (reinterpret_cast<DRAWITEMSTRUCT*>(number)->CtlType ==
                    ODT_STATIC) {
                    drawSummary(*reinterpret_cast<DRAWITEMSTRUCT*>(number));
                } else {
                    drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(number));
                }
                return TRUE;
            case WM_SIZE:
                layout(LOWORD(number), HIWORD(number));
                return 0;
            case WM_COMMAND:
                command(LOWORD(word), HIWORD(word));
                return 0;
            case WM_NOTIFY:
                return notify(reinterpret_cast<NMHDR*>(number));
            case WM_CTLCOLORSTATIC:
                return controlColor(reinterpret_cast<HDC>(word),
                                    reinterpret_cast<HWND>(number));
            case WM_CTLCOLORLISTBOX:
            case WM_CTLCOLORBTN:
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

    HWND addControl(const wchar_t* classValue, const wchar_t* text,
                    const DWORD style, const int identifier) {
        const auto control = CreateWindowExW(
            0, classValue, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 100, 30,
            window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
            GetModuleHandleW(nullptr), nullptr);
        SetWindowTheme(control, L"", L"");
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_),
                     TRUE);
        return control;
    }

    HWND addLabel(const wchar_t* text, const int identifier = 0) {
        return addControl(L"STATIC", text, SS_LEFT | SS_NOPREFIX, identifier);
    }

    static LRESULT CALLBACK buttonProcedure(HWND window, UINT message,
                                            WPARAM word, LPARAM number,
                                            UINT_PTR identifier,
                                            DWORD_PTR data) {
        auto* self = reinterpret_cast<MainWindow*>(data);
        if (message == WM_MOUSEMOVE && self->hoveredButton_ != window) {
            self->hoveredButton_ = window;
            TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window,
                                  0};
            TrackMouseEvent(&track);
            InvalidateRect(window, nullptr, FALSE);
        } else if (message == WM_MOUSELEAVE) {
            if (self->hoveredButton_ == window) self->hoveredButton_ = nullptr;
            InvalidateRect(window, nullptr, FALSE);
        } else if (message == WM_NCDESTROY) {
            RemoveWindowSubclass(window, buttonProcedure, identifier);
        }
        return DefSubclassProc(window, message, word, number);
    }

    HWND addButton(const wchar_t* text, const int identifier) {
        const auto button =
            addControl(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, identifier);
        SetWindowSubclass(button, buttonProcedure, 1,
                          reinterpret_cast<DWORD_PTR>(this));
        return button;
    }

    HWND addEdit(const int identifier, const bool multiline = false) {
        DWORD style = ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER;
        if (multiline) {
            style = ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP |
                    WS_BORDER;
        }
        return addControl(L"EDIT", L"", style, identifier);
    }

    HWND addCombo(const int identifier) {
        return addControl(L"COMBOBOX", L"",
                          CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                          identifier);
    }

    void createControls() {
        font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH, L"Segoe UI");
        titleFont_ = CreateFontW(-23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH, L"Segoe UI");
        LOGFONTW section{};
        GetObjectW(font_, sizeof(section), &section);
        section.lfWeight = FW_SEMIBOLD;
        section.lfHeight = -18;
        sectionFont_ = CreateFontIndirectW(&section);
        heading_ = addLabel(L"Deadline Planner");
        SendMessageW(heading_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_),
                     TRUE);
        subtitle_ = addLabel(L"Задачи");
        summaryActive_ = addLabel(L"Активные\n0");
        summarySoon_ = addLabel(L"Скоро\n0");
        summaryOverdue_ = addLabel(L"Просрочены\n0");
        summaryDone_ = addLabel(L"Выполнены\n0");
        for (HWND card :
             {summaryActive_, summarySoon_, summaryOverdue_, summaryDone_}) {
            SetWindowLongPtrW(card, GWL_STYLE,
                              WS_CHILD | WS_VISIBLE | SS_OWNERDRAW);
        }

        projectFilter_ = addCombo(idProjectFilter);
        taskList_ = addControl(
            WC_LISTVIEWW, L"",
            LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
            idTaskList);
        ListView_SetExtendedListViewStyle(
            taskList_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(taskList_, surfaceColor);
        ListView_SetTextBkColor(taskList_, surfaceColor);
        ListView_SetTextColor(taskList_, textColor);
        SetWindowTheme(ListView_GetHeader(taskList_), L"", L"");
        rowHeightImages_ = ImageList_Create(1, 38, ILC_COLOR32, 1, 1);
        ListView_SetImageList(taskList_, rowHeightImages_, LVSIL_SMALL);
        SetWindowSubclass(ListView_GetHeader(taskList_), headerProcedure, 1, 0);
        addColumn(0, L"Задача", 270);
        addColumn(1, L"Проект", 145);
        addColumn(2, L"Срок", 145);
        addColumn(3, L"Тип", 115);
        addColumn(4, L"Осталось", 150);

        addButton_ = addButton(L"+ Добавить задачу", idAdd);
        completeButton_ = addButton(L"Выполнить", idComplete);
        checklistButton_ = addButton(L"Пункт чек-листа", idChecklist);
        reportsButton_ = addButton(L"Отчёты", idReports);
        refreshButton_ = addButton(L"Обновить", idRefresh);
        exportButton_ = addButton(L"Экспорт", idExport);
        propertiesHeading_ = addLabel(L"Свойства задачи");
        detailsTitle_ = addLabel(L"Выберите задачу");
        details_ =
            addControl(L"EDIT", L"Здесь появятся подробности и шкала срока.",
                       ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                           WS_VSCROLL | WS_TABSTOP,
                       idDetails);
        const int propertyTab = 52;
        SendMessageW(details_, EM_SETTABSTOPS, 1,
                     reinterpret_cast<LPARAM>(&propertyTab));
        SendMessageW(detailsTitle_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(sectionFont_), TRUE);
        emptyState_ = addLabel(
            L"Задач пока нет\r\n\r\n"
            L"Добавьте новую задачу или выберите другой проект.");
        progress_ = addControl(PROGRESS_CLASSW, L"", PBS_SMOOTH, idProgress);
        SendMessageW(progress_, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
        SendMessageW(progress_, PBM_SETBKCOLOR, 0, borderColor);

        formTitleLabel_ = addLabel(L"Новая задача");
        SendMessageW(formTitleLabel_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(sectionFont_), TRUE);
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
        formExtraLabel_ = addLabel(L"Дополнительные параметры");
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
        if (width <= 0 || height <= 0) return;
        constexpr int margin = 24;
        constexpr int toolbarTop = 84;
        constexpr int contentTop = 132;
        MoveWindow(heading_, margin, 24, 400, 30, TRUE);
        MoveWindow(addButton_, width - margin - 174, 21, 174, 36, TRUE);
        MoveWindow(reportsButton_, width - margin - 386, 21, 94, 36, TRUE);
        MoveWindow(exportButton_, width - margin - 280, 21, 94, 36, TRUE);
        MoveWindow(subtitle_, margin, toolbarTop + 5, 86, 24, TRUE);
        MoveWindow(projectFilter_, 120, toolbarTop, 210, 260, TRUE);
        MoveWindow(refreshButton_, 342, toolbarTop - 2, 106, 34, TRUE);
        const HWND summaries[]{summaryActive_, summarySoon_, summaryOverdue_,
                               summaryDone_};
        for (int index = 0; index < 4; ++index) {
            MoveWindow(summaries[index], margin + index * 190, height - 34, 180,
                       30, TRUE);
        }
        const int panelWidth = formVisible_ ? 400 : 360;
        const int listWidth = width - panelWidth - 1;
        const int panelX = listWidth + 1;
        MoveWindow(taskList_, 0, contentTop, listWidth,
                   height - contentTop - 42, TRUE);
        MoveWindow(emptyState_, margin, contentTop + 72, listWidth - margin * 2,
                   110, TRUE);
        const int typeWidth = listWidth < 740 ? 0 : 112;
        const int fixedColumns = 116 + 142 + typeWidth + 126;
        ListView_SetColumnWidth(taskList_, 0,
                                std::max(180, listWidth - fixedColumns));
        ListView_SetColumnWidth(taskList_, 1, 116);
        ListView_SetColumnWidth(taskList_, 2, 142);
        ListView_SetColumnWidth(taskList_, 3, typeWidth);
        ListView_SetColumnWidth(taskList_, 4, 126);
        if (!formVisible_) {
            MoveWindow(propertiesHeading_, panelX + margin, contentTop + 16,
                       panelWidth - margin * 2, 20, TRUE);
            MoveWindow(detailsTitle_, panelX + margin, contentTop + 48,
                       panelWidth - margin * 2, 52, TRUE);
            MoveWindow(progress_, panelX + margin, contentTop + 112,
                       panelWidth - margin * 2, 5, TRUE);
            MoveWindow(details_, panelX + margin, contentTop + 136,
                       panelWidth - margin * 2, height - 364, TRUE);
            MoveWindow(completeButton_, panelX + margin, height - 86, 136, 34,
                       TRUE);
            MoveWindow(checklistButton_, panelX + 172, height - 86,
                       panelWidth - 196, 34, TRUE);
        } else {
            layoutForm(panelX + margin, contentTop + 16,
                       panelWidth - margin * 2);
        }
        InvalidateRect(window_, nullptr, FALSE);
    }

    void layoutForm(const int x, int y, const int width) const {
        auto row = [&y, x, width](HWND label, HWND field, int fieldHeight) {
            MoveWindow(label, x, y, width, 20, TRUE);
            y += 22;
            MoveWindow(field, x, y, width, fieldHeight, TRUE);
            y += fieldHeight + 6;
        };
        const int half = (width - 12) / 2;
        auto pair = [&y, x, half](HWND label1, HWND field1, HWND label2,
                                  HWND field2, bool combo) {
            MoveWindow(label1, x, y, half, 20, TRUE);
            MoveWindow(label2, x + half + 12, y, half, 20, TRUE);
            MoveWindow(field1, x, y + 22, half, combo ? 180 : 28, TRUE);
            MoveWindow(field2, x + half + 12, y + 22, half, combo ? 180 : 28,
                       TRUE);
            y += 56;
        };
        MoveWindow(formTitleLabel_, x, y, width, 28, TRUE);
        y += 32;
        row(formNameLabel_, formTitle_, 28);
        row(formDeadlineLabel_, formDeadline_, 28);
        pair(formProjectLabel_, formProject_, formTypeLabel_, formType_, true);
        pair(formPriorityLabel_, formPriority_, formEstimateLabel_,
             formEstimate_, false);
        row(formExtraLabel_, formExtra_, 28);
        row(formDescriptionLabel_, formDescription_, 34);
        MoveWindow(formSave_, x, y, half, 34, TRUE);
        MoveWindow(formCancel_, x + half + 12, y, half, 34, TRUE);
    }

    void setFormVisible(const bool visible) {
        formVisible_ = visible;
        const int command = visible ? SW_SHOW : SW_HIDE;
        const HWND formControls[]{formTitleLabel_,  formNameLabel_,
                                  formTitle_,       formDeadlineLabel_,
                                  formDeadline_,    formProjectLabel_,
                                  formProject_,     formTypeLabel_,
                                  formType_,        formPriorityLabel_,
                                  formPriority_,    formEstimateLabel_,
                                  formEstimate_,    formExtraLabel_,
                                  formExtra_,       formDescriptionLabel_,
                                  formDescription_, formSave_,
                                  formCancel_};
        for (const auto control : formControls) {
            if (control != nullptr) {
                ShowWindow(control, command);
            }
        }
        const int detailsCommand = visible ? SW_HIDE : SW_SHOW;
        if (detailsTitle_ != nullptr) {
            ShowWindow(propertiesHeading_, detailsCommand);
            ShowWindow(detailsTitle_, detailsCommand);
            ShowWindow(details_, detailsCommand);
            ShowWindow(progress_, detailsCommand);
            ShowWindow(completeButton_, detailsCommand);
            ShowWindow(checklistButton_, detailsCommand);
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
        const auto selected =
            static_cast<int>(SendMessageW(projectFilter_, CB_GETCURSEL, 0, 0));
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
            projectFilter_, CB_SETCURSEL,
            static_cast<WPARAM>(
                selected >= 0 && selected < static_cast<int>(projectIds_.size())
                    ? selected
                    : 0),
            0);
        if (!formProjectIds_.empty()) {
            SendMessageW(formProject_, CB_SETCURSEL, 0, 0);
        }
    }

    void refreshTasks() {
        const auto selectedFilter =
            static_cast<int>(SendMessageW(projectFilter_, CB_GETCURSEL, 0, 0));
        TaskFilter filter;
        if (selectedFilter > 0 &&
            selectedFilter < static_cast<int>(projectIds_.size())) {
            filter.projectId =
                projectIds_[static_cast<std::size_t>(selectedFilter)];
        }
        const auto* previous = selectedTask();
        const auto previousId = previous ? previous->id() : std::string{};
        ListView_DeleteAllItems(taskList_);
        visibleTaskIds_.clear();
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
        ShowWindow(emptyState_, found.empty() ? SW_SHOW : SW_HIDE);
        const auto selected = std::find(visibleTaskIds_.begin(),
                                        visibleTaskIds_.end(), previousId);
        if (selected != visibleTaskIds_.end()) {
            const auto index = static_cast<int>(
                std::distance(visibleTaskIds_.begin(), selected));
            ListView_SetItemState(taskList_, index,
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
        }
    }

    void setCell(const int row, const int column, const std::wstring& value) {
        ListView_SetItemText(taskList_, row, column,
                             const_cast<wchar_t*>(value.c_str()));
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
            (L"В работе\r\n" + std::to_wstring(values.active)).c_str());
        SetWindowTextW(
            summarySoon_,
            (L"Ближайшие сроки\r\n" + std::to_wstring(values.dueSoon)).c_str());
        SetWindowTextW(
            summaryOverdue_,
            (L"Просрочены\r\n" + std::to_wstring(values.overdue)).c_str());
        SetWindowTextW(
            summaryDone_,
            (L"Завершены\r\n" + std::to_wstring(values.completed)).c_str());
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
        EnableWindow(completeButton_,
                     task != nullptr && !task->completedAt().has_value());
        EnableWindow(checklistButton_,
                     task != nullptr && task->type() == TaskType::checklist);
        if (task == nullptr) {
            SetWindowTextW(detailsTitle_, L"Выберите задачу");
            SetWindowTextW(
                details_,
                L"Нажмите на строку слева, чтобы увидеть подробности.");
            SendMessageW(progress_, PBM_SETPOS, 0, 0);
            return;
        }
        const auto scale = task->scale(DateTime::now());
        SetWindowTextW(detailsTitle_, wide(task->title()).c_str());
        std::wostringstream details;
        details << L"Проект\t" << projectTitle(task->projectId()) << L"\r\n"
                << L"Тип\t" << typeLabel(task->type()) << L"\r\n"
                << L"Срок\t" << wide(task->deadline().format()) << L"\r\n"
                << L"Приоритет\t" << task->priority() << L"\r\n"
                << L"Оценка\t" << task->estimate().minutes() << L" мин\r\n\r\n"
                << urgencyLabel(scale) << L"\r\n\r\n"
                << wide(task->description());
        if (task->type() == TaskType::recurring) {
            const auto& recurring = static_cast<const RecurringTask&>(*task);
            details << L"\r\nИнтервал: " << recurring.intervalDays()
                    << L" дн.\r\nВыполнений: " << recurring.completionCount();
        }
        if (task->type() == TaskType::checklist) {
            const auto& checklist = static_cast<const ChecklistTask&>(*task);
            details << L"\r\n\r\nПункты чек-листа\r\n";
            for (const auto& item : checklist.items()) {
                details << (item.done() ? L"✓ " : L"○ ") << wide(item.text())
                        << L"\r\n";
            }
        }
        SetWindowTextW(details_, details.str().c_str());
        const auto value = static_cast<int>(
            std::round(std::clamp(scale.elapsedShare, 0.0, 1.0) * 1000.0));
        SendMessageW(progress_, PBM_SETPOS, static_cast<WPARAM>(value), 0);
        const auto color =
            scale.urgency == DeadlineScale::Urgency::overdue  ? dangerColor
            : scale.urgency == DeadlineScale::Urgency::urgent ? warningColor
                                                              : mutedColor;
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
            } else if (identifier == idFormType &&
                       notification == CBN_SELCHANGE) {
                updateFormType();
            } else if (identifier == idProjectFilter &&
                       notification == CBN_SELCHANGE) {
                refreshTasks();
                updateDetails();
            }
        } catch (const std::exception& error) {
            MessageBoxW(window_, wide(error.what()).c_str(),
                        L"Не удалось выполнить действие",
                        MB_OK | MB_ICONWARNING);
        }
    }

    LRESULT notify(const NMHDR* header) const {
        if (header->idFrom != idTaskList) {
            return 0;
        }
        if (header->code == LVN_ITEMCHANGED) {
            updateDetails();
        }
        if (header->code == NM_CUSTOMDRAW) {
            auto* draw =
                reinterpret_cast<NMLVCUSTOMDRAW*>(const_cast<NMHDR*>(header));
            switch (draw->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:
                    return CDRF_NOTIFYSUBITEMDRAW;
                case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                    const int index = static_cast<int>(draw->nmcd.dwItemSpec);
                    const bool selected =
                        (ListView_GetItemState(taskList_, index,
                                               LVIS_SELECTED) &
                         LVIS_SELECTED) != 0;
                    draw->clrTextBk = selected ? selectedColor : surfaceColor;
                    draw->clrText = textColor;
                    if (draw->iSubItem == 4 && index >= 0 &&
                        index < static_cast<int>(visibleTaskIds_.size())) {
                        const auto& task = tasks_.find(
                            visibleTaskIds_[static_cast<std::size_t>(index)]);
                        const auto urgency =
                            task.scale(DateTime::now()).urgency;
                        draw->clrText =
                            urgency == DeadlineScale::Urgency::overdue
                                ? dangerColor
                            : urgency == DeadlineScale::Urgency::urgent
                                ? warningColor
                                : mutedColor;
                    }
                    return CDRF_NEWFONT;
                }
            }
        }
        return 0;
    }

    void updateFormType() const {
        const auto type = SendMessageW(formType_, CB_GETCURSEL, 0, 0);
        EnableWindow(formExtra_, type != 0);
        SetWindowTextW(formExtraLabel_,
                       type == 1   ? L"Повторять каждые … дней"
                       : type == 2 ? L"Пункты через точку с запятой"
                                   : L"Для этого типа параметры не нужны");
    }

    void openForm() {
        refreshProjects();
        SetWindowTextW(formTitle_, L"");
        SetWindowTextW(formDeadline_,
                       wide(DateTime::now().addDays(7).format()).c_str());
        SetWindowTextW(formExtra_, L"");
        SetWindowTextW(formDescription_, L"");
        SendMessageW(formType_, CB_SETCURSEL, 0, 0);
        updateFormType();
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
        const auto projectIndex =
            static_cast<int>(SendMessageW(formProject_, CB_GETCURSEL, 0, 0));
        require(projectIndex >= 0, "Выберите проект");
        draft.projectId =
            formProjectIds_[static_cast<std::size_t>(projectIndex)];
        const auto typeIndex =
            static_cast<int>(SendMessageW(formType_, CB_GETCURSEL, 0, 0));
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
            checklist.items().begin(), checklist.items().end(),
            [](const ChecklistItem& item) { return !item.done(); });
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
            output.write(reinterpret_cast<const char*>(bom),
                         static_cast<std::streamsize>(sizeof(bom)));
            output << document.content();
            require(output.good(), "Не удалось записать файл экспорта");
            created << L"• " << path.wstring() << L"\r\n";
        }
        MessageBoxW(window_, created.str().c_str(), L"Экспорт завершён",
                    MB_OK | MB_ICONINFORMATION);
    }

    static LRESULT CALLBACK headerProcedure(HWND window, UINT message,
                                            WPARAM word, LPARAM number,
                                            UINT_PTR identifier, DWORD_PTR) {
        if (message == WM_PAINT) {
            PAINTSTRUCT state{};
            HDC dc = BeginPaint(window, &state);
            RECT area{};
            GetClientRect(window, &area);
            const auto brush = CreateSolidBrush(surfaceColor);
            FillRect(dc, &area, brush);
            DeleteObject(brush);
            const auto old =
                SelectObject(dc, reinterpret_cast<HFONT>(
                                     SendMessageW(window, WM_GETFONT, 0, 0)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, mutedColor);
            for (int index = 0; index < Header_GetItemCount(window); ++index) {
                wchar_t text[128]{};
                HDITEMW item{};
                item.mask = HDI_TEXT;
                item.pszText = text;
                item.cchTextMax = 128;
                Header_GetItem(window, index, &item);
                RECT cell{};
                Header_GetItemRect(window, index, &cell);
                cell.left += 10;
                DrawTextW(dc, text, -1, &cell,
                          DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }
            SelectObject(dc, old);
            EndPaint(window, &state);
            return 0;
        }
        if (message == WM_NCDESTROY) {
            RemoveWindowSubclass(window, headerProcedure, identifier);
        }
        return DefSubclassProc(window, message, word, number);
    }

    void drawButton(const DRAWITEMSTRUCT& item) const {
        const bool primary = item.CtlID == idAdd || item.CtlID == idFormSave;
        const bool disabled = (item.itemState & ODS_DISABLED) != 0;
        const bool pressed = (item.itemState & ODS_SELECTED) != 0;
        const bool hovered = hoveredButton_ == item.hwndItem;
        const bool toolbar = item.CtlID == idReports ||
                             item.CtlID == idExport || item.CtlID == idRefresh;
        const auto base = toolbar ? backgroundColor : surfaceColor;
        const auto fill = disabled  ? base
                          : pressed ? selectedColor
                          : primary ? accentColor
                                    : base;
        const auto brush =
            CreateSolidBrush(hovered && !disabled && !pressed
                                 ? primary ? RGB(71, 122, 188) : selectedColor
                                 : fill);
        const auto pen = CreatePen(PS_SOLID, 1,
                                   primary && !disabled  ? accentColor
                                   : toolbar && !hovered ? base
                                                         : borderColor);
        const auto oldBrush = SelectObject(item.hDC, brush);
        const auto oldPen = SelectObject(item.hDC, pen);
        const auto oldFont = SelectObject(item.hDC, font_);
        RoundRect(item.hDC, item.rcItem.left, item.rcItem.top,
                  item.rcItem.right, item.rcItem.bottom, 6, 6);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, disabled ? mutedColor : textColor);
        auto bounds = item.rcItem;
        if (pressed) {
            OffsetRect(&bounds, 0, 1);
        }
        const auto text = windowText(item.hwndItem);
        DrawTextW(item.hDC, text.c_str(), -1, &bounds,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if ((item.itemState & ODS_FOCUS) != 0) {
            InflateRect(&bounds, -4, -4);
            DrawFocusRect(item.hDC, &bounds);
        }
        SelectObject(item.hDC, oldFont);
        SelectObject(item.hDC, oldPen);
        SelectObject(item.hDC, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    void drawSummary(const DRAWITEMSTRUCT& item) const {
        const auto text = windowText(item.hwndItem);
        const auto split = text.find(L'\n');
        FillRect(item.hDC, &item.rcItem, backgroundBrush_);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, mutedColor);
        auto label = item.rcItem;
        const auto old = SelectObject(item.hDC, font_);
        auto name = text.substr(0, split);
        if (!name.empty() && name.back() == L'\r') name.pop_back();
        DrawTextW(item.hDC, name.c_str(), -1, &label,
                  DT_SINGLELINE | DT_VCENTER);
        SIZE size{};
        GetTextExtentPoint32W(item.hDC, name.c_str(),
                              static_cast<int>(name.size()), &size);
        if (split != std::wstring::npos) {
            label.left += size.cx + 12;
            SetTextColor(item.hDC, item.hwndItem == summaryOverdue_
                                       ? dangerColor
                                       : textColor);
            DrawTextW(item.hDC, text.c_str() + split + 1, -1, &label,
                      DT_SINGLELINE | DT_VCENTER);
        }
        SelectObject(item.hDC, old);
    }

    LRESULT controlColor(const HDC context, const HWND control) const {
        SetBkMode(context, TRANSPARENT);
        SetTextColor(context, control == subtitle_ || control == emptyState_ ||
                                      control == propertiesHeading_
                                  ? mutedColor
                                  : textColor);
        return reinterpret_cast<LRESULT>(
            control == heading_ || control == subtitle_ ? backgroundBrush_
                                                        : surfaceBrush_);
    }

    void paint() const {
        PAINTSTRUCT state{};
        const auto context = BeginPaint(window_, &state);
        RECT area{};
        GetClientRect(window_, &area);
        FillRect(context, &area, backgroundBrush_);
        RECT content{0, 132, area.right, area.bottom - 42};
        FillRect(context, &content, surfaceBrush_);
        const int panelWidth = formVisible_ ? 400 : 360;
        RECT divider{area.right - panelWidth - 1, 132, area.right - panelWidth,
                     area.bottom - 42};
        RECT headerRule{0, 72, area.right, 73};
        RECT toolbarRule{0, 131, area.right, 132};
        RECT footerRule{0, area.bottom - 42, area.right, area.bottom - 41};
        const auto line = CreateSolidBrush(borderColor);
        for (const RECT* rule :
             {&divider, &headerRule, &toolbarRule, &footerRule}) {
            FillRect(context, rule, line);
        }
        DeleteObject(line);
        EndPaint(window_, &state);
    }

    TaskService& tasks_;
    [[maybe_unused]] ProjectService& projects_;
    HWND window_ = nullptr;
    HWND hoveredButton_ = nullptr;
    HFONT font_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT sectionFont_ = nullptr;
    HIMAGELIST rowHeightImages_ = nullptr;
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
    HWND emptyState_ = nullptr;
    HWND addButton_ = nullptr;
    HWND completeButton_ = nullptr;
    HWND checklistButton_ = nullptr;
    HWND reportsButton_ = nullptr;
    HWND refreshButton_ = nullptr;
    HWND exportButton_ = nullptr;
    HWND propertiesHeading_ = nullptr;
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

int runWin32Gui(TaskService& tasks, ProjectService& projects,
                const int showCommand) {
    MainWindow window(tasks, projects);
    return window.run(showCommand);
}

}  // namespace deadline::ui

#endif
