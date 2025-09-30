#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <iostream>
#include <xcb/xproto.h>
#include <X11/keysym.h>
#include <vector>
#include <unistd.h>
#include <tesseract/baseapi.h>
#include <array>
#include <memory>
#include <sstream>

tesseract::TessBaseAPI Tess_api;

void points_to_box(xcb_point_t *begin, xcb_point_t *end, xcb_rectangle_t *box){
	begin->x <= end->x ? (box->x = begin->x, box->width = end->x - begin->x) : (box->x = end->x, box->width = begin->x - end->x);
	begin->y <= end->y ? (box->y = begin->y, box->height = end->y - begin->y) : (box->y = end->y, box->height = begin->y - end->y);
}



// 讀取指令輸出
static std::string run_command(const std::string& cmd) {
    std::array<char, 4096> buf{};
    std::string out;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return out;
    while (size_t n = fread(buf.data(), 1, buf.size(), pipe.get())) out.append(buf.data(), n);
    return out;
}



static inline void rstrip_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

std::string normalize_text(const std::string& text) {
    std::istringstream iss(text);
    std::string line, result, prev;
    bool prev_empty = false;   // 是否剛輸出過段落

    while (std::getline(iss, line)) {
        rstrip_cr(line); // 處理 CRLF 的 \r

        if (line.empty()) {
            // 空行：視為段落。先把上一行（若有）寫入，再加一次 \n\n（避免連續堆疊）
            if (!prev.empty()) {
                result += prev;
                prev.clear();
            }
            if (!prev_empty) {
                result += "\n\n";
                prev_empty = true;
            }
            continue;
        }

        // 有內容的行
        if (!prev.empty()) {
            // 斷字：上一行以 '-' 結尾且這一行以 [A-Za-z] 開頭 → 去掉 '-' 直接接
            if (prev.back() == '-' && !line.empty() && std::isalpha((unsigned char)line[0])) {
                prev.pop_back();
                prev += line;
                line.clear();
            } else {
                // 否則把上一行寫出，並以空白銜接
                result += prev;
                result.push_back(' ');
            }
        }
        prev = line;
        prev_empty = false; // 目前不是段落狀態
    }

    if (!prev.empty()) {
        result += prev;
    }

    return result;
}



// Google 翻譯（透過 translate-shell），預設 auto→zh-TW
std::string gtrans(const std::string& text,
                   const std::string& to = "zh-TW",
                   const std::string& from = "auto") {
    // -b: 只輸出翻譯；關掉多餘訊息避免干擾解析
	std::string cmd = "trans -b :" + to + " \"" + text + "\"";

    return run_command(cmd);
}




/*
static uint8_t otsu_thresh(const uint32_t hist[256], uint64_t total, double* out_var=nullptr){
    uint64_t sum=0; for(int i=0;i<256;++i) sum += (uint64_t)i*hist[i];
    uint64_t wB=0, sumB=0; double maxVar=0; int thr=127;
    for(int t=0;t<256;++t){
        wB += hist[t]; if(!wB) continue;
        uint64_t wF = total - wB; if(!wF) break;
        sumB += (uint64_t)t*hist[t];
        double mB = (double)sumB / wB;
        double mF = (double)(sum - sumB) / wF;
        double var = (double)wB*(double)wF*(mB-mF)*(mB-mF);
        if (var > maxVar){ maxVar = var; thr = t; }
    }
    if(out_var) *out_var = maxVar;
    return (uint8_t)thr;
}
*/


