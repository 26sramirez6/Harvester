#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include <cmath>
#include <cassert>
#include <iostream>
#include <string>  
#include <climits>
#include "WinUser.h"
#include "wingdi.h"
#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0
//constexpr int one = 1;
//constexpr bool endianness = *(char *)&one == 1;
//struct endianness {
//	bool value = *(char *)&one == 1;
//};

int bytesToInt(const unsigned char* bytes, bool little_endian) {
	int ret = 0;
	if (little_endian) {
		for (int n = sizeof(ret); n >= 0; n--) {
			ret = (ret << 8) + bytes[n];
		}
	} else {
		for (unsigned n = 0; n < sizeof(ret); n++) {
			ret = (ret << 8) + bytes[n];
		}
	}
	return ret;
}

struct Image {
	unsigned char * byte_buf;
	int bytes_per_pixel;
	RGBQUAD * rgb_buf = NULL;
	int width, height;

	void fillRGBBuffer() {
		if (rgb_buf) delete rgb_buf;
		assert(byte_buf);
		rgb_buf = new RGBQUAD[width * height];
		for (int i = 0; i < width * height; i++) {
			rgb_buf[i].rgbBlue = byte_buf[i*3];
			rgb_buf[i].rgbGreen = byte_buf[i*3 + 1];
			rgb_buf[i].rgbRed = byte_buf[i*3 + 2];
		}
	}
};

struct Point {
	int x, y;
	Point() {};
	Point(int x, int y) : x(x), y(y) {};
	void slide(int dx, int dy) { x += dx; y += dy; }

	std::string toString() {
		std::string ret;
		ret.push_back('(');
		ret.append(std::to_string(x));
		ret.push_back(',');
		ret.append(std::to_string(y));
		ret.push_back(')');
		return ret;
	}
};

struct Rect {
	Point m_bot_left, m_bot_right, m_top_left, m_top_right;
	int m_width, m_height;
	Rect() {};
	Rect(Point bot_left, Point bot_right, Point top_left, Point top_right) :
		m_bot_left(bot_left), m_bot_right(bot_right), m_top_left(top_left), m_top_right(top_right) {
		m_width = bot_right.x - bot_left.x;
		m_height = top_right.y - bot_right.y;
	}

	Rect(Point bot_left, int width, int height) :
		m_bot_left(bot_left), m_bot_right(bot_left), m_top_left(bot_left), m_top_right(bot_left) {
		m_bot_right.slide(width, 0);
		m_top_left.slide(0, height);
		m_top_right.slide(width, height);
		m_width = m_bot_right.x - m_bot_left.x;
		m_height = m_top_right.y - m_bot_right.y;
	}

	void slide(int dx, int dy) {
		m_bot_left.slide(dx, dy);
		m_bot_right.slide(dx, dy);
		m_top_left.slide(dx, dy);
		m_top_right.slide(dx, dy);
	}

	bool contains(int c, int r) const {
		return m_bot_left.x <= c && c <= m_bot_right.x &&
			m_bot_left.y <= r && r <= m_top_left.y;
	}

	std::string toString() {
		std::string ret;
		ret.append(m_bot_left.toString());
		ret.push_back(',');
		ret.append(m_bot_right.toString());
		ret.push_back(',');
		ret.append(m_top_left.toString());
		ret.push_back(',');
		ret.append(m_top_right.toString());
		return ret;
	}
};

static inline int clamp(int val, int lower, int upper) {
	return val > upper ? upper :
		val < lower ? lower : val;
}
constexpr int bin_size = 4;
static int bins[] = { 64, 128, 192, 256 };
struct Histogram {
	int m_green[bin_size] = { 0, 0, 0, 0 };
	int m_red[bin_size] = { 0, 0, 0, 0 };
	int m_blue[bin_size] = { 0, 0, 0, 0 };
	
	std::string toString() {
		std::string out;
		for (int i = 0; i < bin_size; i++) {
			out.append(std::to_string(m_red[i]));
			out.push_back(',');
			out.append(std::to_string(m_green[i]));
			out.push_back(',');
			out.append(std::to_string(m_blue[i]));
			out.push_back(',');
		}
		return out;
	}

	int absoluteSumCompare(Histogram& other) {
		int greenSum = 0;
		for (int i = 0; i < bin_size; i++) {
			greenSum += std::abs(this->m_green[i] - other.m_green[i]);
		};
		int blueSum = 0;
		for (int i = 0; i < bin_size; i++) {
			blueSum += std::abs(this->m_blue[i] - other.m_blue[i]);
		};
		int redSum = 0;
		for (int i = 0; i < bin_size; i++) {
			redSum += std::abs(this->m_red[i] - other.m_red[i]);
		};

		return redSum + greenSum + blueSum;
	}
};

