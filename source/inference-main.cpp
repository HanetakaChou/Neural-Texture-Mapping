#include <tensorflow/lite/c/c_api.h>
#include <tensorflow/lite/delegates/gpu/delegate.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <assert.h>
#include <stdio.h>

#if defined(__GNUC__)
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
#include <xcb/xcb.h>
#include <xcb/present.h>
#include <time.h>
#elif defined(_MSC_VER)
// https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#include <sdkddkver.h>
// https://docs.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers#faster-builds-with-smaller-header-files
#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOMINMAX
#include <Windows.h>
#else
#error Unknown Compiler
#endif

static inline void tflite_error_reporter(void *, const char *format, va_list args);

static inline void tflite_predict(uint8_t (*out_bit_RGBs)[4], int texture_width, int texture_height, TfLiteInterpreter *tflite_interpreter, float *tflite_input, float *tflite_output);

#if defined(__GNUC__)
int main(int argc, char *argv[], char *envp[])
#elif defined(_MSC_VER)
struct window_data
{
    int texture_width;
    int texture_height;
    HDC device_context;
    HDC memory_device_context;
    HBITMAP bitmap;
    uint8_t (*bit_RGBs)[4];
    double performance_frequency;
    double performance_count;
    TfLiteInterpreter *tflite_interpreter;
    float *tflite_input;
    float *tflite_output;
};

static LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

extern "C" IMAGE_DOS_HEADER __ImageBase;