std::string ocr_from_xcb_data(uint8_t *data, int w, int h, int stride, int bpp, int dpi) {
	// 把 BGRA → RGB
	
	std::vector<unsigned char> rgb(3 * w * h);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const unsigned char *p = data + y * stride + x * bpp;
			rgb[(y*w + x)*3 + 0] = p[2]; // R
			rgb[(y*w + x)*3 + 1] = p[1]; // G
			rgb[(y*w + x)*3 + 2] = p[0]; // B
		}
	}
	
	Tess_api.SetImage(rgb.data(), w, h, 3, w*3);


	/*
	// 1) 為 B/G/R 建直方圖，分別算 Otsu，選分離度最佳的通道
    uint32_t histB[256]={0}, histG[256]={0}, histR[256]={0};
    for (int y=0; y<h; ++y){
        const uint8_t* row = data + y*stride;
        for (int x=0; x<w; ++x){
            const uint8_t* p = row + x*bpp; // p[0]=B, p[1]=G, p[2]=R
            histB[p[0]]++; histG[p[1]]++; histR[p[2]]++;
        }
    }
    uint64_t total = (uint64_t)w*h;
    double vb=0, vg=0, vr=0;
    uint8_t tb = otsu_thresh(histB, total, &vb);
    uint8_t tg = otsu_thresh(histG, total, &vg);
    uint8_t tr = otsu_thresh(histR, total, &vr);
    int ch = 0; uint8_t thr = tb; double vmax=vb;
    if (vg>vmax){ ch=1; thr=tg; vmax=vg; }
    if (vr>vmax){ ch=2; thr=tr; vmax=vr; }

    // 2) 生成二值圖（白=255/黑=0），並統計黑白像素
    std::vector<uint8_t> bw(w*h);
    uint64_t black=0, white=0;
    for (int y=0; y<h; ++y){
        const uint8_t* row = data + y*stride;
        for (int x=0; x<w; ++x){
            const uint8_t* p = row + x*bpp;
            uint8_t v = p[ch];
            uint8_t pix = (v > thr) ? 255 : 0;
            bw[y*w + x] = pix;
            (pix==0 ? ++black : ++white);
        }
    }

    // 3) 反相成「黑字白底」（Tesseract 比較吃這個）
    if (black >= white){
        for (auto& px : bw) px = 255 - px;
    }

    // 4) （可選）2× 放大，對小字很有感
    const int W = w*2, H = h*2;
    std::vector<uint8_t> up(W*H);
    for (int y=0; y<H; ++y){
        int sy = y>>1;
        for (int x=0; x<W; ++x){
            int sx = x>>1;
            up[y*W + x] = bw[sy*w + sx];
        }
    }

	Tess_api.SetImage(up.data(), W, H, 1, W);
	Tess_api.SetSourceResolution(dpi * 2);
	*/


	Tess_api.SetSourceResolution(dpi);

	// 設定為多行文字區塊
	Tess_api.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);

	char *out = Tess_api.GetUTF8Text();
	std::string result(out ? out : "");
	delete [] out;

	return result;
}



void get_flame(xcb_connection_t *connection, xcb_screen_t *screen, xcb_rectangle_t *box, int dpi){

	if (box->width == 0 || box->height == 0) return;

	// 從 root window 抓圖
	xcb_get_image_cookie_t cookie = xcb_get_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, screen->root, box->x, box->y, box->width, box->height, ~0);
	xcb_get_image_reply_t *reply = xcb_get_image_reply(connection, cookie, NULL);
	if (reply) {

		int w = box->width;
		int h = box->height;
		int len = xcb_get_image_data_length(reply);
		uint8_t *data = xcb_get_image_data(reply);




		int stride = len / h;
		int bpp = stride / w;

		std::string text = normalize_text(ocr_from_xcb_data(data, w, h, stride, bpp, dpi));
		std::string result = gtrans(text, "zh-TW", "auto");
		std::cout << "--------------------" << std::endl;
		std::cout << text << std::endl;
		std::cout << std::endl;
		std::cout << result << std::endl;
	}
	else {
		fprintf(stderr, "Failed to get image\n");
	}
	free(reply);
}