static void computeHistogram(Histogram& hist, Image * source, Rect rect) {
	for (int r = rect.m_bot_left.y; r < rect.m_height + rect.m_bot_left.y; r++) {
		for (int c = rect.m_bot_left.x; c < rect.m_width + rect.m_bot_left.x; c++) {
			int index = c + r * source->width;
			int red = source->rgb_buf[index].rgbRed;
			int green = source->rgb_buf[index].rgbGreen;
			int blue = source->rgb_buf[index].rgbBlue;

			if (red < bins[0]) {
				hist.m_red[0]++;
			} else if (red < bins[1]) {
				hist.m_red[1]++;
			} else if (red < bins[2]) {
				hist.m_red[2]++;
			} else {
				hist.m_red[3]++;
			}

			if (green < bins[0]) {
				hist.m_green[0]++;
			} else if (green < bins[1]) {
				hist.m_green[1]++;
			} else if (green < bins[2]) {
				hist.m_green[2]++;
			} else {
				hist.m_green[3]++;
			}

			if (blue < bins[0]) {
				hist.m_blue[0]++;
			} else if (blue < bins[1]) {
				hist.m_blue[1]++;
			} else if (blue < bins[2]) {
				hist.m_blue[2]++;
			} else {
				hist.m_blue[3]++;
			}
		}
	}
}

void fromBMP1(const char *fileName, Image * img) {
	FILE * imageFile;
	fopen_s(&imageFile, fileName, "rb");
	int dataOffset;
	fseek(imageFile, DATA_OFFSET_OFFSET, SEEK_SET);
	fread(&dataOffset, 4, 1, imageFile);
	//Read width
	fseek(imageFile, WIDTH_OFFSET, SEEK_SET);
	fread(&img->width, 4, 1, imageFile);
	//Read height
	fseek(imageFile, HEIGHT_OFFSET, SEEK_SET);
	fread(&img->height, 4, 1, imageFile);
	//Read bits per pixel
	unsigned short bitsPerPixel;
	fseek(imageFile, BITS_PER_PIXEL_OFFSET, SEEK_SET);
	fread(&bitsPerPixel, 2, 1, imageFile);
	//Allocate a pixel array
	img->bytes_per_pixel = bitsPerPixel / 8;
	int paddedRowSize = (int)(4 * ceil((float)(img->width) / 4.0f))*(img->bytes_per_pixel);
	int unpaddedRowSize = (img->width)*(img->bytes_per_pixel);
	//Total size of the pixel data in bytes
	int totalSize = unpaddedRowSize * (img->height);
	img->byte_buf = new unsigned char[totalSize];
	//point to the last row of our pixel array (unpadded)
	byte * currentRowPointer = img->byte_buf + ((img->height - 1)*unpaddedRowSize);
	for (int i = 0; i < img->height; i++) {
		//put file cursor in the next row from top to bottom
		fseek(imageFile, dataOffset + (i*paddedRowSize), SEEK_SET);
		//read only unpaddedRowSize bytes (we can ignore the padding bytes)
		fread(currentRowPointer, 1, unpaddedRowSize, imageFile);
		//point to the next row (from bottom to top)
		currentRowPointer -= unpaddedRowSize;
	}
	img->fillRGBBuffer();
	fclose(imageFile);
}

static void fromBMP2(const char * filename, Image * img) {
	FILE * f;
	fopen_s(&f, filename, "rb");
	if (f == NULL) throw "Argument Exception";
	unsigned char info[54];
	fread(info, sizeof(unsigned char), 54, f); // read the 54-byte header
	// extract image height and width from header
	int width = *(int*)&info[18];
	int height = *(int*)&info[22];

	int row_padded = (width * 3 + 3) & (~3);
	img->byte_buf = new unsigned char[width * height * 3];
	unsigned char * current_row = img->byte_buf;
	img->width = width;
	img->height = height;
	for (int i = 0; i < height; i++) {
		fread(current_row, sizeof(unsigned char), row_padded, f);
		current_row += row_padded;
		//for (int j = 0; j < width * 3; j += 3) {
		//	// Convert (B, G, R) to (R, G, B)
		//	unsigned char tmp = img->byte_buf[j];
		//	img->byte_buf[j] = img->byte_buf[j + 2];
		//	img->byte_buf[j + 2] = tmp;
		//}
	}
	img->fillRGBBuffer();
	fclose(f);
}

