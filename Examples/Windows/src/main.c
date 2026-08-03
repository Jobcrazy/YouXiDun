#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shield_sdk.h"
#include "tcp_client.h"
#include "resource.h"

#define APP_CLASS_NAME "NetGuardWinDemo"
#define APP_TITLE "NetGuard SDK Demo"
#define APP_ID "ac7f95bb-1e4d-4186-8e01-e6334462a608"
#define WM_TCP_EVENT (WM_APP + 1)
#define MAX_LOG_ENTRIES 200

enum ControlId {
    ID_HOST = 100,
    ID_PORT,
    ID_CONNECT,
    ID_DISCONNECT,
    ID_LOG
};

typedef enum LogType {
    LOG_INFO,
    LOG_PING,
    LOG_PONG,
    LOG_ERROR,
    LOG_WARNING
} LogType;

typedef struct ConnectionContext {
    HWND window;
    unsigned generation;
} ConnectionContext;

typedef struct PostedTcpEvent {
    TcpEventType type;
    unsigned generation;
    char message[];
} PostedTcpEvent;

typedef struct AppState {
    HWND window;
    HWND title;
    HWND status;
    HWND host;
    HWND port;
    HWND connect_button;
    HWND disconnect_button;
    HWND log;
    HFONT title_font;
    HFONT ui_font;
    HFONT log_font;
    HBRUSH background_brush;
    HBRUSH input_brush;
    TcpClient *client;
    ConnectionContext *connection_context;
    unsigned generation;
} AppState;

static COLORREF color_background(void) { return RGB(13, 17, 23); }
static COLORREF color_input(void) { return RGB(22, 27, 34); }
static COLORREF color_text(void) { return RGB(224, 230, 237); }
static COLORREF color_blue(void) { return RGB(79, 195, 247); }
static COLORREF color_green(void) { return RGB(129, 199, 132); }
static COLORREF color_red(void) { return RGB(239, 83, 80); }
static COLORREF color_orange(void) { return RGB(255, 183, 77); }

static COLORREF log_color(LogType type) {
    switch (type) {
        case LOG_PING: return color_blue();
        case LOG_PONG: return color_green();
        case LOG_ERROR: return color_red();
        case LOG_WARNING: return color_orange();
        default: return RGB(176, 190, 197);
    }
}