int main () {

	if (Tess_api.Init(nullptr, "eng+chi_tra")) {return 0;}

	int screen_number;
	xcb_connection_t *connection;
	connection = xcb_connect(nullptr, &screen_number);
	if (xcb_connection_has_error(connection)) {
		throw std::runtime_error("xcb 連接失敗");
	}
	//std::cout << "螢幕編號:" << screen_number;



	const xcb_setup_t *setup;
	setup = xcb_get_setup(connection);



	int screen_width;
	int screen_height;
	xcb_screen_iterator_t screen_it = xcb_setup_roots_iterator(setup);
	while (screen_it.rem) {
		xcb_screen_t *target_screen = screen_it.data;
		screen_width = target_screen->width_in_pixels;
		screen_height = target_screen->height_in_pixels;
		//std::cout << "    width:" << target_screen->width_in_pixels << "    height:" << target_screen->height_in_pixels << std::endl;
		xcb_screen_next(&screen_it);
	}
	xcb_segment_t empty_line[2];
	empty_line[0].x1 = 0;
	empty_line[0].y1 = 0;
	empty_line[0].x2 = screen_width;
	empty_line[0].y2 = screen_height;
	empty_line[1].x1 = screen_width;
	empty_line[1].y1 = 0;
	empty_line[1].x2 = 0;
	empty_line[1].y2 = screen_height;



	xcb_screen_iterator_t iter;
	iter = xcb_setup_roots_iterator(setup);



	xcb_screen_t *screen;
	screen = iter.data;

	double dpi_x = screen->width_in_pixels  / (screen->width_in_millimeters  / 25.4);
	double dpi_y = screen->height_in_pixels / (screen->height_in_millimeters / 25.4);
	int dpi = static_cast<int>((dpi_x + dpi_y) / 2.0); // 平均值


	xcb_visualtype_t *ARGB_visual = nullptr;
	xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);
	while (depth_iter.rem) {
		if (depth_iter.data->depth == 32) {
			xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
			while (visual_iter.rem) {
				xcb_visualtype_t *visual = visual_iter.data;
				if (visual->_class == XCB_VISUAL_CLASS_TRUE_COLOR) {
					ARGB_visual = visual;
				}
				xcb_visualtype_next(&visual_iter);
			}
		}
		xcb_depth_next(&depth_iter);
	}



	xcb_colormap_t colormap = xcb_generate_id(connection);
	xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, ARGB_visual->visual_id);



	uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
	uint32_t value_list[5];
	value_list[0] = 0;									// 设置 Back Pixel
	value_list[1] = 0;									// 设置 Border Pixel 为 0
	value_list[2] = 1;									// 設成1，覆蓋在整個螢幕上，不經過WM
	value_list[3] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_PRESS;			
	value_list[4] = colormap;							// 設置 colormap



	xcb_window_t window;
	window = xcb_generate_id(connection);
	xcb_create_window(
		connection,
		32,
		window,
		screen->root,
		0, 0, screen_width, screen_height,
		1,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		ARGB_visual->visual_id,
		value_mask,
		value_list
	);
	xcb_map_window(connection, window);
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT, window, XCB_CURRENT_TIME);
	xcb_flush(connection);


	// 把鍵盤事件鎖定給自己，因為滑鼠點擊最左上角時會出現(1,1)座標，這時焦點會跳到未知的地方，讓esc鍵失效
	{
		xcb_grab_keyboard_cookie_t gck = xcb_grab_keyboard(connection, 0, window, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
		xcb_grab_keyboard_reply_t* grep = xcb_grab_keyboard_reply(connection, gck, nullptr);
		if (!grep || grep->status != XCB_GRAB_STATUS_SUCCESS) {
			std::cerr << "警告：grab_keyboard 失敗（可能被其他程式抓住）\n";
		}
		free(grep);
	}



	xcb_generic_event_t *event;
	xcb_keysym_t keysym;
	xcb_key_symbols_t *key_symbols;
	key_symbols = xcb_key_symbols_alloc(connection);
	struct timespec draw_sleep;
	draw_sleep.tv_sec = 0;
	draw_sleep.tv_nsec = 1000000000.0 / 60;
	struct timespec sleep_time;
	int stage = 0;



	// 建立 GC 之前先申請紅色跟綠色
	xcb_alloc_color_cookie_t red_color_cookie = xcb_alloc_color(connection, colormap, 65535, 0, 0); // R=FFFF, G=000, B=000
	xcb_alloc_color_reply_t *red_color_reply = xcb_alloc_color_reply(connection, red_color_cookie, NULL);
	xcb_alloc_color_cookie_t green_color_cookie = xcb_alloc_color(connection, colormap, 0, 65535, 0); // R=000, G=FFFF, B=000
	xcb_alloc_color_reply_t *green_color_reply = xcb_alloc_color_reply(connection, green_color_cookie, NULL);
	xcb_alloc_color_cookie_t blue_color_cookie = xcb_alloc_color(connection, colormap, 0, 0, 65535); // R=000, G=FFFF, B=000
	xcb_alloc_color_reply_t *blue_color_reply = xcb_alloc_color_reply(connection, blue_color_cookie, NULL);

	xcb_gcontext_t temp_box_gc = xcb_generate_id(connection);
	uint32_t temp_gc_values[2] = {screen->white_pixel, 0};
	if (red_color_reply) {temp_gc_values[0] = red_color_reply->pixel;}
	xcb_create_gc(connection, temp_box_gc, window, XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES, temp_gc_values);
	free(red_color_reply);
	uint32_t red_line_width = 2;
	xcb_change_gc(connection, temp_box_gc, XCB_GC_LINE_WIDTH, &red_line_width);

	xcb_gcontext_t last_box_gc = xcb_generate_id(connection);
	uint32_t last_gc_values[2] = {screen->white_pixel, 0};
	if (green_color_reply) {last_gc_values[0] = green_color_reply->pixel;}
	xcb_create_gc(connection, last_box_gc, window, XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES, last_gc_values);
	free(green_color_reply);
	uint32_t green_line_width = 2;
	xcb_change_gc(connection, last_box_gc, XCB_GC_LINE_WIDTH, &green_line_width);

	xcb_gcontext_t box_gc = xcb_generate_id(connection);
	uint32_t box_gc_values[2] = {screen->white_pixel, 0};
	if (blue_color_reply) {box_gc_values[0] = blue_color_reply->pixel;}
	xcb_create_gc(connection, box_gc, window, XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES, box_gc_values);
	free(blue_color_reply);


	xcb_point_t begin = {0, 0};
	xcb_point_t end = {0, 0};
	xcb_rectangle_t temp_box = {0, 0, 0, 0};
	std::vector<xcb_rectangle_t> boxes;

	while (1) {

		event = xcb_poll_for_event(connection); 
		if (event != nullptr){
			switch (event->response_type & ~0x80) {
				case XCB_KEY_PRESS:
					{
						xcb_key_press_event_t *key_press_event = (xcb_key_press_event_t*)event;
						keysym = xcb_key_symbols_get_keysym(key_symbols, key_press_event->detail, 0);
						if (keysym == XK_Escape) {
							switch (stage) {
								case 0:

									xcb_destroy_window(connection, window);
									xcb_flush(connection);
									free(event);
									goto quit;
								case 1:
									xcb_clear_area(connection, 1, window, 0, 0, 0, 0);
									stage = 0;
							}
						}
					}
					break;
				case XCB_BUTTON_PRESS:
					{
						xcb_button_press_event_t *butten_press_event = (xcb_button_press_event_t*)event;

						if (butten_press_event->detail == 1) {
							switch (stage) {
								case 0:
									begin.x = butten_press_event->event_x;
									begin.y = butten_press_event->event_y;
									stage = 1;
									break;
								case 1:
									xcb_clear_area(connection, 1, window, 0, 0, 0, 0);
									xcb_flush(connection);
									usleep(100000);	// 避免截到還沒更新的舊圖
									get_flame(connection, screen, &temp_box, dpi);
									boxes.push_back(temp_box);
									stage = 0;
									break;
							}
						}
					}
					break;
			}
		}


		// 清除整個視窗
		xcb_clear_area(connection, 1, window, 0, 0, 0, 0);
		if (boxes.size() > 0) {
			xcb_poly_rectangle(connection, window, box_gc, boxes.size() - 1, boxes.data());
			xcb_poly_rectangle(connection, window, last_box_gc, 1, &boxes.back());
		}
		else {
			xcb_poly_segment(connection, window, temp_box_gc, 2, empty_line);
		}



		if (stage == 1) {
			xcb_query_pointer_cookie_t cookie = xcb_query_pointer(connection, window);
			xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(connection, cookie, nullptr);
			if(reply){
				// 滑鼠當前位置
				end.x = reply->win_x;
				end.y = reply->win_y;
				points_to_box(&begin, &end, &temp_box);
				xcb_poly_rectangle(connection, window, temp_box_gc, 1, &temp_box);
			}
		}

		xcb_flush(connection);
		sleep_time = draw_sleep;
		while(nanosleep(&sleep_time, &sleep_time) == -1 && errno == EINTR);
	}



quit:
	Tess_api.End();
	xcb_free_colormap(connection, colormap);
	xcb_free_gc(connection, temp_box_gc);
	xcb_free_gc(connection, last_box_gc);
	xcb_free_gc(connection, box_gc);
	xcb_ungrab_keyboard(connection, XCB_CURRENT_TIME);
	xcb_key_symbols_free(key_symbols);
	xcb_disconnect(connection);
	return 0;
}