int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
#else
#error Unknown Compiler
#endif
{
    // Data
    constexpr int const texture_width = 512;
    constexpr int const texture_height = 512;

    // Model
    TfLiteModel *tflite_model = NULL;
    {
        // NOTE: the memory of the "model_data" must remain valid as long as the "TfLiteModel" is still in use.
        // We use "static" keyword for convenience
        static uint8_t tflite_model_data[] = {
#include "neural-texture-mapping.inl"
        };

        tflite_model = TfLiteModelCreateWithErrorReporter(tflite_model_data, sizeof(tflite_model_data), tflite_error_reporter, NULL);
    }
    assert(tflite_model);

    TfLiteDelegate *tflite_delegate = NULL;
    {
        TfLiteGpuDelegateOptionsV2 tflite_delegate_options = TfLiteGpuDelegateOptionsV2Default();
        tflite_delegate_options.experimental_flags = TFLITE_GPU_EXPERIMENTAL_FLAGS_NONE;

        tflite_delegate = TfLiteGpuDelegateV2Create(&tflite_delegate_options);
    }
    assert(tflite_delegate);

    TfLiteInterpreter *tflite_interpreter = NULL;
    {
        TfLiteInterpreterOptions *tflite_interpreter_options = TfLiteInterpreterOptionsCreate();
        assert(tflite_interpreter_options);

        TfLiteInterpreterOptionsAddDelegate(tflite_interpreter_options, tflite_delegate);

        tflite_interpreter = TfLiteInterpreterCreate(tflite_model, tflite_interpreter_options);
        assert(tflite_interpreter);

        TfLiteInterpreterOptionsDelete(tflite_interpreter_options);
    }

    {
        int tflite_input_dims[2] = {texture_width * texture_height, 2};
        TfLiteStatus tflite_status_resize_input_tensor = TfLiteInterpreterResizeInputTensor(tflite_interpreter, 0, tflite_input_dims, sizeof(tflite_input_dims) / sizeof(tflite_input_dims[0]));
        assert(kTfLiteOk == tflite_status_resize_input_tensor);
    }

    {
        TfLiteStatus tflite_status_allocate_tensors = TfLiteInterpreterAllocateTensors(tflite_interpreter);
        assert(kTfLiteOk == tflite_status_allocate_tensors);
    }

    float *tflite_input = TfLiteInterpreterGetInputTensor(tflite_interpreter, 0)->data.f;
    float *tflite_output = TfLiteInterpreterGetOutputTensor(tflite_interpreter, 0)->data.f;

    std::vector<uint8_t[4]> bit_RGBs(static_cast<size_t>(texture_width * texture_height));

#if defined(__GNUC__)
    xcb_connection_t *connection = NULL;
    xcb_screen_t *screen = NULL;
    {
        int screen_number;
        connection = xcb_connect(NULL, &screen_number);
        assert(0 == xcb_connection_has_error(connection));

        xcb_setup_t const *setup = xcb_get_setup(connection);

        int i = 0;
        for (xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(setup); screen_iterator.rem > 0; xcb_screen_next(&screen_iterator))
        {
            if (i == screen_number)
            {
                screen = screen_iterator.data;
                break;
            }
            ++i;
        }
    }

    constexpr uint8_t const depth = 32;
    xcb_visualid_t visual_id = 0;
    {
        for (xcb_depth_iterator_t depth_iterator = xcb_screen_allowed_depths_iterator(screen); depth_iterator.rem > 0; xcb_depth_next(&depth_iterator))
        {
            if (depth == depth_iterator.data->depth)
            {
                for (xcb_visualtype_iterator_t visual_iterator = xcb_depth_visuals_iterator(depth_iterator.data); visual_iterator.rem > 0; xcb_visualtype_next(&visual_iterator))
                {
                    if (XCB_VISUAL_CLASS_TRUE_COLOR == visual_iterator.data->_class)
                    {
                        visual_id = visual_iterator.data->visual_id;
                        break;
                    }
                }
                break;
            }
        }
    }

    uint8_t present_extension_opcode = 0;
    {
        xcb_present_query_version_cookie_t cookie_present_query_version = xcb_present_query_version(connection, 1, 0);

        xcb_generic_error_t *error_reply_present_query_version;
        xcb_present_query_version_reply_t *reply_present_query_version = xcb_present_query_version_reply(connection, cookie_present_query_version, &error_reply_present_query_version);
        assert(NULL == error_reply_present_query_version);

        assert(reply_present_query_version->major_version >= 1);
        assert(reply_present_query_version->minor_version >= 0);

        free(reply_present_query_version);

        present_extension_opcode = xcb_get_extension_data(connection, &xcb_present_id)->major_opcode;
    }

    xcb_colormap_t colormap = 0;
    {
        colormap = xcb_generate_id(connection);

        xcb_void_cookie_t cookie_create_colormap = xcb_create_colormap_checked(connection, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, visual_id);

        xcb_generic_error_t *error_create_colormap = xcb_request_check(connection, cookie_create_colormap);
        assert(NULL == error_create_colormap);
    }

    xcb_window_t window = 0;
    {
        window = xcb_generate_id(connection);

        // Both "border pixel" and "colormap" are required when the depth is NOT equal to the root window's.
        uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_BACKING_STORE | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;

        uint32_t value_list[5] = {screen->black_pixel, 0, XCB_BACKING_STORE_NOT_USEFUL, XCB_EVENT_MASK_EXPOSURE, colormap};

        xcb_void_cookie_t cookie_create_window = xcb_create_window_checked(connection, depth, window, screen->root, 0, 0, texture_width, texture_height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visual_id, value_mask, value_list);

        xcb_generic_error_t *error_create_window = xcb_request_check(connection, cookie_create_window);
        assert(NULL == error_create_window);
    }

    xcb_atom_t atom_wm_protocols = 0;
    xcb_atom_t atom_wm_delete_window = 0;
    {
        xcb_intern_atom_cookie_t cookie_wm_protocols = xcb_intern_atom(connection, 0, 12U, "WM_PROTOCOLS");

        xcb_intern_atom_cookie_t cookie_wm_delete_window = xcb_intern_atom(connection, 0, 16U, "WM_DELETE_WINDOW");

        xcb_generic_error_t *error_intern_atom_reply_wm_protocols;
        xcb_intern_atom_reply_t *reply_wm_protocols = xcb_intern_atom_reply(connection, cookie_wm_protocols, &error_intern_atom_reply_wm_protocols);
        assert(NULL == error_intern_atom_reply_wm_protocols);
        atom_wm_protocols = reply_wm_protocols->atom;
        free(error_intern_atom_reply_wm_protocols);

        xcb_generic_error_t *error_intern_atom_reply_wm_delete_window;
        xcb_intern_atom_reply_t *reply_wm_delete_window = xcb_intern_atom_reply(connection, cookie_wm_delete_window, &error_intern_atom_reply_wm_delete_window);
        assert(NULL == error_intern_atom_reply_wm_delete_window);
        atom_wm_delete_window = reply_wm_delete_window->atom;
        free(error_intern_atom_reply_wm_delete_window);
    }

    {
        xcb_void_cookie_t cookie_change_property_wm_name = xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8 * sizeof(uint8_t), 22, "Neural-Texture-Mapping");

        // xcb/xcb_icccm.h
        constexpr uint32_t const ICCCM_SIZE_HINT_P_MIN_SIZE = 1 << 4;
        constexpr uint32_t const ICCCM_SIZE_HINT_P_MAX_SIZE = 1 << 5;
        struct
        {
            uint32_t flags;
            int32_t x, y;
            int32_t width, height;
            int32_t min_width, min_height;
            int32_t max_width, max_height;
            int32_t width_inc, height_inc;
            int32_t min_aspect_num, min_aspect_den;
            int32_t max_aspect_num, max_aspect_den;
            int32_t base_width, base_height;
            uint32_t win_gravity;
        } size_hints = {};
        size_hints.flags = ICCCM_SIZE_HINT_P_MIN_SIZE | ICCCM_SIZE_HINT_P_MAX_SIZE;
        size_hints.min_width = texture_width;
        size_hints.min_height = texture_height;
        size_hints.max_width = texture_width;
        size_hints.max_height = texture_height;
        xcb_void_cookie_t cookie_change_property_size_hints = xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 8 * sizeof(uint32_t), sizeof(size_hints) / sizeof(uint32_t), &size_hints);

        xcb_void_cookie_t cookie_change_property_wm_protocols_delete_window = xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, window, atom_wm_protocols, XCB_ATOM_ATOM, 8 * sizeof(uint32_t), sizeof(xcb_atom_t) / sizeof(uint32_t), &atom_wm_delete_window);

        xcb_generic_error_t *error_change_property_net_wm_name = xcb_request_check(connection, cookie_change_property_wm_name);
        assert(NULL == error_change_property_net_wm_name);

        xcb_generic_error_t *error_change_property_size_hints = xcb_request_check(connection, cookie_change_property_size_hints);
        assert(NULL == error_change_property_size_hints);

        xcb_generic_error_t *error_change_property_wm_protocols_delete_window = xcb_request_check(connection, cookie_change_property_wm_protocols_delete_window);
        assert(NULL == error_change_property_wm_protocols_delete_window);
    }

    xcb_present_event_t present_event = 0;
    {
        present_event = xcb_generate_id(connection);

        xcb_void_cookie_t cookie_present_select_input = xcb_present_select_input_checked(connection, present_event, window, XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY);

        xcb_generic_error_t *error_present_select_input = xcb_request_check(connection, cookie_present_select_input);
        assert(NULL == error_present_select_input);
    }

    {
        xcb_void_cookie_t cookie_map_window = xcb_map_window_checked(connection, window);

        xcb_generic_error_t *error_map_window = xcb_request_check(connection, cookie_map_window);
        assert(NULL == error_map_window);
    }

    xcb_gcontext_t graphics_context = 0;
    {
        graphics_context = xcb_generate_id(connection);

        uint32_t value_mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_SUBWINDOW_MODE | XCB_GC_GRAPHICS_EXPOSURES;

        uint32_t value_list[4] = {screen->white_pixel, screen->black_pixel, XCB_SUBWINDOW_MODE_CLIP_BY_CHILDREN | 1};

        xcb_void_cookie_t cookie_create_graphics_context = xcb_create_gc_checked(connection, graphics_context, window, value_mask, value_list);

        xcb_generic_error_t *error_create_graphics_context = xcb_request_check(connection, cookie_create_graphics_context);
        assert(NULL == error_create_graphics_context);
    }

    xcb_pixmap_t pixmap = 0;
    {
        pixmap = xcb_generate_id(connection);

        xcb_void_cookie_t cookie_create_pixmap = xcb_create_pixmap_checked(connection, depth, pixmap, window, texture_width, texture_height);

        xcb_generic_error_t *error_create_pixmap = xcb_request_check(connection, cookie_create_pixmap);
        assert(NULL == error_create_pixmap);
    }

    struct timespec time_monotonic;
    clock_gettime(CLOCK_MONOTONIC, &time_monotonic);

    bool quit = false;
    xcb_generic_event_t *event;

    while ((!quit) && ((event = xcb_wait_for_event(connection)) != NULL))
    {
        // The most significant bit(uint8_t(0X80)) in this code is set if the event was generated from a SendEvent request.
        // https://www.x.org/releases/current/doc/xproto/x11protocol.html#event_format
        switch (event->response_type & (~uint8_t(0X80)))
        {
        case XCB_GE_GENERIC:
        {
            // wl_surface::frame

            assert(XCB_GE_GENERIC == (event->response_type & (~uint8_t(0X80))));

            xcb_ge_generic_event_t *ge_generic_event = reinterpret_cast<xcb_ge_generic_event_t *>(event);

            assert(present_extension_opcode == ge_generic_event->extension && XCB_PRESENT_COMPLETE_NOTIFY == ge_generic_event->event_type);

            xcb_present_complete_notify_event_t *present_complete_notify_event = reinterpret_cast<xcb_present_complete_notify_event_t *>(event);

            assert(present_complete_notify_event->event == present_event);
        }
        case XCB_EXPOSE:
        case XCB_GRAPHICS_EXPOSURE:
        case XCB_NO_EXPOSURE:
        {
            // FPS
            double fps = -1.0;
            {
                struct timespec new_time_monotonic;
                clock_gettime(CLOCK_MONOTONIC, &new_time_monotonic);

                fps = 1E9 / (1E9 * (new_time_monotonic.tv_sec - time_monotonic.tv_sec) + (new_time_monotonic.tv_nsec - time_monotonic.tv_nsec));

                time_monotonic = new_time_monotonic;
            }

            // Inference
            tflite_predict(&bit_RGBs[0], texture_width, texture_height, tflite_interpreter, tflite_input, tflite_output);

#ifdef NDEBUG
            // write "texture" into "back buffer"
            xcb_put_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, graphics_context, texture_width, texture_height, 0, 0, 0, depth, bit_RGBs.size() * sizeof(bit_RGBs[0]), &bit_RGBs[0][0]);

            // write "text" into "back buffer"
            {
                char fps_string[64];
                int fps_string_length = sprintf(fps_string, "FPS: %d", static_cast<int>(fps));

                xcb_image_text_8(connection, fps_string_length, pixmap, graphics_context, 7, 17, fps_string);
            }

            // copy from "back-buffer" into "front buffer"
            xcb_present_pixmap(connection, window, pixmap, 0, XCB_NONE, XCB_NONE, 0, 0, XCB_NONE, XCB_NONE, XCB_NONE, XCB_PRESENT_OPTION_NONE, 0, 0, 0, 0, NULL);

            int result_flush = xcb_flush(connection);
            assert(result_flush > 0);

#else
            // write "texture" into "back buffer"
            xcb_void_cookie_t cookie_put_image = xcb_put_image_checked(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, graphics_context, texture_width, texture_height, 0, 0, 0, depth, bit_RGBs.size() * sizeof(bit_RGBs[0]), &bit_RGBs[0][0]);

            // write "text" into "back buffer"
            xcb_void_cookie_t cookie_image_text = {};
            {
                char fps_string[64];
                int fps_string_length = sprintf(fps_string, "FPS: %d", static_cast<int>(fps));

                cookie_image_text = xcb_image_text_8_checked(connection, fps_string_length, pixmap, graphics_context, 7, 17, fps_string);
            }

            // copy from "back-buffer" into "front buffer"
            xcb_void_cookie_t cookie_present_pixmap = xcb_present_pixmap_checked(connection, window, pixmap, 0, XCB_NONE, XCB_NONE, 0, 0, XCB_NONE, XCB_NONE, XCB_NONE, XCB_PRESENT_OPTION_NONE, 0, 0, 0, 0, NULL);

            xcb_generic_error_t *error_put_image = xcb_request_check(connection, cookie_put_image);
            assert(NULL == error_put_image);

            xcb_generic_error_t *error_image_text = xcb_request_check(connection, cookie_image_text);
            assert(NULL == error_image_text);

            xcb_generic_error_t *error_present_pixmap = xcb_request_check(connection, cookie_present_pixmap);
            assert(NULL == error_present_pixmap);
#endif
        }
        break;
        case XCB_CLIENT_MESSAGE:
        {
            assert(XCB_CLIENT_MESSAGE == (event->response_type & (~uint8_t(0X80))));

            xcb_client_message_event_t *client_message_event = reinterpret_cast<xcb_client_message_event_t *>(event);
            assert(client_message_event->type == atom_wm_protocols && client_message_event->data.data32[0] == atom_wm_delete_window && client_message_event->window == window);

            quit = true;
        }
        break;
        case 0:
        {
            assert(0 == (event->response_type & (~uint8_t(0X80))));

            xcb_generic_error_t *error = reinterpret_cast<xcb_generic_error_t *>(event);

            printf("Error Code: %d Major Code: %d", static_cast<int>(error->error_code), static_cast<int>(error->major_code));
        }
        break;
        }

        free(event);
    }

    {
        xcb_void_cookie_t cookie_free_pixmap = xcb_free_pixmap_checked(connection, pixmap);

        xcb_generic_error_t *error_free_pixmap = xcb_request_check(connection, cookie_free_pixmap);
        assert(NULL == error_free_pixmap);
    }

    {
        xcb_void_cookie_t cookie_free_graphics_context = xcb_free_gc_checked(connection, graphics_context);

        xcb_generic_error_t *error_free_graphics_context = xcb_request_check(connection, cookie_free_graphics_context);
        assert(NULL == error_free_graphics_context);
    }

    {
        xcb_void_cookie_t cookie_free_colormap = xcb_free_colormap_checked(connection, colormap);

        xcb_generic_error_t *error_free_colormap = xcb_request_check(connection, cookie_free_colormap);
        assert(NULL == error_free_colormap);
    }

    xcb_disconnect(connection);

