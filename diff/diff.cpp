#include <iostream>
#include <QtGui>

using namespace std;

inline int color(QRgb color, int index) {
	return color >> (index * 8) & 0xff;
}

int main(int argc, char *argv[]) {
	if(argc != 3) {
		cerr << "Give me two images to compare" << endl;
		return 1;
	}

	QImage a(argv[1]);	
	QImage b(argv[2]);

	int max_diff = 0;

	if(a.width() != b.width() || a.height() != b.height()) {
		cerr << "Different dimensions!" << endl;
	}

	for(int x = 0; x < a.width(); ++x) {
		for(int y = 0; y < a.height(); ++y) {
			int pa = a.pixel(x, y);
			int pb = b.pixel(x, y);

			QRgb diff = 0xff << 24;

			for(int n = 0; n < 3; ++n) {
				int channel_diff = abs(color(pa, n) - color(pb, n));
				diff |= channel_diff << (n * 8);
				max_diff = max(max_diff, channel_diff);
			}

			a.setPixel(x, y, diff);
		}
	}

	cout << "Max diff: " << max_diff << endl;

	a.save("diff.png");
}