void toBMP1(const char *file_name, Image * img) {
	//Open file in binary mode
	FILE * outputFile;
	fopen_s(&outputFile, file_name, "wb");
	//*****HEADER************//
	//write signature
	const char *BM = "BM";
	fwrite(&BM[0], 1, 1, outputFile);
	fwrite(&BM[1], 1, 1, outputFile);
	//Write file size considering padded bytes
	int paddedRowSize = (int)(4 * ceil((float)img->width / 4.0f))*img->bytes_per_pixel;
	int fileSize = paddedRowSize * img->height + HEADER_SIZE + INFO_HEADER_SIZE;
	fwrite(&fileSize, 4, 1, outputFile);
	//Write reserved
	int reserved = 0x0000;
	fwrite(&reserved, 4, 1, outputFile);
	//Write data offset
	int dataOffset = HEADER_SIZE + INFO_HEADER_SIZE;
	fwrite(&dataOffset, 4, 1, outputFile);

	//*******INFO*HEADER******//
	//Write size
	int infoHeaderSize = INFO_HEADER_SIZE;
	fwrite(&infoHeaderSize, 4, 1, outputFile);
	//Write width and height
	fwrite(&img->width, 4, 1, outputFile);
	fwrite(&img->height, 4, 1, outputFile);
	//Write planes
	unsigned short planes = 1; //always 1
	fwrite(&planes, 2, 1, outputFile);
	//write bits per pixel
	unsigned short bitsPerPixel = img->bytes_per_pixel * 8;
	fwrite(&bitsPerPixel, 2, 1, outputFile);
	//write compression
	int compression = NO_COMPRESION;
	fwrite(&compression, 4, 1, outputFile);
	//write image size (in bytes)
	int imageSize = img->width * img->height*img->bytes_per_pixel;
	fwrite(&imageSize, 4, 1, outputFile);
	//write resolution (in pixels per meter)
	int resolutionX = 11811; //300 dpi
	int resolutionY = 11811; //300 dpi
	fwrite(&resolutionX, 4, 1, outputFile);
	fwrite(&resolutionY, 4, 1, outputFile);
	//write colors used 
	int colorsUsed = MAX_NUMBER_OF_COLORS;
	fwrite(&colorsUsed, 4, 1, outputFile);
	//Write important colors
	int importantColors = ALL_COLORS_REQUIRED;
	fwrite(&importantColors, 4, 1, outputFile);
	//write data
	int i = 0;
	int unpaddedRowSize = img->width * img->bytes_per_pixel;
	for (i = 0; i < img->height; i++) {
		//start writing from the beginning of last row in the pixel array
		int pixelOffset = ((img->height - i) - 1)*unpaddedRowSize;
		fwrite(&img->byte_buf[pixelOffset], 1, paddedRowSize, outputFile);
	}
	fclose(outputFile);
}