#elif defined(_MSC_VER)
    window_data window_data_instance;

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);

    ATOM atom = 0;
    {
        WNDCLASSEXW wc;
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WindowProcedure;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = sizeof(window_data *);
        wc.hInstance = instance;
        wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = L"Neural-Texture-Mapping";
        wc.hIconSm = wc.hIcon;
        atom = RegisterClassExW(&wc);
    }
    assert(0 != atom);

    HWND window;
    {
        constexpr DWORD const dw_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        constexpr DWORD const dw_ex_style = WS_EX_APPWINDOW;

        RECT rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = texture_width;
        rect.bottom = texture_height;
        BOOL result_adjust_window_rect = AdjustWindowRectEx(&rect, dw_style, FALSE, dw_ex_style);
        assert(FALSE != result_adjust_window_rect);

        int const n_width = rect.right - rect.left;
        int const n_height = rect.bottom - rect.top;

        window = CreateWindowExW(dw_ex_style, MAKEINTATOM(atom), L"Neural-Texture-Mapping", dw_style, CW_USEDEFAULT, CW_USEDEFAULT, n_width, n_height, NULL, NULL, instance, &window_data_instance);
    }
    assert(NULL != window);

    HDC device_context = GetDC(window);
    assert(NULL != device_context);

    HDC memory_device_context = CreateCompatibleDC(device_context);
    assert(NULL != memory_device_context);

    HBITMAP bitmap = CreateCompatibleBitmap(device_context, texture_width, texture_height);
    assert(NULL != bitmap);

    double performance_frequency = -1.0;
    {
        LARGE_INTEGER int64_frequency;
        BOOL result_query_performance_frequency = QueryPerformanceFrequency(&int64_frequency);
        assert(NULL != result_query_performance_frequency);

        performance_frequency = static_cast<double>(int64_frequency.QuadPart);
    }

    double performance_count = -1.0;
    {
        LARGE_INTEGER int64_performance_count;
        BOOL result_query_performance_counter = QueryPerformanceCounter(&int64_performance_count);
        assert(NULL != result_query_performance_counter);

        performance_count = static_cast<double>(int64_performance_count.QuadPart);
    }

    window_data_instance.texture_width = texture_width;
    window_data_instance.texture_height = texture_height;
    window_data_instance.device_context = device_context;
    window_data_instance.memory_device_context = memory_device_context;
    window_data_instance.bitmap = bitmap;
    window_data_instance.bit_RGBs = &bit_RGBs[0];
    window_data_instance.performance_frequency = performance_frequency;
    window_data_instance.performance_count = performance_count;
    window_data_instance.tflite_interpreter = tflite_interpreter;
    window_data_instance.tflite_input = tflite_input;
    window_data_instance.tflite_output = tflite_output;

    ShowWindow(window, SW_SHOWDEFAULT);

    {
        BOOL result_update_window = UpdateWindow(window);
        assert(FALSE != result_update_window);
    }

    {
        MSG msg;
        while (FALSE != GetMessageW(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    {
        BOOL result_delete_object = DeleteObject(bitmap);
        assert(FALSE != result_delete_object);
    }

    {
        BOOL result_delete_dc = DeleteDC(memory_device_context);
        assert(FALSE != result_delete_dc);
    }

#else
#error Unknown Compiler
#endif

    TfLiteInterpreterDelete(tflite_interpreter);
    TfLiteGpuDelegateV2Delete(tflite_delegate);
    TfLiteModelDelete(tflite_model);

    return 0;
}

static inline void tflite_error_reporter(void *, const char *format, va_list args)
{
    vprintf(format, args);
    return;
}

static inline void tflite_predict(uint8_t (*out_bit_RGBs)[4], int texture_width, int texture_height, TfLiteInterpreter *tflite_interpreter, float *tflite_input, float *tflite_output)
{
    float(*input_UVs)[2] = reinterpret_cast<float(*)[2]>(tflite_input);
    for (int h = 0; h < texture_height; ++h)
    {
        for (int w = 0; w < texture_width; ++w)
        {
            input_UVs[texture_width * h + w][0] = (w + 0.5F) / texture_width;
            input_UVs[texture_width * h + w][1] = (h + 0.5F) / texture_height;
        }
    }

    TfLiteStatus tflite_status_invoke = TfLiteInterpreterInvoke(tflite_interpreter);
    assert(kTfLiteOk == tflite_status_invoke);

    float(*prediction_RGBs)[3] = reinterpret_cast<float(*)[3]>(tflite_output);

    for (int h = 0; h < texture_height; ++h)
    {
        for (int w = 0; w < texture_width; ++w)
        {
            float R = prediction_RGBs[texture_width * h + w][0] * 255.0F;
            float G = prediction_RGBs[texture_width * h + w][1] * 255.0F;
            float B = prediction_RGBs[texture_width * h + w][2] * 255.0F;

            if (R > 255.0F)
            {
                R = 255.0F;
            }
            if (G > 255.0F)
            {
                G = 255.0F;
            }
            if (B > 255.0F)
            {
                B = 255.0F;
            }

            if (R < 0.0F)
            {
                R = 0.0F;
            }
            if (G < 0.0F)
            {
                G = 0.0F;
            }
            if (B < 0.0F)
            {
                B = 0.0F;
            }

            out_bit_RGBs[texture_width * h + w][0] = static_cast<uint8_t>(B);
            out_bit_RGBs[texture_width * h + w][1] = static_cast<uint8_t>(G);
            out_bit_RGBs[texture_width * h + w][2] = static_cast<uint8_t>(R);
            out_bit_RGBs[texture_width * h + w][3] = 255;
        }
    }
}

#if defined(__GNUC__)
#elif defined(_MSC_VER)
static LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        assert(WM_CREATE == uMsg);
        window_data *window_data_instance = reinterpret_cast<window_data *>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams);
        assert(NULL != window_data_instance);
        SetWindowLongPtrW(hWnd, 0, reinterpret_cast<LONG_PTR>(window_data_instance));
        return 0;
    }
    case WM_PAINT:
    {
        window_data *window_data_instance = reinterpret_cast<window_data *>(GetWindowLongPtrW(hWnd, 0));

        // FPS
        double fps = -1.0;
        {
            LARGE_INTEGER int64_performance_count;
            BOOL result_query_performance_counter = QueryPerformanceCounter(&int64_performance_count);
            assert(NULL != result_query_performance_counter);

            double new_performance_count = static_cast<double>(int64_performance_count.QuadPart);

            fps = window_data_instance->performance_frequency / (new_performance_count - window_data_instance->performance_count);

            window_data_instance->performance_count = new_performance_count;
        }

        // Inference
        tflite_predict(window_data_instance->bit_RGBs, window_data_instance->texture_width, window_data_instance->texture_height, window_data_instance->tflite_interpreter, window_data_instance->tflite_input, window_data_instance->tflite_output);

        {
            // write "texture" into "back buffer"
            BITMAPINFO bmi;
            ZeroMemory(&bmi, sizeof(bmi));
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = window_data_instance->texture_width;
            bmi.bmiHeader.biHeight = -window_data_instance->texture_height;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            int result_set_dib_bits = SetDIBits(NULL, window_data_instance->bitmap, 0, window_data_instance->texture_height, window_data_instance->bit_RGBs, &bmi, DIB_RGB_COLORS);
            assert(result_set_dib_bits > 0);
        }

        {
            HBITMAP old_bitmap = reinterpret_cast<HBITMAP>(SelectObject(window_data_instance->memory_device_context, window_data_instance->bitmap));
            assert(NULL != old_bitmap && HGDI_ERROR != old_bitmap);

            // write "text" into "back buffer"
            WCHAR fps_string[64];
            int fps_string_length = wsprintfW(fps_string, L"FPS: %d", static_cast<int>(fps));

            int result_text_out = TextOutW(window_data_instance->memory_device_context, 7, 7, fps_string, fps_string_length);
            assert(0 != result_text_out);

            // copy from "back-buffer" into "front buffer"
            int result_bit_blt = BitBlt(window_data_instance->device_context, 0, 0, window_data_instance->texture_width, window_data_instance->texture_height, window_data_instance->memory_device_context, 0, 0, SRCCOPY);
            assert(0 != result_bit_blt);

            HGDIOBJ new_bitmap = reinterpret_cast<HBITMAP>(SelectObject(window_data_instance->memory_device_context, old_bitmap));
            assert(new_bitmap == window_data_instance->bitmap);
        }

        return 0;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}
#else
#error Unknown Compiler
#endif