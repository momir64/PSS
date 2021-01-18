#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core.hpp>
#include <filesystem>
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
using namespace cv;
using namespace std;

#define DELAY 20
#define DRANGE 300
#define MAXIMG 500

string img(long long br) {
	return 	"images\\" + to_string(br) + ".jpg";
}

long long range(long long in) {
	if (in > MAXIMG)
		return in - MAXIMG - 1;
	else if (in < 0)
		return in + MAXIMG + 1;
	else
		return in;
}

bool ExistsInRange(long long br) {
	long long delay = 0;
	for (long long i = br; i <= br + DRANGE; i++)
		if (filesystem::exists(img(range(i))))
			if (delay == DELAY)
				return true;
			else
				delay++;
	return false;
}

int main(int argc, char **argv) {
	long long br = 0;
	Mat image;

	namedWindow("Monitor", WINDOW_NORMAL);

	while (!filesystem::exists(img(br))) br = range(br + 1);
	while (!ExistsInRange(br)) waitKey(1);
	while (1) {
		waitKey(5);
		fstream file(img(br));
		file.seekg(-2, ios_base::end);
		if ((char)file.get() == '\xff' && (char)file.get() == '\xd9') {
			SetConsoleTitleA(("Image: " + to_string(br)).c_str());
			image = imread(img(br), IMREAD_COLOR);
			if (image.data)
				imshow("Monitor", image);
		}
		file.close();
		DeleteFileA(img(range(br)).c_str());
		do
			br = range(br + 1);
		while (!filesystem::exists(img(br)));
		while (!ExistsInRange(br)) waitKey(1);
	}

	return 0;
}