static void toBMP(const char * name, const Image * source, const Rect target) {
	unsigned char * img = NULL;
	int filesize = 54 + 3 * source->width*source->height;

	img = (unsigned char *)malloc(3 * source->width*source->height * sizeof(unsigned char));
	memset(img, 0, 3 * source->width*source->height);
	for (int r = 0; r < source->height; r++) {
		for (int c = 0; c < source->width; c++) {
			int red, green, blue;
			if (target.contains(c, r)) {
				red = 255; green = 0; blue = 0;
			} else {
				red = clamp(source->rgb_buf[r*source->width + c].rgbRed, 0, 255);
				green = clamp(source->rgb_buf[r*source->width + c].rgbGreen, 0, 255);
				blue = clamp(source->rgb_buf[r*source->width + c].rgbBlue, 0, 255);
			}
			int y = (source->height - 1) - r;
			img[(c + y * source->width) * 3 + 2] = (unsigned char)(red);
			img[(c + y * source->width) * 3 + 1] = (unsigned char)(green);
			img[(c + y * source->width) * 3 + 0] = (unsigned char)(blue);
		}
	}

	unsigned char bmpfileheader[14] = { 'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0 };
	unsigned char bmpinfoheader[40] = { 40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0 };
	unsigned char bmppad[3] = { 0,0,0 };

	bmpfileheader[2] = (unsigned char)(filesize);
	bmpfileheader[3] = (unsigned char)(filesize >> 8);
	bmpfileheader[4] = (unsigned char)(filesize >> 16);
	bmpfileheader[5] = (unsigned char)(filesize >> 24);

	bmpinfoheader[4] = (unsigned char)(source->width);
	bmpinfoheader[5] = (unsigned char)(source->width >> 8);
	bmpinfoheader[6] = (unsigned char)(source->width >> 16);
	bmpinfoheader[7] = (unsigned char)(source->width >> 24);
	bmpinfoheader[8] = (unsigned char)(source->height);
	bmpinfoheader[9] = (unsigned char)(source->height >> 8);
	bmpinfoheader[10] = (unsigned char)(source->height >> 16);
	bmpinfoheader[11] = (unsigned char)(source->height >> 24);

	FILE * f;
	fopen_s(&f, name, "wb");
	fwrite(bmpfileheader, 1, 14, f);
	fwrite(bmpinfoheader, 1, 40, f);
	for (int i = 0; i < source->height; i++) {
		fwrite(img + (source->width*(source->height - i - 1) * 3), 3, source->width, f);
		fwrite(bmppad, 1, (4 - (source->width * 3) % 4) % 4, f);
	}

	free(img);
	fclose(f);
}

static Rect scanForImage(Image * source, Image * target, int hstride, int vstride) {
	Point bl(0, 0), br(target->width,0), 
		tl(0, target->height), tr(target->width, target->height);
	Rect current_rect(bl, br, tl, tr);;
	Histogram target_hist;
	computeHistogram(target_hist, target, current_rect);
	std::cout << target_hist.toString() << std::endl;
	std::cout << source->width << ", " << source->height << std::endl;
	Rect highest_match_rect;
	int lowest_diff_value = INT_MAX;
	for (int r = 0; r < source->height - current_rect.m_height; r += vstride) {
		for (int c = 0; c < source->width - current_rect.m_width; c += hstride) {
			Histogram current_hist;
			computeHistogram(current_hist, source, current_rect);
			int match_value = target_hist.absoluteSumCompare(current_hist);
			if (lowest_diff_value > match_value) {
				lowest_diff_value = match_value;
				highest_match_rect = current_rect;
				if (lowest_diff_value == 0) {
					return highest_match_rect;
				}
				std::cout << "new match value: " << lowest_diff_value << 
					", " << current_hist.toString() << std::endl;
			}
			
			current_rect.slide(hstride, 0);
		}
		current_rect.slide(-(source->width - current_rect.m_width), 0);
		current_rect.slide(0, vstride);
	}
	
	return highest_match_rect;
}

static void getScreenshotImage(Image * out) {
	int n_screen_width = 1920;//GetSystemMetrics(SM_CXSCREEN);
	int n_screen_height = 1080;//GetSystemMetrics(SM_CYSCREEN);
	HWND desktop_window = GetDesktopWindow();
	HDC desktop_dc = GetDC(desktop_window);
	HDC capture_dc = CreateCompatibleDC(desktop_dc);
	HBITMAP capture_bitmap = CreateCompatibleBitmap(desktop_dc, n_screen_width, n_screen_height);
	SelectObject(capture_dc, capture_bitmap);

	BitBlt(capture_dc, 0, 0, n_screen_width, n_screen_height, desktop_dc, 0, 0, SRCCOPY | CAPTUREBLT);

	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = n_screen_width;
	bmi.bmiHeader.biHeight = n_screen_height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	out->height = n_screen_height;
	out->width = n_screen_width;
	out->rgb_buf = new RGBQUAD[n_screen_width * n_screen_height];

	// Call GetDIBits to copy the bits from the device dependent bitmap
	// into the buffer allocated above, using the pixel format you
	// chose in the BITMAPINFO.
	::GetDIBits(capture_dc,
		capture_bitmap,
		0,  // starting scanline
		n_screen_height,  // scanlines to copy
		out->rgb_buf,  // buffer for your copy of the pixels
		&bmi,  // format you want the data in
		DIB_RGB_COLORS);  // actual pixels, not palette references
}

int main() {
	Image source, refresh;
	getScreenshotImage(&source);
	std::cout << "read screenshot" << std::endl;
	
	char pBuf[256]; 
	size_t len = sizeof(pBuf);
	int bytes = GetModuleFileName(NULL, pBuf, len);
	OutputDebugString(pBuf);
	//std::cout << pBuf << std::endl;

	fromBMP1("Refresh.bmp", &refresh);
	std::cout << "read refresh" << std::endl;
	//Rect test(Point(1560, 540), refresh.width, refresh.height);
	//Histogram h;
	//computeHistogram(h, &source, test);
	//std::cout << h.toString() << std::endl;

	//Histogram g;
	//computeHistogram(g, &refresh, Rect(Point(0,0), refresh.width, refresh.height));
	//std::cout << g.toString() << std::endl;
	//Rect highest_match = scanForImage(&source, &refresh, 1, 1);
	//std::cout << highest_match.toString() << std::endl;
	std::cout << refresh.width << ", " <<  refresh.height << std::endl;
	toBMP("img.bmp", &refresh, Rect(Point(0, 0), 1,1));
	//toBMP1("img.bmp", &refresh);
	std::cout << "finished" << std::endl;
	return 0;//
}