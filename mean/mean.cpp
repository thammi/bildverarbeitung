#include <iostream>
#include <vector>

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

		for(int y = 1; y < height; ++y) {
			for(int x = 1; x < width; ++x) {
				QRgb pixel = img.pixel(x, y);

				for(int n = 0; n < 3; ++n) {
					set(x, y, n, get(x - 1, y, n) + get(x, y - 1, n) - get(x - 1, y - 1, n) + normColor(pixel, n));
				}
			}
		}
	}

	QRgb mean(const int x, const int y, const int rad=1) {
		const int start_x = max(0, x - rad);
		const int end_x = min(width - 1, x + rad);
		const int start_y = max(0, y - rad);
		const int end_y = min(height - 1, y + rad);

		const int div = (end_x - start_x + 1) * (end_y - start_y + 1);

		QRgb color = 0xff;

		for(int n = 2; n >= 0; --n) {
			color <<= 8;

			if(x <= rad) {
				if(y <= rad) {
					color |= get(end_x, end_y, n) / div;
				} else {
					color |= (get(end_x, end_y, n) - get(end_x, start_y - 1, n)) / div;
				}
			} else if(y <= rad) {
				color |= (get(end_x, end_y, n) - get(start_x - 1, end_y, n)) / div;
			} else {
				color |= (get(end_x, end_y, n) - get(start_x - 1, end_y, n) - get(end_x, start_y - 1, n) + get(start_x - 1, start_y - 1, n)) / div;
			}
		}

		return color;
	}

	QRgb fastMean(const int x, const int y, const int rad=1) {
		const int w = rad * 2 + 1;
		const int div = w * w;

		QRgb color = 0xff;

		for(int n = 2; n >= 0; --n) {
			color <<= 8;
			color |= (get(x + rad, y + rad, n) - get(x - rad - 1, y + rad, n) - get(x + rad, y - rad - 1, n) + get(x - rad - 1, y - rad - 1, n)) / div;
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

static void real_fast_mean(QImage& img, const int rad) {
	cout << "Using the really fast code path" << endl;

	MeanChannel mean(img);

	const int box_x_start = rad + 1;
	const int box_x_end = img.width() - rad;
	const int box_y_start = rad + 1;
	const int box_y_end = img.height() - rad;

	for(int y = 0; y < img.height(); ++y) {
		for(int x = 0; x < img.width(); ++x) {
			if(x == box_x_start && y > box_y_start && y < box_y_end) {
				x = box_x_end;
			}

			img.setPixel(x, y, mean.mean(x, y, rad));
		}
	}

	for(int y = box_y_start; y < box_y_end; ++y) {
		for(int x = box_x_start; x < box_x_end; ++x) {
			img.setPixel(x, y, mean.fastMean(x, y, rad));
		}
	}
}

static void fast_mean(QImage& img, const int rad) {
	cout << "Using the faster code path" << endl;

	MeanChannel mean(img);

	for(int y = 0; y < img.height(); ++y) {
		for(int x = 0; x < img.width(); ++x) {
			img.setPixel(x, y, mean.mean(x, y, rad));
		}
	}
}

static void slow_mean(QImage& img, const int rad) {
	cout << "Using the slower code path" << endl;

	QImage tmp(img);

	for(int y = 0; y < img.height(); ++y) {
		for(int x = 0; x < img.width(); ++x) {
			int sum[3] = {0, 0, 0};
			int count = 0;

			for(int ay = max(0, y - rad); ay <= min(img.height() - 1, y + rad); ++ay) {
				for(int ax = max(0, x - rad); ax <= min(img.width() - 1, x + rad); ++ax) {
					QRgb pixel = tmp.pixel(ax, ay);

					for(int n = 0; n < 3; ++n) {
						sum[n] += pixel >> (n * 8) & 0xff;
					}

					++count;
				}
			}

			const QRgb color = 0xff000000 | ((sum[2] / count) << 16) | ((sum[1] / count) << 8) | (sum[0] / count);
			img.setPixel(x, y, color);
		}
	}
}

typedef void (*mean_fun)(QImage& img, const int rad);

struct fun_struct {
	mean_fun fun;
	string prefix;

	fun_struct() : fun(NULL) {}
	fun_struct(mean_fun fun, const char* prefix) : fun(fun), prefix(prefix) {}
};

int main(int argc, char *argv[]) {
	vector<fun_struct> methods;
	int radius = 1;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("radius,r", po::value<int>(), "set filter mask radius")
		("slow,s", "use the slow code path")
		("fast,f", "use the fast code path (default)")
		("optimized,o", "use the optimized code path")
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

	if(vm.count("radius")) {
		radius = vm["radius"].as<int>();
	}

	if(vm.count("slow")) {
		methods.push_back(fun_struct(slow_mean, "naive_"));
	}

	if(vm.count("optimized")) {
		methods.push_back(fun_struct(real_fast_mean, "optimized_"));
	}

	if(methods.empty() || vm.count("fast")) {
		methods.push_back(fun_struct(fast_mean, "integral_"));
	}

	if (vm.count("input"))
	{
		QElapsedTimer timer;

		BOOST_FOREACH(const string& file, vm["input"].as< vector<string> >()) {
			cout << "Processing " << file << " with radius " << radius << endl;

			foreach(const fun_struct& method, methods) {
				QImage img(file.c_str());

				timer.start();

				method.fun(img, radius);

				cout << "Processing took " << (timer.elapsed() / 1000.) << " seconds" << endl;

				string out_name(method.prefix + file);
				img.save(out_name.c_str());
				cout << "Output written to " << out_name << endl;
			}
		}
	} else {
		cout << "No input files specified." << endl << "Hint: Try '-h' for help." << endl;
	}

	return 0;
}
