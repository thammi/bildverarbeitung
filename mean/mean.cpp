#include <iostream>
#include <stdint.h>

#include <boost/foreach.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>

#include <QtGui>

using namespace std;
using namespace boost;

namespace po = program_options;

inline int normColor(const int color, const int index) {
	return color >> (index * 8) & 0xff;
}

class MeanChannel {
public:
	MeanChannel(const QImage& img) : width(img.width()), height(img.height()) {
		if(width == 0 || height == 0) {
			return;
		}

		for(int n = 0; n < 3; ++n) {
			data[n] = new int[width*height];
		}

		QRgb rootPixel = img.pixel(0, 0);
		for(int n = 0; n < 3; ++n) {
			data[n][0] = normColor(rootPixel, n);
		}

		for(int x = 1; x < width; ++x) {
			QRgb pixel = img.pixel(x, 0);

			for(int n = 0; n < 3; ++n) {
				data[n][x] = data[n][x-1] + normColor(pixel, n);
			}
		}

		for(int y = 1; y < height; ++y) {
			QRgb pixel = img.pixel(0, y);

			for(int n = 0; n < 3; ++n) {
				data[n][y*width] = data[n][(y-1)*width] + normColor(pixel, n);
			}
		}

		for(int x = 1; x < width; ++x) {
			for(int y = 1; y < height; ++y) {
				QRgb pixel = img.pixel(x, y);

				for(int n = 0; n < 3; ++n) {
					set(x, y, n, get(x - 1, y, n) + get(x, y - 1, n) - get(x - 1, y - 1, n) + normColor(pixel, n));
				}
			}
		}
	}

	QRgb mean(const int x, const int y, const int rad=1) {
		const int w = rad * 2 + 1;
		const int div = w * w;

		QRgb color = 0xff;

		for(int n = 2; n >= 0; --n) {
			color <<= 8;

			if(x <= rad) {
				if(y <= rad) {
					color |= get(x + rad, y + rad, n) / div;
				} else {
					color |= (get(x + rad, y + rad, n) - get(x + rad, y - rad - 1, n)) / div;
				}
			} else if(y <= rad) {
				color |= (get(x + rad, y + rad, n) - get(x - rad - 1, y + rad, n)) / div;
			} else {
				color |= (get(x + rad, y + rad, n) - get(x - rad - 1, y + rad, n) - get(x + rad, y - rad - 1, n) + get(x - rad - 1, y - rad - 1, n)) / div;
			}
		}

		return color;
	}

protected:
	inline int get(const int x, const int y, const int n) {
		return data[n][x + y * width];
	}

	inline void set(const int x, const int y, const int n, const int v) {
		data[n][x + y * width] = v;
	}

private:
	const int width, height;
	int* data[3];
};

static void fast_mean(QImage& img, const int rad=1) {
	cout << "Using the faster code path" << endl;

	MeanChannel mean(img);

	for(int x = rad; x < img.width() - rad; ++x) {
		for(int y = rad; y < img.height() - rad; ++y) {
			//cout << red.mean(x, y) << endl;
			img.setPixel(x, y, mean.mean(x, y));
			//img.setPixel(x, y, qRgb(0,0,0));
		}
	}
}

static void slow_mean(QImage& img, const int rad=1) {
	cout << "Using the slower code path" << endl;

	QImage tmp(img);

	const int w = rad * 2 + 1;
	const int div = w * w;

	for(int x = rad; x < img.width() - rad; ++x) {
		for(int y = rad; y < img.height() - rad; ++y) {
			int sum[3] = {0, 0, 0};

			for(int ax = x - rad; ax <= x + rad; ++ax) {
				for(int ay = y - rad; ay <= y + rad; ++ay) {
					QRgb pixel = tmp.pixel(ax, ay);

					for(int n = 0; n < 3; ++n) {
						sum[n] += pixel >> (n * 8) & 0xff;
					}
				}
			}

			const QRgb color = 0xff000000 | ((sum[2] / div) << 16) | ((sum[1] / div) << 8) | (sum[0] / div);
			img.setPixel(x, y, color);
		}
	}
}

typedef void (*mean_fun)(QImage& img, const int rad);

int main(int argc, char *argv[]) {
	mean_fun fun = fast_mean;
	int radius = 1;
	string prefix = "integral_";

	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("radius,r", po::value<int>(), "set filter mask radius")
		("slow,s", "use the slow code path")
		("fast,f", "use the fast code path (default)")
		("input", po::value< vector<string> >(), "input file")
		;

	po::positional_options_description p;
	p.add("input", -1);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
	po::notify(vm);    

	if(vm.count("help")) {
		cout << desc << "\n";
		return 0;
	}

	if(vm.count("slow")) {
		fun = slow_mean;
		prefix = "naive_";
	}

	if(vm.count("radius")) {
		radius = vm["radius"].as<int>();
	}

	if (vm.count("input"))
	{
		BOOST_FOREACH(const string& file, vm["input"].as< vector<string> >()) {
			cout << "Processing " << file << " with radius " << radius << endl;

			QImage img(file.c_str());

			fun(img, radius);

			string out_name(prefix + file);
			img.save(out_name.c_str());
			cout << "Output written to " << out_name << endl;
		}
	} else {
		cout << "No input files specified." << endl << "Hint: Try '-h' for help." << endl;
	}

	return 0;
}
