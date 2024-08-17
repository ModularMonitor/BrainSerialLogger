#include <iostream>
#include <filesystem>
#include <fstream>

#include "serial_port.h"

int main()
{
	std::cout << "Beginning scan..." << std::endl;

	while (1) {
		for (size_t p = 0; p < 120; ++p) {
			std::unique_ptr<SerialReader> dev;
			try {
				dev = std::unique_ptr<SerialReader>(new SerialReader(p));
			}
			catch (const SerialException& ex)
			{
				if (ex.get_type() != SerialException::type::CREATE_FILE) {
					std::cout << "Error on port #" << p << ": " << ex.get_message() << std::endl;
					continue;
				}
			}

			std::cout << "COM#" << p << "...   \r";

			bool got_format_beg = false;
			dev->set_callback([&](std::string line) {
				got_format_beg |= (line.size() > 3 && strncmp("!1$", line.c_str(), 3) == 0);
			});

			dev->put_string("help\n");

			for(size_t c = 0; c < 5 && !got_format_beg; ++c)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

			if (got_format_beg) {
				std::cout << "Nice! Connected to correct device!" << std::endl;
				std::cout << "Type +LOGWRITES at any time to show/hide this app activity." << std::endl;
				std::cout << "Anything else is redirected to the device." << std::endl;
			}
			else {
				//std::cout << "Wrong device, searching again..." << std::endl;
				continue;
			}

			//std::cout << "Connected to device on COM#" << p 
			//	<< ". Hit ENTER three times to exit / continue search. "
			//	"Send anything else to send to the device." << std::endl;

			std::mutex cout_mtx;
			bool show_write_logs = true;
			
			dev->set_callback([&] (std::string line){
				
				try {
					// possible format: &&&|1|0000000000321379|I2C|/dht/temperature,19.600000

					if (line.size() > 27 && strncmp("&&&", line.c_str(), 3) == 0) {
						const uint64_t time = std::strtoull(line.c_str() + 6, nullptr, 10);
						const std::string name = line.substr(23, 3);
						const std::string rest = line.substr(27);

						const auto p = rest.find(',');

						if (p == std::string::npos)
							throw std::runtime_error("Could not parse device input. Strange data.");

						auto path = rest.substr(0, p);
						const auto val = rest.substr(p + 1);

						std::for_each(path.begin(), path.end(), [](char& i) { if (i == '/' || i == '\\') i = '_'; });
						path += ".csv";

						const std::string real_path = "./" + name + "/" + path;

						std::filesystem::create_directory("./" + name);

						const bool file_did_exist = std::filesystem::exists(real_path);
						std::fstream fp(real_path, std::ios::app | std::ios::binary);

						if (!fp || fp.bad())
							throw std::runtime_error("Could not open file " + real_path + " to write data on.");

						if (!file_did_exist) {
							constexpr char begin_top[] = "time;computer_time;value\n";
							fp.write(begin_top, sizeof(begin_top) - 1);
						}

						const std::string final_write = std::to_string(time) + ";"
							+ std::to_string(std::chrono::duration_cast<std::chrono::duration<uint64_t, std::milli>>(std::chrono::system_clock::now().time_since_epoch()).count()) + ";"
							+ val + "\n";

						fp.write(final_write.c_str(), final_write.size());

						if (show_write_logs) {
							std::lock_guard<std::mutex> l(cout_mtx);
							std::cout << "W+ " << real_path << ": " << final_write;
						}
					}
					else {
						std::lock_guard<std::mutex> l(cout_mtx);
						std::cout << line << std::endl;
					}
				}
				catch (const std::exception& e) {
					std::cout << "Got exception: " << e.what() << std::endl;
				}
			});
			
			while (dev && *dev) {
				std::string input;
				std::getline(std::cin, input);
				
				if (input == "+LOGWRITES") {
					show_write_logs = !show_write_logs;
					std::cout << "[Local] Showing write logs is " << (show_write_logs ? "ENABLED" : "DISABLED") << std::endl;
				}
				else {
					dev->put_string(input);
					//std::lock_guard<std::mutex> l(cout_mtx);
					//std::cout << "SENT: " << input << std::endl;
				}
			
			}
		}
	}
}