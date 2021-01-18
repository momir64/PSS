#include <ctime>
#include <mutex>
#include <vector>
#include <thread>
#include <fstream>
#include <iostream>
#include <Windows.h>
#include <gdiplus.h>
#include <atlimage.h>
#include <curl/curl.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgcodecs/legacy/constants_c.h>
#pragma comment (lib,"Gdiplus.lib")

using namespace Gdiplus;
using namespace std;
using namespace cv;

struct WriteThis {
	const char *readptr;
	size_t sizeleft;
};

struct TimeOut {
	chrono::time_point<chrono::system_clock> start;
	bool firsttime = true;
};

clock_t last = clock();
#define current (long long)((clock()-last)/(CLOCKS_PER_SEC/1000)) 
#define THREADS 10
#define TIME_DELAY 100

string ip;
int IMG_COMPRESION = 50;
double IMG_SCALE = 0.5;
long long br = 0, maxbr = 500;
mutex mtxbr, mtxtime;

static size_t timeoutfunction(struct TimeOut *timeout, curl_off_t, curl_off_t, curl_off_t upload, curl_off_t) {
	if (upload > 500000)
		return 1;
	if (timeout->firsttime) {
		timeout->start = chrono::system_clock::now();
		timeout->firsttime = false;
	}
	else if (chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeout->start).count() > 666)
		return 1;

	return CURL_PROGRESSFUNC_CONTINUE;
}

static size_t writefunction(char *data, size_t, size_t nmemb, void *mydata) {
	(static_cast<string *>(mydata))->append(data, nmemb);
	return nmemb;
}

static size_t readfunction(void *ptr, size_t size, size_t nmemb, void *userp) {
	struct WriteThis *upload = (struct WriteThis *)userp;
	size_t max = size * nmemb;

	if (max < 1)
		return 0;

	if (upload->sizeleft) {
		size_t copylen = max;
		if (copylen > upload->sizeleft)
			copylen = upload->sizeleft;
		memcpy(ptr, upload->readptr, copylen);
		upload->readptr += copylen;
		upload->sizeleft -= copylen;
		return copylen;
	}

	return 0;
}

bool BMPtoJPG(HBITMAP hBitmap, vector<BYTE> &buf) {
	IStream *stream = NULL;
	HRESULT hr = CreateStreamOnHGlobal(0, TRUE, &stream);
	CImage image;
	ULARGE_INTEGER liSize;
	image.Attach(hBitmap);
	image.Save(stream, Gdiplus::ImageFormatJPEG);
	IStream_Size(stream, &liSize);
	DWORD len = liSize.LowPart;
	IStream_Reset(stream);
	buf.resize(len);
	IStream_Read(stream, &buf[0], len);
	stream->Release();
	return true;
}

void TakeScreenShot(vector <uchar> &ScreenShot) {
	CoInitialize(NULL);
	ULONG_PTR token;
	Gdiplus::GdiplusStartupInput tmp;
	Gdiplus::GdiplusStartup(&token, &tmp, NULL);
	RECT rc;
	GetClientRect(GetDesktopWindow(), &rc);
	auto hdc = GetDC(0);
	auto memdc = CreateCompatibleDC(hdc);
	auto hbitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
	auto oldbmp = SelectObject(memdc, hbitmap);
	BitBlt(memdc, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
	SelectObject(memdc, oldbmp);
	DeleteDC(memdc);
	ReleaseDC(0, hdc);
	vector<BYTE> imageRaw;
	BMPtoJPG(hbitmap, imageRaw);
	DeleteObject(hbitmap);
	Gdiplus::GdiplusShutdown(token);
	CoUninitialize();
	Mat imgResized, imgRaw(1, (int)imageRaw.size(), CV_8UC1, (void *)imageRaw.data());
	Mat imgDecoded = imdecode(imgRaw, CV_LOAD_IMAGE_COLOR);
	resize(imgDecoded, imgResized, cv::Size(), IMG_SCALE, IMG_SCALE);
	vector<int> compression;
	compression.push_back(CV_IMWRITE_JPEG_QUALITY);
	compression.push_back(IMG_COMPRESION);
	cv::imencode(".jpg", imgResized, ScreenShot, compression);
}

void Upload(char *username_password) {
	CURL *curl;
	struct WriteThis upload;
	struct TimeOut timeout;
	vector <uchar> ScreenShot;

	curl = curl_easy_init();
	if (!curl) {
		cout << "Curl easy init error!" << endl;
		_exit(-3);
	}

	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, timeoutfunction);
	curl_easy_setopt(curl, CURLOPT_USERPWD, username_password);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfunction);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &timeout);
	curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	if (curl)
		while (true) {
			ScreenShot.clear();
			while (true) {
				while (current < TIME_DELAY) Sleep(5);
				mtxtime.lock();
				if (current < TIME_DELAY)
					mtxtime.unlock();
				else {
					last = clock();
					mtxtime.unlock();
					break;
				}
			}

			TakeScreenShot(ScreenShot);
			Sleep(10);

			if (ScreenShot.size() < 500000) {
				mtxbr.lock();
				curl_easy_setopt(curl, CURLOPT_URL, (ip + to_string(br) + ".jpg").c_str());
				br = br + 1 > maxbr ? 0 : br + 1;
				mtxbr.unlock();
				timeout.firsttime = true;
				upload.sizeleft = ScreenShot.size();
				upload.readptr = (char *)ScreenShot.data();
				curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)upload.sizeleft);
				curl_easy_perform(curl);
			}
		}

	curl_easy_cleanup(curl);
}


int main(int argc, char **argv) {
	if (argc < 3 || argc > 6) {
		cout << "Wrong number of arguments!" << endl;
		return -1;
	}

	ip = "ftp://" + string(argv[1]) + "/images/";

	if (argc > 3)
		IMG_SCALE = (double)((double)stoi(argv[3]) / (double)100.0);
	if (argc > 4)
		IMG_COMPRESION = stoi(argv[4]);
	if (argc > 5)
		maxbr = stoll(argv[5]);

	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
		cout << "Curl global init error!" << endl;
		return -2;
	}

	for (int i = 0; i < THREADS - 1; i++) {
		thread th(Upload, argv[2]);
		th.detach();
	}

	thread th(Upload, argv[2]);
	th.join();

	curl_global_cleanup();
	return 1;
}