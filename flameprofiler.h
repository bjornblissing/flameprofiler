/* MIT License
 *
 * Copyright (c) 2018 Björn Blissing
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _FLAMEPROFILER_H_
#define _FLAMEPROFILER_H_

#ifndef USE_PROFILER
// Expand to nothing if profiling is disabled
#define PZone(name)
#define PZoneCat( name, category )
#define PMetadata( title, value )
#else
// Macro concatenation
#define CONCAT_IMPL( x, y ) x##y
#define MACRO_CONCAT( x, y ) CONCAT_IMPL( x, y )
// Macro names
#define PZone( name ) Profiler::Zone MACRO_CONCAT( profileZone, __COUNTER__ )( name )
#define PZoneCat( name, category ) Profiler::Zone MACRO_CONCAT( profileZone, __COUNTER__ )( name , category )
#define PMetadata( title, value ) Profiler::FlameGraphWriter::instance().addMetadata( title, value )

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <limits>
#include <thread>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Profiler {

	struct TracePoint {
		char name[64];
		char category[40];
		uint64_t timeStart;
		uint64_t timeEnd;
		uint32_t processId;
		uint32_t threadId;
	}; // 128 byte struct

	class FlameGraphWriter {
		public:
			~FlameGraphWriter()
			{
				// Find first stored profiler time
				uint64_t profilerTimerStart = std::numeric_limits<uint64_t>::max();

				if (!m_tracepoints.empty()) {
					for (std::vector<TracePoint>::const_iterator it = m_tracepoints.begin(); it != m_tracepoints.end(); ++it) {
						profilerTimerStart = std::min(profilerTimerStart, (*it).timeStart);
					}
				}
				else {
					profilerTimerStart = 0;
				}

				// Open file
				std::ofstream ofs;
				std::string filename = "profiler.json";
				ofs.open(filename, std::ofstream::out);

				// Write content
				if (ofs.is_open()) {
					// Header
					ofs << "{\n";
					ofs << "\t\"traceEvents\": ";
					// Begin trace events
					ofs << "[";
					// For each tracepoint write
					std::string separator = "";

					for (std::vector<TracePoint>::const_iterator it = m_tracepoints.begin(); it != m_tracepoints.end(); ++it) {
						ofs << separator << "\n";
						ofs << "\t\t{";
						ofs << " \"pid\":" << (*it).processId << ",";
						ofs << " \"tid\":" << (*it).threadId << ",";
						ofs << " \"ts\":" << (*it).timeStart - profilerTimerStart << ",";
						ofs << " \"dur\":" << (*it).timeEnd - (*it).timeStart << ",";
						ofs << " \"ph\":\"X\",";
						ofs << " \"name\":\"" << (*it).name << "\",";
						ofs << " \"cat\":\"" << (*it).category << "\"";
						ofs << "}";
						separator = ",";
					}

					ofs << "\n\t]";
					// End trace events

					// Begin metadata
					if (!m_metadata.empty()) {
						for (MetadataConstIterator it = m_metadata.begin(); it != m_metadata.end(); it++) {
							ofs << ",\n";
							ofs << "\t\"" << (*it).first << "\": \"" << (*it).second << "\"";
						}
					}

					// End metadata
					ofs << "\n}\n";
					// Close file
					ofs.close();
				}
			}

			static FlameGraphWriter& instance()
			{
				static FlameGraphWriter instance;
				return instance;
			}
			void addMetadata(const std::string& title, const std::string& value)
			{
				std::pair<std::string, std::string> metadata = std::make_pair(title, value);
				m_metadata.push_back(metadata);
			}
			void addTracePoint(const TracePoint& point) { m_tracepoints.push_back(point); }
		private:
			FlameGraphWriter() {};
			FlameGraphWriter(const FlameGraphWriter&); // No copy allowed
			FlameGraphWriter& operator= (const FlameGraphWriter&); // No assignment allowed

			std::string m_filename;
			std::vector<TracePoint> m_tracepoints;
			std::vector<std::pair<std::string, std::string>> m_metadata;
			typedef std::vector<std::pair<std::string, std::string>>::const_iterator MetadataConstIterator;
	};

	class Zone {
		public:
			Zone(const char* name, const char* category = "default")
			{
				snprintf(m_tracepoint.name, sizeof(m_tracepoint.name), "%s", name);
				snprintf(m_tracepoint.category, sizeof(m_tracepoint.category), "%s", category);
				size_t thread = std::hash<std::thread::id>()(std::this_thread::get_id());
				m_tracepoint.threadId = static_cast<uint32_t>(thread);
#ifdef _WIN32
				LARGE_INTEGER timer;
				QueryPerformanceCounter(&timer);
				convertToMicroSecond(timer);
				m_tracepoint.timeStart = timer.QuadPart;
				m_tracepoint.processId = static_cast<uint32_t>(GetCurrentProcessId());
#else
				struct timeval tv;
				struct timezone tz;
				::gettimeofday(&tv, &tz);
				m_tracepoint.timeStart = tv.tv_sec * TICKS_PER_SECOND + tv.tv_usec;
				m_tracepoint.processId = static_cast<uint32_t>(::getpid());
#endif
			}

			~Zone()
			{
#ifdef _WIN32
				LARGE_INTEGER timer;
				QueryPerformanceCounter(&timer);
				convertToMicroSecond(timer);
				m_tracepoint.timeEnd = timer.QuadPart;
#else
				struct timeval tv;
				struct timezone tz;
				::gettimeofday(&tv, &tz);
				m_tracepoint.timeEnd = tv.tv_sec * TICKS_PER_SECOND + tv.tv_usec;
#endif
				FlameGraphWriter::instance().addTracePoint(m_tracepoint);
			}
		private:
			TracePoint m_tracepoint;
#ifdef _WIN32
			static void convertToMicroSecond(LARGE_INTEGER& time)
			{
				LARGE_INTEGER Frequency;
				QueryPerformanceFrequency(&Frequency);
				// Convert to ms
				time.QuadPart *= 1000000;
				time.QuadPart /= Frequency.QuadPart;
			}
#else
			static const int64_t TICKS_PER_SECOND = 1000000L;
#endif
	};
} // namespace profiler

#endif // USE_PROFILER

#endif // !_FLAMEPROFILER_H_