static HFONT create_font(int height, int weight) {
    return CreateFontA(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
}

static void set_control_font(HWND control, HFONT font) {
    SendMessageA(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static void set_status(AppState *state, const char *text, BOOL connected) {
    SetWindowTextA(state->status, text);
    SetWindowLongPtrA(state->status, GWLP_USERDATA,
                      (LONG_PTR)(connected ? color_green() : color_red()));
    InvalidateRect(state->status, NULL, TRUE);
}

static void apply_disconnected_state(AppState *state, const char *text) {
    ShowWindow(state->connect_button, SW_SHOW);
    ShowWindow(state->disconnect_button, SW_HIDE);
    set_status(state, text, FALSE);
}

static void apply_connecting_state(AppState *state) {
    ShowWindow(state->connect_button, SW_HIDE);
    ShowWindow(state->disconnect_button, SW_SHOW);
    set_status(state, "Connecting...", FALSE);
}

static void apply_connected_state(AppState *state) {
    ShowWindow(state->connect_button, SW_HIDE);
    ShowWindow(state->disconnect_button, SW_SHOW);
    set_status(state, "Connected", TRUE);
}

static void resize_log_columns(AppState *state) {
    const int time_width = 104;
    RECT client;
    GetClientRect(state->log, &client);
    int available = client.right - client.left - time_width
                  - GetSystemMetrics(SM_CXVSCROLL) - 4;
    ListView_SetColumnWidth(state->log, 0, time_width);
    ListView_SetColumnWidth(state->log, 1, available > 100 ? available : 100);
    ShowScrollBar(state->log, SB_HORZ, FALSE);
}

static void append_log(AppState *state, const char *message, LogType type) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    char time_text[24];
    snprintf(time_text, sizeof(time_text), "%02u:%02u:%02u.%03u",
             now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);

    if (ListView_GetItemCount(state->log) >= MAX_LOG_ENTRIES) {
        ListView_DeleteItem(state->log, 0);
    }
    int row = ListView_GetItemCount(state->log);
    LVITEMA item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = row;
    item.pszText = time_text;
    item.lParam = (LPARAM)type;
    row = ListView_InsertItem(state->log, &item);
    ListView_SetItemText(state->log, row, 1, (char *)(message ? message : ""));
    resize_log_columns(state);
    ListView_EnsureVisible(state->log, row, FALSE);
}

static void destroy_client(AppState *state) {
    ++state->generation;
    if (state->client) {
        tcp_client_destroy(state->client);
        state->client = NULL;
    }
    free(state->connection_context);
    state->connection_context = NULL;
}

static void tcp_event_callback(TcpEventType type, const char *message, void *context) {
    ConnectionContext *connection = (ConnectionContext *)context;
    size_t length = strlen(message ? message : "");
    PostedTcpEvent *event = (PostedTcpEvent *)malloc(sizeof(*event) + length + 1u);
    if (!event) return;
    event->type = type;
    event->generation = connection->generation;
    memcpy(event->message, message ? message : "", length + 1u);
    if (!PostMessageA(connection->window, WM_TCP_EVENT, 0, (LPARAM)event)) free(event);
}

static void connect_client(AppState *state) {
    char host[256];
    char port_text[32];
    GetWindowTextA(state->host, host, sizeof(host));
    GetWindowTextA(state->port, port_text, sizeof(port_text));
    if (!host[0] || !port_text[0]) {
        append_log(state, "! Enter a host and port", LOG_WARNING);
        return;
    }
    char *end = NULL;
    long port = strtol(port_text, &end, 10);
    if (!end || *end != '\0' || port < 1 || port > 65535) {
        append_log(state, "! Port must be between 1 and 65535", LOG_WARNING);
        return;
    }

    destroy_client(state);
    ConnectionContext *context = (ConnectionContext *)calloc(1, sizeof(*context));
    TcpClient *client = context ? tcp_client_create(tcp_event_callback, context) : NULL;
    if (!context || !client) {
        free(context);
        tcp_client_destroy(client);
        append_log(state, "x Unable to allocate TCP client", LOG_ERROR);
        apply_disconnected_state(state, "Connection failed");
        return;
    }
    context->window = state->window;
    context->generation = state->generation;
    state->connection_context = context;
    state->client = client;

    char log_message[320];
    snprintf(log_message, sizeof(log_message), "> Connecting to %s:%ld", host, port);
    append_log(state, log_message, LOG_INFO);
    apply_connecting_state(state);
    if (!tcp_client_start(client, host, (unsigned short)port)) {
        append_log(state, "x Unable to start TCP worker", LOG_ERROR);
        destroy_client(state);
        apply_disconnected_state(state, "Connection failed");
    }
}

static void disconnect_client(AppState *state, BOOL show_log) {
    destroy_client(state);
    apply_disconnected_state(state, "Not connected");
    if (show_log) append_log(state, "- Disconnected manually", LOG_INFO);
}

static void handle_tcp_event(AppState *state, PostedTcpEvent *event) {
    if (event->generation != state->generation) return;
    char text[1024];
    switch (event->type) {
        case TCP_EVENT_CONNECTED:
            apply_connected_state(state);
            append_log(state, "+ Connected successfully", LOG_INFO);
            break;
        case TCP_EVENT_DISCONNECTED:
            snprintf(text, sizeof(text), "x Disconnected: %s", event->message);
            append_log(state, text, LOG_ERROR);
            destroy_client(state);
            apply_disconnected_state(state, "Disconnected");
            break;
        case TCP_EVENT_ERROR:
            snprintf(text, sizeof(text), "x %s", event->message);
            append_log(state, text, LOG_ERROR);
            destroy_client(state);
            apply_disconnected_state(state, "Connection failed");
            break;
        case TCP_EVENT_SENT:
            snprintf(text, sizeof(text), "^ %s", event->message);
            append_log(state, text, LOG_PING);
            break;
        case TCP_EVENT_RECEIVED:
            snprintf(text, sizeof(text), "v %s", event->message);
            append_log(state, text, LOG_PONG);
            break;
    }
}

static HWND create_control(DWORD ex_style, const char *class_name, const char *text,
                           DWORD style, int id, HWND parent) {
    return CreateWindowExA(ex_style, class_name, text, style | WS_CHILD,
                           0, 0, 0, 0, parent, (HMENU)(INT_PTR)id,
                           GetModuleHandleA(NULL), NULL);
}

static int create_interface(AppState *state) {
    state->background_brush = CreateSolidBrush(color_background());
    state->input_brush = CreateSolidBrush(color_input());
    state->title_font = create_font(22, FW_BOLD);
    state->ui_font = create_font(15, FW_NORMAL);
    state->log_font = create_font(13, FW_NORMAL);

    state->title = create_control(0, "STATIC", APP_TITLE, SS_LEFT | SS_CENTERIMAGE | WS_VISIBLE, 0, state->window);
    state->status = create_control(0, "STATIC", "Not connected", SS_RIGHT | SS_CENTERIMAGE | WS_VISIBLE, 0, state->window);
    state->host = create_control(WS_EX_CLIENTEDGE, "EDIT", "127.69.88.11",
                                 ES_AUTOHSCROLL | WS_TABSTOP | WS_VISIBLE, ID_HOST, state->window);
    state->port = create_control(WS_EX_CLIENTEDGE, "EDIT", "10000",
                                 ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP | WS_VISIBLE, ID_PORT, state->window);
    state->connect_button = create_control(0, "BUTTON", "Connect",
                                           BS_OWNERDRAW | WS_TABSTOP | WS_VISIBLE, ID_CONNECT, state->window);
    state->disconnect_button = create_control(0, "BUTTON", "Disconnect",
                                              BS_OWNERDRAW | WS_TABSTOP, ID_DISCONNECT, state->window);
    state->log = create_control(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                                LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS | WS_TABSTOP | WS_VISIBLE,
                                ID_LOG, state->window);
    if (!state->title || !state->status || !state->host || !state->port ||
        !state->connect_button || !state->disconnect_button || !state->log) return 0;

    set_control_font(state->title, state->title_font);
    set_control_font(state->status, state->ui_font);
    set_control_font(state->host, state->ui_font);
    set_control_font(state->port, state->ui_font);
    set_control_font(state->connect_button, state->ui_font);
    set_control_font(state->disconnect_button, state->ui_font);
    set_control_font(state->log, state->log_font);
    SendMessageW(state->host, EM_SETCUEBANNER, FALSE, (LPARAM)L"Host");
    SendMessageW(state->port, EM_SETCUEBANNER, FALSE, (LPARAM)L"Port");

    ListView_SetExtendedListViewStyle(state->log, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(state->log, color_background());
    ListView_SetTextBkColor(state->log, color_background());
    LVCOLUMNA column;
    memset(&column, 0, sizeof(column));
    column.mask = LVCF_WIDTH;
    column.cx = 104;
    ListView_InsertColumn(state->log, 0, &column);
    column.cx = 600;
    ListView_InsertColumn(state->log, 1, &column);
    return 1;
}

static void layout_interface(AppState *state, int width, int height) {
    const int margin = 14;
    const int header_height = 34;
    const int row_height = 42;
    const int gap = 8;
    int y = margin;
    MoveWindow(state->title, margin, y, width * 2 / 3 - margin, header_height, TRUE);
    MoveWindow(state->status, width * 2 / 3, y, width / 3 - margin, header_height, TRUE);
    y += header_height + gap;
    int port_width = 120;
    MoveWindow(state->host, margin, y, width - 2 * margin - port_width - gap, row_height, TRUE);
    MoveWindow(state->port, width - margin - port_width, y, port_width, row_height, TRUE);
    y += row_height + gap;
    MoveWindow(state->connect_button, margin, y, width - 2 * margin, row_height, TRUE);
    MoveWindow(state->disconnect_button, margin, y, width - 2 * margin, row_height, TRUE);
    y += row_height + 12;
    MoveWindow(state->log, margin, y, width - 2 * margin, height - y - margin, TRUE);
    resize_log_columns(state);
}

static void draw_button(const DRAWITEMSTRUCT *draw) {
    char text[32];
    GetWindowTextA(draw->hwndItem, text, sizeof(text));
    BOOL disconnect = draw->CtlID == ID_DISCONNECT;
    COLORREF background = disconnect ? color_red() : color_blue();
    if (draw->itemState & ODS_SELECTED) {
        background = disconnect ? RGB(210, 65, 62) : RGB(54, 160, 208);
    }
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(draw->hDC, &draw->rcItem, brush);
    DeleteObject(brush);
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, color_background());
    HFONT old = (HFONT)SelectObject(draw->hDC, (HFONT)SendMessageA(draw->hwndItem, WM_GETFONT, 0, 0));
    DrawTextA(draw->hDC, text, -1, (RECT *)&draw->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(draw->hDC, old);
    if (draw->itemState & ODS_FOCUS) DrawFocusRect(draw->hDC, &draw->rcItem);
}

static LRESULT handle_log_custom_draw(NMLVCUSTOMDRAW *draw) {
    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        draw->clrText = log_color((LogType)draw->nmcd.lItemlParam);
        draw->clrTextBk = color_background();
        return CDRF_NEWFONT;
    }
    return CDRF_DODEFAULT;
}

static void initialize_sdk(AppState *state) {
    char error[256] = {0};
    int result = shield_sdk_init("Shield.dll", APP_ID, error, sizeof(error));
    if (result == 0) {
        append_log(state, "SDK initialized successfully", LOG_INFO);
    } else if (error[0]) {
        append_log(state, error, LOG_ERROR);
    } else {
        char message[96];
        snprintf(message, sizeof(message), "SDK initialization failed: %d", result);
        append_log(state, message, LOG_ERROR);
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    AppState *state = (AppState *)GetWindowLongPtrA(window, GWLP_USERDATA);
    switch (message) {
        case WM_CREATE: {
            CREATESTRUCTA *create = (CREATESTRUCTA *)lparam;
            state = (AppState *)create->lpCreateParams;
            state->window = window;
            SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)state);
            if (!create_interface(state)) return -1;
            apply_disconnected_state(state, "Not connected");
            initialize_sdk(state);
            return 0;
        }
        case WM_SIZE:
            if (state) layout_interface(state, LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_COMMAND:
            if (!state || HIWORD(wparam) != BN_CLICKED) break;
            if (LOWORD(wparam) == ID_CONNECT) connect_client(state);
            else if (LOWORD(wparam) == ID_DISCONNECT) disconnect_client(state, TRUE);
            return 0;
        case WM_DRAWITEM:
            draw_button((const DRAWITEMSTRUCT *)lparam);
            return TRUE;
        case WM_NOTIFY:
            if (state && ((NMHDR *)lparam)->hwndFrom == state->log &&
                ((NMHDR *)lparam)->code == NM_CUSTOMDRAW) {
                return handle_log_custom_draw((NMLVCUSTOMDRAW *)lparam);
            }
            break;
        case WM_CTLCOLORSTATIC:
            if (state) {
                HDC dc = (HDC)wparam;
                SetBkColor(dc, color_background());
                if ((HWND)lparam == state->status) {
                    SetTextColor(dc, (COLORREF)GetWindowLongPtrA(state->status, GWLP_USERDATA));
                } else {
                    SetTextColor(dc, color_text());
                }
                return (LRESULT)state->background_brush;
            }
            break;
        case WM_CTLCOLOREDIT:
            if (state) {
                HDC dc = (HDC)wparam;
                SetBkColor(dc, color_input());
                SetTextColor(dc, color_text());
                return (LRESULT)state->input_brush;
            }
            break;
        case WM_TCP_EVENT:
            if (state) handle_tcp_event(state, (PostedTcpEvent *)lparam);
            free((void *)lparam);
            return 0;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            if (state) destroy_client(state);
            PostQuitMessage(0);
            return 0;
        case WM_NCDESTROY:
            if (state) {
                DeleteObject(state->title_font);
                DeleteObject(state->ui_font);
                DeleteObject(state->log_font);
                DeleteObject(state->background_brush);
                DeleteObject(state->input_brush);
                SetWindowLongPtrA(window, GWLP_USERDATA, 0);
            }
            return DefWindowProcA(window, message, wparam, lparam);
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show_command) {
    (void)previous;
    (void)command_line;
    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        MessageBoxA(NULL, "Unable to initialize Winsock.", APP_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    AppState state;
    memset(&state, 0, sizeof(state));
    WNDCLASSEXA window_class;
    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(IDI_APP_ICON));
    window_class.hIconSm = (HICON)LoadImageA(instance, MAKEINTRESOURCEA(IDI_APP_ICON),
                                             IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    window_class.hbrBackground = CreateSolidBrush(color_background());
    window_class.lpszClassName = APP_CLASS_NAME;
    if (!RegisterClassExA(&window_class)) {
        WSACleanup();
        return 1;
    }

    HWND window = CreateWindowExA(0, APP_CLASS_NAME, APP_TITLE,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 780, 620,
                                  NULL, NULL, instance, &state);
    if (!window) {
        DeleteObject(window_class.hbrBackground);
        WSACleanup();
        return 1;
    }
    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    DeleteObject(window_class.hbrBackground);
    WSACleanup();
    return (int)message.wParam;
}
